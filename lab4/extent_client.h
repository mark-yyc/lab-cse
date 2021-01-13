// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "extent_server.h"

class extent_client {
 private:
  rpcc *cl;

  struct file_info {
    bool buf_dirty;
    bool attr_dirty;
    bool keep_writing;
    std::string buf;
    extent_protocol::attr attr;

    file_info(bool b_dirty, bool a_dirty, uint32_t type){
        buf_dirty=b_dirty;
        attr_dirty=a_dirty;
        keep_writing = false;
        attr.type = type;
        attr.size = 0;
        attr.atime = attr.mtime = attr.ctime = time(NULL);
    } 
    };

  std::map<extent_protocol::extentid_t, file_info *> file_cache;
  int rextent_port;
  std::string id;

 public:
  static int last_port;
  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &attr);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  rextent_protocol::status pull_handler(extent_protocol::extentid_t eid, int &);
  rextent_protocol::status push_handler(extent_protocol::extentid_t eid, int &);
};

#endif