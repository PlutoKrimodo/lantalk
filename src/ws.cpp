#include "ws.h"
#include <openssl/sha.h>    
#include<openssl/evp.h>

std::string compute_accept(const std::string & key){
    const std::string MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    std::string input=key+MAGIC;

    //20字节二进制
    unsigned char sha[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), sha);

    //Base64 长度约为 28 字节   
    unsigned char b64[64];
    int n= EVP_EncodeBlock(b64,sha,SHA_DIGEST_LENGTH);
    return std::string(reinterpret_cast<char*>(b64), n);
}