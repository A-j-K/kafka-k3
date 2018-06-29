
#include <map>
#include <string>
#include <cstdio>
#include <csignal>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include <jansson.h>

#include "s3.hpp"
#include "awsguard.hpp"
#include "iosguard.hpp"
#include "messagetime.hpp"
#include "messagewrapper.hpp"

static bool run_system;

typedef int64_t DiscoveryTimestamp;

struct DiscoveryItem
{
	DiscoveryItem() { memset(this, 0, sizeof(DiscoveryItem)); };
	~DiscoveryItem() {}
	DiscoveryItem(const DiscoveryItem &other) { memcpy(this, &other, sizeof(DiscoveryItem)); }
	int64_t			_timestamp;
	std::string		_s3key;
	char			_header[sizeof(K3::MessageHeader)];
};

typedef std::map<DiscoveryTimestamp, DiscoveryItem> DiscoveryMap;

/* Forward declarations */

static void sigterm(int sig);

static std::string cleanup_name(const std::string & inprefix, const std::string & in);

static void
s3_discovery(const std::string & intopic, const K3::MessageTime & infrom,
	K3::S3 & ins3client, std::vector<std::string> & output,
	int infetch = 10);

static void
batch_discovery(const std::string & intopic, const std::vector<std::string> & inkeys,
        K3::S3 & ins3client, DiscoveryMap & output);

static void
process_event(K3::S3 & ins3client, int64_t, DiscoveryItem & event_item);

int
main(int argc, char *argv[], char **envp)
{
	int rval = 1;
	json_t *paws = json_object();
	json_object_set_new(paws, "loglevel", json_string("off"));

	std::vector<std::string> vlist;
	DiscoveryMap vmap;

	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);

	K3::AwsGuard aws(paws);
	K3::S3 s3client;
	//K3::MessageTime from("2018-01-01-00-00-00.0");
	K3::MessageTime from("2018-06-28-12");
	s3client.setup(paws, envp);

	s3_discovery("rmm.entity/", from, s3client, vlist);
	std::cout << "S3 discovered S3 keys: " << vlist.size() << std::endl;

	batch_discovery("rmm.entity/", vlist, s3client, vmap);
	std::cout << "Batch discovered S3 keys: " << vmap.size() << std::endl;

	for(auto itor = vmap.begin(); itor != vmap.end(); itor++) {
		process_event(s3client, itor->first, itor->second);
	}

	json_decref(paws);
	std::cout << "Shutting down: " << rval << std::endl;
	return rval;
}

static void
process_event(K3::S3 & ins3client, int64_t index, DiscoveryItem & event_item)
{
		K3::IosGuard guard(std::cout);
		K3::MessageHeader *pheader = 
			reinterpret_cast<K3::MessageHeader*>(&event_item._header[0]);
		std::cout << "Index: " << index << std::endl;
		std::cout << "   timestamp  : " << event_item._timestamp << std::endl;
		std::cout << "   s3key      : " << event_item._s3key << std::endl;
		std::cout << "   kafka key  : " << pheader->key_field << std::endl;
		std::cout << "   kafka topic: " << pheader->topic_name << std::endl;
		std::cout << "   partition  : " << pheader->details.partition << std::endl;
		std::cout << "   offset     : " << pheader->details.offset << std::endl;
		std::cout << "   payload_len: " << pheader->details.payload_len << std::endl;
		std::cout << "   batch index: " << pheader->details.index << std::endl;
		std::cout << "   checksum   : 0x" << std::setfill('0') << std::setw(2) << std::hex << pheader->details.csum << std::endl;
}

static void
batch_discovery(const std::string & intopic, const std::vector<std::string> & inkeys,
	K3::S3 & ins3client, DiscoveryMap & output)
{
	for(auto itor = inkeys.begin(); itor != inkeys.end(); itor++) {
		K3::MessageHeader *pheader;
		Aws::Map<Aws::String, Aws::String> metadata;
		std::stringstream local_target;
		local_target << "/tmp/" << *itor;
		ins3client.get(*itor, local_target.str(), &metadata);
		std::ifstream file(local_target.str().c_str(), std::ios::binary | std::ios::ate);
		if(file.is_open()) {
			std::streamsize size = file.tellg();
			file.seekg(0, std::ios::beg);
			std::vector<char> buffer(size);
			file.read(buffer.data(), size);
			file.close();
			std::remove(local_target.str().c_str());
			pheader = reinterpret_cast<K3::MessageHeader*>(buffer.data());
			for(int nummsgs = pheader->details.index + 1; nummsgs; nummsgs--) {
				DiscoveryItem item;
				item._timestamp = pheader->details.timestamp;
				item._s3key = *itor;
				memcpy(&item._header[0], pheader, sizeof(K3::MessageHeader));
				output[item._timestamp] = item; // order by timestamp
				pheader = reinterpret_cast<K3::MessageHeader*>(
					buffer.data() + sizeof(K3::MessageHeader) + pheader->details.payload_len);
			}
		}
		else {
			std::cout << "Failed to open file: " << local_target.str() << std::endl;
		}
	}
}

static void
s3_discovery(const std::string & intopic, const K3::MessageTime & infrom,
	K3::S3 & ins3client, std::vector<std::string> & output,
	int infetch)
{
	std::string lastkey;
	while(true) {
		std::vector<std::string> vlist;
		if(lastkey.size() > 0) {
			if(!ins3client.list(intopic, vlist, infetch, &lastkey)) {
				return;
			}
		}
		else {
			if(!ins3client.list(intopic, vlist, infetch)) {
				return;
			}
		}
		if(vlist.size() < 1) {
			return;
		}
		for(auto itor = vlist.begin(); itor != vlist.end(); itor++) {
			std::string cleaned = cleanup_name(intopic, *itor);
			K3::MessageTime t(cleaned);
			if(t > infrom) {
				output.push_back(*itor);
			}
			lastkey = *itor;
		}
	}
	return;
}

static void
sigterm(int sig) 
{
	run_system = false;
}

static std::string
cleanup_name(const std::string & inprefix, const std::string & in)
{
	std::string rval = in.substr(inprefix.size());
	rval = rval.substr(0, rval.find("-rand"));
	return rval;
}

