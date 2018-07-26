
#include <map>
#include <string>
#include <cstdio>
#include <csignal>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include <sys/stat.h>

#include <jansson.h>

#include "s3.hpp"
#include "awsguard.hpp"
#include "iosguard.hpp"
#include "messagetime.hpp"
#include "messagewrapper.hpp"

static bool run_system;

typedef std::string DiscoveryTimestamp;

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
        K3::S3 & ins3client, DiscoveryMap & output, bool indelete);
static void
process_event(K3::S3 & ins3client, const std::string &, DiscoveryItem & event_item);

/* Main */

int
main(int argc, char *argv[], char **envp)
{
	int rval = 1;
	json_t *paws = json_object();
	json_object_set_new(paws, "loglevel", json_string("debug"));
	const char *ptopic;

	std::vector<std::string> vlist;
	DiscoveryMap vmap;

	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);

	K3::AwsGuard aws(paws);
	K3::S3 s3client;
	//K3::MessageTime from("2018-01-01-00-00-00.0");
	K3::MessageTime from("2018-07-20-12");
	s3client.setup(paws, envp);

	if((ptopic = std::getenv("AWS_TOPIC")) == NULL) {
		ptopic = "rmm.entity/";
	}
	
	std::stringstream dir;
	dir << "/tmp/" << ptopic;
	mkdir(dir.str().c_str(), 0x777);

	s3_discovery(ptopic, from, s3client, vlist);
	std::cout << "S3 discovered S3 keys: " << vlist.size() << std::endl;

	batch_discovery(ptopic, vlist, s3client, vmap, false);
	std::cout << "Batch discovered S3 keys: " << vmap.size() << std::endl;

	for(auto itor = vmap.begin(); itor != vmap.end(); itor++) {
		process_event(s3client, itor->first, itor->second);
	}

	json_decref(paws);
	std::cout << "Shutting down: " << rval << std::endl;
	return rval;
}

static void
process_event(K3::S3 & ins3client, const std::string & index, DiscoveryItem & event_item)
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
	K3::S3 & ins3client, DiscoveryMap & output, bool indelete)
{
	for(auto itor = inkeys.begin(); itor != inkeys.end(); itor++) {
		std::stringstream s3path;
		Aws::Map<Aws::String, Aws::String> metadata;
		std::stringstream local_target;
		local_target << "/tmp/" << *itor;
		ins3client.get(*itor, local_target.str(), &metadata);
		std::ifstream file(local_target.str().c_str(), std::ios::binary | std::ios::ate);
		if(file.is_open()) {
			std::streamsize size = file.tellg() * 2;
			file.seekg(0, std::ios::beg);
			std::vector<char> buffer(size);
			file.read(buffer.data(), size);
			file.close();
			if(size < sizeof(K3::MessageHeader)) {
				std::cout << "File " << local_target.str() << " incorrect size " << size << std::endl;
			}
			else {
				void *offset;
				char *p = (char*)buffer.data();
				K3::MessageHeader *pbase = (K3::MessageHeader*)buffer.data();
				for(int nummsgs = pbase->details.index + 1; nummsgs; nummsgs--) {
					K3::MessageHeader *pheader = (K3::MessageHeader*)p;
					std::cout << nummsgs << " " << *itor << " p: " << p << std::endl;
					if(pheader->details.timestamp) {
						std::stringstream oss;
						DiscoveryItem item;
						oss << pheader->details.timestamp << "." << pheader->details.offset;
						item._timestamp = pheader->details.timestamp;
						item._s3key = *itor;
						memcpy(&item._header[0], p, sizeof(K3::MessageHeader));
						output[oss.str()] = item; // order by timestamp
					}
					else {
						std::cout << "Warning, " << *itor << " has a zero timestamp." << std::endl;
					}
					p += (sizeof(K3::MessageHeader) + pheader->details.payload_len);
				}
			}
			if(indelete) {
				std::remove(local_target.str().c_str());
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

