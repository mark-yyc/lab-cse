#include "ydb_server_occ.h"
#include "extent_client.h"

using namespace std;

//#define DEBUG 1

ydb_server_occ::ydb_server_occ(std::string extent_dst, std::string lock_dst) : ydb_server(extent_dst, lock_dst) {
	current_transaction_id=0;
}

ydb_server_occ::~ydb_server_occ() {
}


ydb_protocol::status ydb_server_occ::transaction_begin(int, ydb_protocol::transaction_id &out_id) {    // the first arg is not used, it is just a hack to the rpc lib
	while(true){
		lc->acquire(0);
		if(read_set.find(current_transaction_id)==read_set.end()){
			map<extent_protocol::extentid_t,string>tmp;
			read_set.insert(pair<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >(current_transaction_id,tmp));
			write_set.insert(pair<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >(current_transaction_id,tmp));
			out_id=current_transaction_id;
			current_transaction_id++;
			break;
		}
		current_transaction_id++;
	}
	printf("transaction begin %d\n",out_id);
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::transaction_commit(ydb_protocol::transaction_id id, int &) {
	printf("transaction commit %d\n",id);
	lc->acquire(0);
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=read_set.find(id);
	if(it==read_set.end()){
		lc->release(0);
		printf("OCC get: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}
	for(map<extent_protocol::extentid_t,string>::iterator i=it->second.begin();i!=it->second.end();i++){
		string out_value;
		if(ec->get(i->first,out_value)!= extent_protocol::OK){
			lc->release(0);
			return ydb_protocol::RPCERR;
		}
		if(out_value!=i->second){
			read_set.erase(id);
			write_set.erase(id);
			lc->release(0);
			return ydb_protocol::ABORT;
		}
	}
	it=write_set.find(id);
	if(it==write_set.end()){
		lc->release(0);
		printf("OCC get: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}
	for(map<extent_protocol::extentid_t,string>::iterator i=it->second.begin();i!=it->second.end();i++){
		if(ec->put(i->first,i->second)!=extent_protocol::OK){
			lc->release(0);
			return ydb_protocol::RPCERR;
		}
	}
	read_set.erase(id);
	write_set.erase(id);
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::transaction_abort(ydb_protocol::transaction_id id, int &) {
	printf("transaction abort %d\n",id);
	lc->acquire(0);
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=read_set.find(id);
	if(it==read_set.end()){
		lc->release(0);
		printf("OCC get: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}
	it=write_set.find(id);
	if(it==write_set.end()){
		lc->release(0);
		printf("OCC get: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}
	read_set.erase(id);
	write_set.erase(id);
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value) {
	extent_protocol::extentid_t extentid=SDBMHash(key);
	printf("transaction %d: get %lld\n",id,extentid);

	lc->acquire(0);
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=read_set.find(id);
	if(it==read_set.end()){
		lc->release(0);
		printf("OCC get: %d not allocated in transaction_group \n",id);
		return ydb_protocol::TRANSIDINV;
	}

	map<extent_protocol::extentid_t,string>::iterator log=it->second.find(extentid);
	if(log==it->second.end()){
		if(ec->get(extentid,out_value)!= extent_protocol::OK){
			lc->release(0);
			return ydb_protocol::RPCERR;
		}
		if(out_value.size()==0){
			lc->release(0);
			printf("OCC get: key %lld del\n",extentid);
			return ydb_protocol::RPCERR;
		}
		it->second.insert(pair<extent_protocol::extentid_t,string>(extentid,out_value));
	}
	else{
		out_value=log->second;
	}

	it=write_set.find(id);
	if(it==write_set.end()){
		lc->release(0);
		printf("OCC set: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}

	log=it->second.find(extentid);
	if(log!=it->second.end()){
		out_value=log->second;
	}
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &) {
	extent_protocol::extentid_t extentid=SDBMHash(key);
	printf("transaction %d: set %lld\n",id,extentid);

	lc->acquire(0);
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=write_set.find(id);
	if(it==write_set.end()){
		lc->release(0);
		printf("OCC set: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}

	map<extent_protocol::extentid_t,string>::iterator log=it->second.find(extentid);
	if(log==it->second.end()){
		it->second.insert(pair<extent_protocol::extentid_t,string>(extentid,value));
	}
	else{
		log->second=value;
	}
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::del(ydb_protocol::transaction_id id, const std::string key, int &) {
	extent_protocol::extentid_t extentid=SDBMHash(key);
	printf("transaction %d: del %lld\n",id,extentid);

	lc->acquire(0);
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=write_set.find(id);
	if(it==write_set.end()){
		lc->release(0);
		printf("OCC del: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}

	map<extent_protocol::extentid_t,string>::iterator log=it->second.find(extentid);
	if(log==it->second.end()){
		it->second.insert(pair<extent_protocol::extentid_t,string>(extentid,""));
	}
	else{
		log->second="";
	}
	lc->release(0);
	return ydb_protocol::OK;
}

