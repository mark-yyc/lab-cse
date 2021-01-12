#include "ydb_server_2pl.h"
#include "extent_client.h"

using namespace std;
//#define DEBUG 1

ydb_server_2pl::ydb_server_2pl(std::string extent_dst, std::string lock_dst) : ydb_server(extent_dst, lock_dst) {
	current_transaction_id=-1;
}

ydb_server_2pl::~ydb_server_2pl() {
}

ydb_protocol::status ydb_server_2pl::transaction_begin(int, ydb_protocol::transaction_id &out_id) {    // the first arg is not used, it is just a hack to the rpc lib
	while(true){
		lc->acquire(0);
		if(transaction_group.find(current_transaction_id)==transaction_group.end()){
			map<extent_protocol::extentid_t,string>tmp;
			transaction_group.insert(pair<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >(current_transaction_id,tmp));
			out_id=current_transaction_id;
			node_group.insert(pair<ydb_protocol::transaction_id,ydb_protocol::transaction_id>(out_id,0));
			current_transaction_id--;
			break;
		}
		current_transaction_id--;
	}
	printf("transaction begin %d\n",out_id);
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::transaction_commit(ydb_protocol::transaction_id id, int &) {
	printf("transaction commit %d\n",id);
	lc->acquire(0);
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=transaction_group.find(id);
	if(it==transaction_group.end()){
		printf("2PL get: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}
	for(map<extent_protocol::extentid_t,string>::iterator i=it->second.begin();i!=it->second.end();i++){
		if(ec->put(i->first,i->second)!=extent_protocol::OK){
			lc->release(0);
			printf("2PL commit: %lld put error\n",i->first);
			return ydb_protocol::RPCERR;
		}
		map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator l_node=node_group.find(i->first);
		l_node->second=0;
		lc->release(i->first);
	}
	node_group.erase(id);
	transaction_group.erase(id);
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::transaction_abort(ydb_protocol::transaction_id id, int &) {
	printf("transaction abort %d\n",id);
	lc->acquire(0);
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=transaction_group.find(id);
	if(it==transaction_group.end()){
		printf("2PL get: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}
	for(map<extent_protocol::extentid_t,string>::iterator i=it->second.begin();i!=it->second.end();i++){
		map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator l_node=node_group.find(i->first);
		l_node->second=0;
		lc->release(i->first);
	}
	node_group.erase(id);
	transaction_group.erase(id);
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value) {
	extent_protocol::extentid_t extentid=SDBMHash(key);
	printf("transaction %d: get %lld\n",id,extentid);
	lc->acquire(0);
	map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator l_node=node_group.find(extentid);
	if(l_node==node_group.end()){
		node_group.insert(pair<ydb_protocol::transaction_id,ydb_protocol::transaction_id>(extentid,0));
		l_node=node_group.find(extentid);
	}

	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=transaction_group.find(id);
	if(it==transaction_group.end()){
		lc->release(0);
		printf("2PL get: %d not allocated in transaction_group \n",id);
		return ydb_protocol::TRANSIDINV;
	}

	map<extent_protocol::extentid_t,string>::iterator log=it->second.find(extentid);
	if(log==it->second.end()){
		map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator t_node=node_group.find(id);
		if(t_node==node_group.end()){
			lc->release(0);
			printf("2PL get: %d not allocated in node group\n",id);
			return ydb_protocol::TRANSIDINV;
		}
		t_node->second=extentid;
		if(check_cycle(id)){
			lc->release(0);
			int tmp;
			transaction_abort(id,tmp);
			return ydb_protocol :: ABORT;
		}
		lc->release(0);
		lc->acquire(extentid);
		lc->acquire(0);
		t_node->second=0;
		l_node->second=id;
		if(ec->get(extentid,out_value)!= extent_protocol::OK){
			lc->release(0);
			return ydb_protocol::RPCERR;
		}
		if(out_value.size()==0){
			lc->release(0);
			printf("2PL get: key %lld del\n",extentid);
			return ydb_protocol::RPCERR;
		}
		it->second.insert(pair<extent_protocol::extentid_t,string>(extentid,out_value));
	}
	else{
		out_value=log->second;
	}
	lc->release(0);
	return ydb_protocol::OK;
	
}

ydb_protocol::status ydb_server_2pl::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &) {
	extent_protocol::extentid_t extentid=SDBMHash(key);
	printf("transaction %d: set %lld\n",id,extentid);
	lc->acquire(0);
	map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator l_node=node_group.find(extentid);
	if(l_node==node_group.end()){
		node_group.insert(pair<ydb_protocol::transaction_id,ydb_protocol::transaction_id>(extentid,0));
		l_node=node_group.find(extentid);
	}

	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=transaction_group.find(id);
	if(it==transaction_group.end()){
		lc->release(0);
		printf("2PL set: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}

	map<extent_protocol::extentid_t,string>::iterator log=it->second.find(extentid);
	if(log==it->second.end()){
		map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator t_node=node_group.find(id);
		if(t_node==node_group.end()){
			lc->release(0);
			printf("2PL set: %d not allocated in node group\n",id);
			return ydb_protocol::TRANSIDINV;
		}
		t_node->second=extentid;
		if(check_cycle(id)){
			lc->release(0);
			int tmp;
			transaction_abort(id,tmp);
			return ydb_protocol :: ABORT;
		}
		lc->release(0);
		lc->acquire(extentid);
		lc->acquire(0);
		t_node->second=0;
		l_node->second=id;
		it->second.insert(pair<extent_protocol::extentid_t,string>(extentid,value));
	}
	else{
		log->second=value;
	}
	lc->release(0);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::del(ydb_protocol::transaction_id id, const std::string key, int &) {
	extent_protocol::extentid_t extentid=SDBMHash(key);
	printf("transaction %d: del %lld\n",id,extentid);
	lc->acquire(0);
	map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator l_node=node_group.find(extentid);
	if(l_node==node_group.end()){
		node_group.insert(pair<ydb_protocol::transaction_id,ydb_protocol::transaction_id>(extentid,0));
		l_node=node_group.find(extentid);
	}
	map<ydb_protocol::transaction_id,map<extent_protocol::extentid_t,string> >::iterator it=transaction_group.find(id);
	if(it==transaction_group.end()){
		lc->release(0);
		printf("2PL del: %d not allocated\n",id);
		return ydb_protocol::TRANSIDINV;
	}

	map<extent_protocol::extentid_t,string>::iterator log=it->second.find(extentid);
	if(log==it->second.end()){
		map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator t_node=node_group.find(id);
		if(t_node==node_group.end()){
			lc->release(0);
			printf("2PL del: %d not allocated in node group\n",id);
			return ydb_protocol::TRANSIDINV;
		}
		t_node->second=extentid;
		if(check_cycle(id)){
			int tmp;
			transaction_abort(id,tmp);
			return ydb_protocol :: ABORT;
		}
		lc->release(0);
		lc->acquire(extentid);
		lc->acquire(0);
		t_node->second=0;
		l_node->second=id;
		it->second.insert(pair<extent_protocol::extentid_t,string>(extentid,""));
	}
	else{
		log->second="";
	}
	lc->release(0);
	return ydb_protocol::OK;
}

bool ydb_server_2pl::check_cycle(ydb_protocol::transaction_id t_id){
	map<ydb_protocol::transaction_id,int>check;
	ydb_protocol::transaction_id id=t_id;
	while(true){
		map<ydb_protocol::transaction_id,ydb_protocol::transaction_id>::iterator node=node_group.find(id);
		if(node==node_group.end()){
			return false;
		}
		map<ydb_protocol::transaction_id,int>::iterator it=check.find(node->first);
		if(it!=check.end()){
			printf("transaction %d deadlock\n",t_id);
			return true;
		}
		check.insert(pair<ydb_protocol::transaction_id,int>(node->first,0));
		if(node->second==0){
			return false;
		}
		id=node->second;
	}
}


