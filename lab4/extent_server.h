// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <set>
#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;
  
  struct client_info {
  std::string current_writer;
  std::set<std::string> cached_cids;
};

  std::map<extent_protocol::extentid_t, client_info *> client_map;

  void notify(extent_protocol::extentid_t eid, std::string cl);
  void require(extent_protocol::extentid_t eid);

 public:
  extent_server();

  int create(std::string cl, uint32_t type, extent_protocol::extentid_t &id);
  int put(std::string cl, extent_protocol::extentid_t id, std::string, int &);
  int get(std::string cl, extent_protocol::extentid_t id, std::string &);
  int getattr(std::string cl, extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(std::string cl, extent_protocol::extentid_t id, int &);
};

#endif 







