
#include <map>
#include <string>
#include <cstdio>
#include <csignal>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include <ctype.h>
#include <sys/stat.h>

#include <jansson.h>

#include "s3.hpp"
#include "produce.hpp"
#include "awsguard.hpp"
#include "iosguard.hpp"
#include "messagetime.hpp"
#include "messagewrapper.hpp"

#define TMP_DIR "/tmp"

static bool run_system;

static bool debug_output = true;

typedef std::string DiscoveryTimestamp;

struct DiscoveryItem
{
	typedef std::shared_ptr<char> ShPtr;
	DiscoveryItem() { memset(this, 0, sizeof(DiscoveryItem)); };
	~DiscoveryItem() {}
	DiscoveryItem(const DiscoveryItem &other) { memcpy(this, &other, sizeof(DiscoveryItem)); }
	int64_t			_timestamp;
	std::string		_s3key;
	char			_header[sizeof(K3::MessageHeader)];
	ShPtr 			_payload;
	void setpayload(char *p, int l) {
		char *np = new char[l];
		_payload = ShPtr(np);
		memcpy(np, p, l);
	}
};

typedef std::map<DiscoveryTimestamp, DiscoveryItem> DiscoveryMap;

/* Forward declarations */

static void sigterm(int sig);
static std::string cleanup_name(const std::string & inprefix, const std::string & in);
static void
s3_discovery(const std::string & intopic, const K3::MessageTime & infrom,
	K3::S3 & ins3client, std::vector<std::string> & output,
	int infetch = 50);
static void
batch_discovery(const std::string & intopic, const std::vector<std::string> & inkeys,
        K3::S3 & ins3client, DiscoveryMap & output, bool indelete);
static void
process_event(K3::S3 & ins3client, const std::string &, DiscoveryItem & event_item, K3::Produce & prod);

/* Main */

int
main(int argc, char *argv[], char **envp)
{
	int rval = 1;
	char *from_time = NULL;
	json_t *paws = json_object();
	json_object_set_new(paws, "loglevel", json_string("debug"));
	const char *ptopic;

	std::vector<std::string> vlist;
	DiscoveryMap vmap;


	if((from_time = std::getenv("FROM_TIME")) == NULL) {
		printf("No from time defined.\n");
		return -1;
	}

	if((ptopic = std::getenv("AWS_TOPIC")) == NULL) {
		printf("S3 source topic supplied.\n");
	}
	
	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);
	K3::AwsGuard aws(paws);
	K3::S3 s3client;

	// Examples of time format
	//K3::MessageTime from("2018-01-01-00-00-00.0");
	//K3::MessageTime from("2018-08-22-09");
	K3::MessageTime from(from_time);
	s3client.setup(paws, envp);

	std::stringstream dir;
	dir << "/tmp/" << ptopic;
	mkdir(dir.str().c_str(), 0x777);

	s3_discovery(ptopic, from, s3client, vlist);
	std::cout << "S3 discovered S3 keys: " << vlist.size() << std::endl;

	batch_discovery(ptopic, vlist, s3client, vmap, false);
	std::cout << "Batch discovered S3 keys: " << vmap.size() << std::endl;

	K3::Produce p(envp);
	p.setup();

	for(auto itor = vmap.begin(); itor != vmap.end(); itor++) {
		process_event(s3client, itor->first, itor->second, p);
	}

	json_decref(paws);
	std::cout << "Shutting down: " << rval << std::endl;
	return rval;
}

static std::string
hexdump_line(char *p, int l) 
{
	std::stringstream rval;
	rval << std::setw(2) << std::hex << "    ";
	for(int i = 0; i < 16 && i < l; ++i) {
		unsigned c = (unsigned)((*(p+i)) & 0xFF);
		rval.width(2);
		rval.fill('0');
		rval << std::hex << std::uppercase << c << " ";
		if(((i+1)%8) == 0) rval << " ";
	}
	rval << " :: ";
	for(int i = 0; i < 16 && i < l; ++i) {
		char c = *(p+i);
		if(!isprint(c)) c = '.';
		rval << c;	
	}
	return rval.str();
}

static void
process_event(K3::S3 & ins3client, const std::string & index, DiscoveryItem & event_item, K3::Produce & p)
{
		K3::MessageHeader *pheader = 
			reinterpret_cast<K3::MessageHeader*>(&event_item._header[0]);
		if(debug_output) {
			K3::IosGuard guard(std::cout);
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
			std::cout << "   payload    : ";

			if(pheader->details.payload_len > 0 && event_item._payload.get() != NULL) {
				int offset = 0;
				while(offset < pheader->details.payload_len) {
					int i = (offset+16 > pheader->details.payload_len) ? pheader->details.payload_len-offset : 16;
					std::string s = hexdump_line(event_item._payload.get()+offset, i);	
					std::cout << std::endl << s;
					offset += i;
				}
				std::cout << std::endl;
				std::cout << "Producing event to topic" << std::endl;
				p.produce(
					event_item._payload.get(),
					pheader->details.payload_len,
					pheader->key_field,
					strlen(pheader->key_field)
				);
			}
			else {
				std::cout << "NULL" << std::endl;
			}
		}
}

static void
batch_discovery_bykey(const std::string & inkey, const std::string & intopic,
	K3::S3 & ins3client, DiscoveryMap & output, bool indelete)
{
	std::stringstream s3path;
	Aws::Map<Aws::String, Aws::String> metadata;
	std::stringstream local_target;
	local_target << TMP_DIR << "/" << inkey;
	ins3client.get(inkey, local_target.str(), &metadata);
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
				std::cout << nummsgs << " " << inkey << std::endl;
				if(pheader->details.timestamp) {
					std::stringstream oss;
					DiscoveryItem item;
					oss << pheader->details.timestamp << "." << pheader->details.offset;
					item._timestamp = pheader->details.timestamp;
					item._s3key = inkey;
					memcpy(&item._header[0], p, sizeof(K3::MessageHeader));
					if(pheader->details.payload_len) {
						char *p1 = (char*)buffer.data();
						char *payload = p1 + sizeof(K3::MessageHeader); 
						item.setpayload(payload, pheader->details.payload_len);
					}
					output[oss.str()] = item; // order by timestamp
				}
				else {
					std::cout << "Warning, " << inkey << " has a zero timestamp." << std::endl;
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

static void
batch_discovery(const std::string & intopic, const std::vector<std::string> & inkeys,
	K3::S3 & ins3client, DiscoveryMap & output, bool indelete)
{
	for(auto itor = inkeys.begin(); itor != inkeys.end(); itor++) {
		std::string key = *itor;
		batch_discovery_bykey(key, intopic, ins3client, output, indelete);
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
				std::cout << "Discovered " << *itor << " and matched time window" << std::endl;
				output.push_back(*itor);
			}
			else {
				std::cout << "Discovered " << *itor << " but didn't meet specified time window" << std::endl;
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

