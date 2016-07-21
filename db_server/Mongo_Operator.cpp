/*
 * Mongo_Operator.cpp
 *
 *  Created on: Dec 29, 2015
 *      Author: zhangyalei
 */

#include "Msg_Define.h"
#include "Mongo_Operator.h"
#include "Server_Config.h"
#include "DB_Manager.h"

Mongo_Operator::Mongo_Operator(void) { }

Mongo_Operator::~Mongo_Operator(void) { }

Mongo_Operator *Mongo_Operator::instance_;

Mongo_Operator *Mongo_Operator::instance(void) {
	if (instance_ == 0)
		instance_ = new Mongo_Operator;
	return instance_;
}

DBClientConnection &Mongo_Operator::connection(void) {
	GUARD(MUTEX, mon, connection_map_lock_);

	const int64_t thread_key = pthread_self();
	DBClientConnection *conn = 0;
	Connection_Map::iterator it = connection_map_.find(thread_key);
	if (it == connection_map_.end()) {
		conn = connection_pool_.pop();

		std::string err_msg;
		std::stringstream host_stream;
		const Json::Value &mongodb_game = SERVER_CONFIG->server_misc()["mongodb_game"];
		if (mongodb_game == Json::Value::null) {
			LOG_FATAL("server_misc config cannot find mongodb_game");
		}
		host_stream << (mongodb_game["ip"].asString());
		if (mongodb_game.isMember("port")) {
			host_stream << ":" << (mongodb_game["port"].asInt());
		}
		conn->connect(host_stream.str().c_str(), err_msg);
		connection_map_[thread_key] = conn;
	} else {
		conn = it->second;
	}

  return *conn;
}

int Mongo_Operator::init(void) {
	/// 创建所有索引
	this->create_index();
	return 0;
}

int Mongo_Operator::create_index(void) {
	{//global
		MONGO_CONNECTION.createIndex("mmo.global", BSON("key" << 1));

		if (MONGO_CONNECTION.count("mmo.global", BSON("key" << "role")) == 0) {
			BSONObjBuilder builder;
			builder << "key" << "role" << "id" << 0;

			MONGO_CONNECTION.update("mmo.global", MONGO_QUERY("key" << "role"),
					BSON("$set" << builder.obj()), true);
		}
	}
	{
		//role
		MONGO_CONNECTION.createIndex("mmo.role", BSON("role_id" << 1));
		MONGO_CONNECTION.createIndex("mmo.role", BSON("role_name" << 1));
		MONGO_CONNECTION.createIndex("mmo.role", BSON("account" << 1));

		MONGO_CONNECTION.createIndex("mmo.role", BSON("account" << 1 << "agent_num" << 1 << "server_num" << 1));
		MONGO_CONNECTION.createIndex("mmo.role", BSON("account" << 1 << "level" << -1));
	}
	{
		for(DB_Manager::DB_Struct_Id_Map::iterator iter = DB_MANAGER->db_struct_id_map().begin();
				iter != DB_MANAGER->db_struct_id_map().end(); iter++){
			MONGO_CONNECTION.createIndex(iter->second->db_name(), BSON(iter->second->index() << 1));
		}
	}
	return 0;
}

int Mongo_Operator::load_db_cache(int cid) {
	unsigned long long res_count = MONGO_CONNECTION.count("mmo.role");
	BSONObjBuilder field_builder;
	field_builder << "role_id" << 1
					<< "account" << 1
					<< "role_name" << 1
					<< "agent_num" << 1
					<< "server_num" << 1
					<< "level" << 1
					<< "gender" << 1
					<< "career" << 1
					<< "last_sign_out_time" << 1;

	BSONObj field_bson = field_builder.obj();
	std::vector<BSONObj> iter_record;
	iter_record.reserve(res_count);

	MONGO_CONNECTION.findN(iter_record, "mmo.role", mongo::Query(), res_count, 0, &field_bson);

	MSG_550000 msg;
	for (std::vector<BSONObj>::iterator iter = iter_record.begin(); iter != iter_record.end(); ++iter) {
		BSONObj obj = *iter;

		Player_DB_Cache db_cache;
		db_cache.role_id = obj["role_id"].numberLong();
		db_cache.account = obj["account"].valuestrsafe();
		db_cache.role_name = obj["role_name"].valuestrsafe();
		db_cache.agent_num = obj["agent_num"].numberInt();
		db_cache.server_num = obj["server_num"].numberInt();
		db_cache.level = obj["level"].numberInt();
		db_cache.gender = obj["gender"].numberInt();
		db_cache.career = obj["career"].numberInt();
		msg.db_cache_vec.push_back(db_cache);
	}

	Block_Buffer buf;
	buf.make_inner_message(SYNC_DB_GAME_LOAD_DB_CACHE);
	msg.serialize(buf);
	buf.finish_message();
	DB_MANAGER->send_data_block(cid, buf);
	return 0;
}

int64_t Mongo_Operator::create_init_player(Game_Player_Info &player_info) {
	BSONObj fields = BSON("account" << 1 << "role_id" << 1 << "role_name" << 1);
	BSONObj res = MONGO_CONNECTION.findOne("mmo.role",
			MONGO_QUERY("role_name" << player_info.role_name), &fields);

	if (!res.isEmpty()) {
		LOG_ERROR("role_name = %s has existed.", player_info.role_name.c_str());
		return -1;
	}

	BSONObj cmd = fromjson("{findandmodify:'global', query:{key:'role'}, update:{$inc:{id:1}}}");
	if (MONGO_CONNECTION.runCommand("mmo", cmd, res) == false) {
		LOG_ERROR("increase role key failed.");
		return -1;
	}

	const Json::Value &server_misc = SERVER_CONFIG->server_misc();
	if (server_misc == Json::Value::null) {
		LOG_ERROR("server_misc is null");
		return -1;
	}
	int agent_num = server_misc["agent_num"].asInt();
	int server_num = server_misc["server_num"].asInt();
	if (agent_num < 1) agent_num = 1;
	if (server_num < 1) server_num = 1;

	int order = res.getFieldDotted("value.id").numberLong() + 1;
	int64_t agent = agent_num * 10000000000000L;
	int64_t server = server_num * 1000000000L;
	int64_t role_id = agent + server + order;

	player_info.role_id = role_id;
	player_info.agent_num = agent_num;
	player_info.server_num = server_num;
	int now_sec = Time_Value::gettimeofday().sec();
	MONGO_CONNECTION.update("mmo.role", MONGO_QUERY("role_id" << ((long long int)role_id)), BSON("$set" <<
			BSON("role_id" << (long long int)role_id
					<< "agent_num" << player_info.agent_num
					<< "server_num" << player_info.server_num
					<< "account" << player_info.account
					<< "role_name" << player_info.role_name
					<< "level" << 1
					<< "gender" << player_info.gender
					<< "career" << 0
					<< "last_sign_in_time" << now_sec
					<< "last_sign_out_time" << now_sec)), true);
	return role_id;
}

bool Mongo_Operator::role_exist(std::string &account) {
	BSONObj res = MONGO_CONNECTION.findOne("mmo.role", MONGO_QUERY("account" << account));
	if (res.isEmpty()) {
		return false;
	} else {
		return true;
	}
}

int64_t Mongo_Operator::get_role_id(const std::string &account) {
	BSONObj res = MONGO_CONNECTION.findOne("mmo.role", MONGO_QUERY("account" << account).sort(BSON("level" << -1)));
	if (res.isEmpty()) {
		return 0;
	} else {
		return res["role_id"].numberLong();
	}
}
