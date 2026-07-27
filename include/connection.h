#include<string>
#include<map>

enum class ParseState{
    REQUEST_LINE,
    HEADERS,
    BODY,
    DONE
};

struct Conn{
    int fd=-1;
    std::string rbuf; // 读缓冲区
    ParseState state=ParseState::REQUEST_LINE;
    std::string method, path, version;
    std::map<std::string ,std::string > headers;
    std::string body;
};