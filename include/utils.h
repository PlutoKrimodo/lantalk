#pragma once
#include <string>

std::string random_hex(int nbytes);
std::string sha256_hex(const std::string& s);
std::string now_hhmm();