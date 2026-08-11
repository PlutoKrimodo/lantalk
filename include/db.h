#pragma once

#include <mysql/mysql.h>
#include <string>
#include <cstdio>

class Db{
public:
    Db() = default;
    ~Db() {
        if(conn_){
            mysql_close(conn_);
        }
    }

    bool connect(const std::string & user,
                const std::string& pass,
                const std::string& dbname,
                const std::string& host = "localhost",
                unsigned int port = 0){
            conn_=mysql_init(nullptr);
            if(!conn_){
                fprintf(stderr,"mysql_init failed\n");
                return false;
            }

            if(!mysql_real_connect(conn_,host.c_str(),user.c_str(),
        pass.c_str(),dbname.c_str(),port,nullptr,0)){
            fprintf(stderr,"db connect failed: %s\n",mysql_error(conn_));
            return false;
        }

        //设置字符集
        if(mysql_set_character_set(conn_,"utf8mb4")){
            fprintf(stderr,"mysql_set_character_set failed: %s\n",mysql_error(conn_));
            return false;
        }
        return true;
    }

    MYSQL* raw() {return conn_;}
private:
    MYSQL* conn_ = nullptr;
};