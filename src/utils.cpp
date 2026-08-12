#include "utils.h"
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>

std::string random_hex(int nbytes){
    std::ifstream urandom("/dev/urandom",std::ios::binary);
    if(!urandom){
        throw std::runtime_error("Failed to open /dev/urandom\n");
    }
    unsigned char buffer[256];
    if(nbytes>256) nbytes=256;
    urandom.read(reinterpret_cast<char*>(buffer),nbytes);
    if(!urandom){
        throw std::runtime_error("Failed to open /dev/urandom\n");
    }
    std::stringstream ss;
    ss<<std::hex<<std::setfill('0');
    for(int i=0;i<nbytes;++i){
        ss<<std::setw(2)<<static_cast<int>(buffer[i]);
    }
    return ss.str();
}

std::string sha256_hex(const std::string& s){
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),hash);
    char buf[65];
    for(int i=0;i<32;++i){
        sprintf(buf+2*i,"%02x",hash[i]);
    }
    return std::string(buf,64);
}
    