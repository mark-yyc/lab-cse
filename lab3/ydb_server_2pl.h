#ifndef ydb_server_2pl_h
#define ydb_server_2pl_h

#include <string>
#include <map>
#include <vector>
// #include <pthread.h>

#include "extent_client.h"
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "ydb_protocol.h"
#include "ydb_server.h"


class ydb_server_2pl: public ydb_server {
private:
	// pthread_mutex_t mutex;
	ydb_protocol::transaction_id current_transaction_id;
	std::map<ydb_protocol::transaction_id,std::map<extent_protocol::extentid_t,std::string> > transaction_group;
	std::map<ydb_protocol::transaction_id,ydb_protocol::transaction_id> node_group; 

	bool check_cycle(ydb_protocol::transaction_id id);
public:
	ydb_server_2pl(std::string, std::string);
	~ydb_server_2pl();
	ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
	ydb_protocol::status transaction_commit(ydb_protocol::transaction_id, int &);
	ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
	ydb_protocol::status get(ydb_protocol::transaction_id, const std::string, std::string &);
	ydb_protocol::status set(ydb_protocol::transaction_id, const std::string, const std::string, int &);
	ydb_protocol::status del(ydb_protocol::transaction_id, const std::string, int &);
};

#endif

