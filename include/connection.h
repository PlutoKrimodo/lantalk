#include<string>
#include<map>

enum class ParseState{
    REQUEST_LINE,
    HEADERS,
    BODY,
    DONE
};

enum class Proto{
    HTTP,
    WS
};

struct Conn{
    int fd=-1;
    std::string rbuf; // 读缓冲区
    ParseState state=ParseState::REQUEST_LINE;
    Proto proto=Proto::HTTP;
    std::string method, path, version;
    std::map<std::string ,std::string > headers;
    std::string body;
    int content_length=0;

    //身份字段
    int uid=0;
    std::string username;
    Conn():fd(-1),state(ParseState::REQUEST_LINE){}
};