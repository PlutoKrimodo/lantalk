#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <cassert>
#include <cstdint>

#include "connection.h"
#include "ws.h"



constexpr int MAX_EVENTS=64;
std::unordered_map<int, Conn> conns;
//fd转换为非阻塞模式
void set_nonblock(int fd){
    int flags=fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flags|O_NONBLOCK);
}

//辅助清理函数
void close_connection(int epfd, int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    conns.erase(fd);  // 从全局 map 移除
}

std::string get_content_type(const std::string& path){
    if(path.size()>=5&&path.substr(path.size()-5)==".html"){
        return "text/html; charset=utf-8";
    }
    if(path.size()>=4&&path.substr(path.size()-4)==".css"){
        return "text/css; charset=utf-8";
    }
    if(path.size()>=3&&path.substr(path.size()-3)==".js"){
        return "application/javascript; charset=utf-8";
    }
    if(path.size()>=4&&path.substr(path.size()-4)==".ico"){
        return "image/x-icon";
    }
    return "text/plain";
}

std::string make_response(int code, const std::string& reason,const std::string & ctype
,const std::string & body){
    std::string resp="HTTP/1.1 "+std::to_string(code)+" "+reason+"\r\n";
    resp+="Content-Type: " + ctype+"\r\n";
    resp+="Content-Length: "+std::to_string(body.size())+"\r\n";
    resp+="Connection: close\r\n";
    resp+="\r\n";
    resp+=body;
    return resp;
}

void parse(Conn&conn, int epfd){
    printf("fd=%d rbuf(%zu bytes):",conn.fd,conn.rbuf.size());

    for(char c:conn.rbuf){
        if(c=='\r'){
            printf("\\r");
        }else if(c=='\n'){
            printf("\\n\n");
        }else{
            putchar(c); 
        }
    }
    printf("\n---\n");
    
    //REQUEST_LINE
    if(conn.state==ParseState::REQUEST_LINE){
        size_t pos=conn.rbuf.find("\r\n");
        if(pos==std::string::npos){
            return; //请求行不完整，继续等待下次read
        }
        std::string line=conn.rbuf.substr(0,pos);
        conn.rbuf.erase(0,pos+2); //删除请求行和\r\n

        std::string method, path, version;
        size_t p1=line.find(' ');
        if(p1==std::string::npos){
            close_connection(epfd, conn.fd); // 错误的请求行，关闭连接
            return;
        }
        size_t p2=line.find(' ',p1+1);
        if(p2==std::string::npos){
            close_connection(epfd, conn.fd);
            return; 
        }
        method=line.substr(0,p1);
        path=line.substr(p1+1,p2-p1-1);
        version=line.substr(p2+1);

        printf("method=%s path=%s version=%s\n",method.c_str(),path.c_str(),version.c_str());

        conn.method = method;
        conn.path = path;
        conn.version = version;

        if(method!="GET"&&method !="POST"){
            const char * resp501=
            "HTTP/1.1 501 Not Implemented\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";
            write(conn.fd,resp501,strlen(resp501));
            close_connection(epfd,conn.fd);
            return;
        }
        conn.state=ParseState::HEADERS;
    }

    //HEADERS
    if(conn.state==ParseState::HEADERS){
        while(true){
            size_t pos=conn.rbuf.find("\r\n");
            if(pos==std::string::npos) return;
            if(pos==0){
                conn.rbuf.erase(0,2);
                if(conn.method=="GET"){
                    conn.state=ParseState::DONE;
                }else if(conn.method=="POST"){
                    auto it =conn.headers.find("content-length");
                    if(it!=conn.headers.end()){
                        //非法，返回400
                        char * end;
                        long len=strtol(it->second.c_str(),&end,10);
                        if(*end!='\0'||len<0){
                            const char * resp400=
                            "HTTP/1.1 400 Bad Request\r\n"
                            "Content-Length: 0\r\n"
                            "Connection: close\r\n"
                            "\r\n";

                            write(conn.fd,resp400,strlen(resp400));
                            close_connection(epfd,conn.fd);
                            return;
                        }
                        conn.content_length=len;
                        if(len>0){
                            conn.state=ParseState::BODY;
                        }else{
                            conn.state=ParseState::DONE;
                        }
                    }else{
                        //没有content-length，视为0
                        conn.state=ParseState::DONE;
                    }
                }
                //验证，打印所有头
                printf("=== Headers ===\n");
                for (const auto& [k, v] : conn.headers) {
                    printf("%s: %s\n", k.c_str(), v.c_str());
                }
                printf("===============\n");
                break;
            }

            std::string line=conn.rbuf.substr(0,pos);
            conn.rbuf.erase(0,pos+2);
            std::string key,value;
            size_t p=line.find(':');
            if(p==std::string::npos){
                // 畸形行：返回 400 并关闭
                const char* resp400 = 
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                write(conn.fd, resp400, strlen(resp400));
                close_connection(epfd, conn.fd); // 错误的请求头，关闭连接
                return;
            }

            key=line.substr(0,p);
            //转小写
            std::transform(key.begin(),key.end(),key.begin(),
            [](unsigned char c){return std::tolower(c);});

            value=line.substr(p+1);
            //去掉value前后的空格
            size_t start=value.find_first_not_of(" \t");
            size_t end=value.find_last_not_of(" \t");
            if(start==std::string::npos||end==std::string::npos){
                value="";
            }else{
                value=value.substr(start,end-start+1);
            }
            conn.headers[key]=value;
        }
    }

    //BODY
    if(conn.state==ParseState::BODY){
        size_t need=conn.content_length;
        // 超过 1MB 直接断开
        if (need > 1024 * 1024) {
            const char* resp400 = 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n";
            write(conn.fd, resp400, strlen(resp400));
            close_connection(epfd, conn.fd);
            return;
        }

        //没攒够，等下次read
        if(conn.rbuf.size()<need)return;
        conn.body=conn.rbuf.substr(0,need);
        conn.rbuf.erase(0,need);
        conn.state=ParseState::DONE;
    }
}

//服务端发送WebSocket文本帧
void send_ws_text(int fd, const std::string&msg){
    std::string frame;
    frame.push_back(static_cast<char>(0x81));
    size_t n= msg.size();
    if(n<=125){
        frame.push_back(static_cast<char>(n));
    }else if(n<=65535){
        frame.push_back(static_cast<char>(126));
        uint16_t nl=htons(static_cast<uint16_t>(n));
        frame.append(reinterpret_cast<const char*>(&nl),2);
    }else{
        //大于65535，无法处理
        return;
    }
    frame+=msg;

    const char*data=frame.data();
    size_t remaining =frame.size();
    while(remaining>0){
        ssize_t w=write(fd,data,remaining);
        if(w<=0){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                return;
            }
            //出错
            return;
        }
        data += w;
        remaining -= w;
    }
}

void parse_ws_frame(Conn&conn, int epfd){
    while(true){
        if(conn.rbuf.size()<2) return;

        uint8_t b0 = static_cast<uint8_t> (conn.rbuf[0]);
        uint8_t b1 = static_cast<uint8_t> (conn.rbuf[1]);

        bool fin = b0& 0x80;
        (void) fin; // 目前不处理分片帧，暂时忽略 fin 标志
        uint8_t opcode = b0& 0x0F;
        bool masked = b1 & 0x80;
        uint8_t len = b1 & 0x7F;

        if(!masked){
            fprintf(stderr,"Client frame must be masked\n");
            close_connection(epfd,conn.fd);
            return;
        }

        //处理扩展长度
        uint64_t payload_len=len;
        size_t header_len =2;   //基本头
        if(len==126){
            if(conn.rbuf.size()<4)return;
            uint16_t ext_len;
            memcpy(&ext_len,conn.rbuf.data()+2,2);
            payload_len=ntohs(ext_len);
            header_len+=2;
        }else if(len==127){
            //不支持64位长度
            std::string close_frame;
            close_frame.push_back(static_cast<char>(0x88));
            close_frame.push_back(static_cast<char>(0x00));
            write(conn.fd,close_frame.data(),close_frame.size());
            close_connection(epfd,conn.fd);
            return;
        }
        //拒绝过大帧
        if(payload_len>1024*1024){
            close_connection(epfd,conn.fd);
            return;
        }
        
        size_t key_start=2+(len==126?2:0);  //注意，没有处理127
        size_t total_header=key_start+4;
        size_t total_len = total_header+payload_len;

        if(conn.rbuf.size()<total_len){
            return;
        }

        //提取掩码key
        uint8_t mask_key[4];
        for(int i=0;i<4;++i){
            mask_key[i]=static_cast<uint8_t>(conn.rbuf[key_start+i]);
        }

        //提取payload
        size_t payload_start = key_start+4;
        std::string payload = conn.rbuf.substr(payload_start,payload_len);

        //解码
        for(size_t i=0;i<payload_len;++i){
            payload[i] ^= mask_key[i%4];
        }

        conn.rbuf.erase(0,total_len);

        if(opcode==0x9||opcode==0xA){
            printf("Ignored ping/pong frame\n");
            continue;
        }else if(opcode==0x8){
            std::string close_frame;
            close_frame.push_back(static_cast<char>(0x88));
            close_frame.push_back(static_cast<char>(0x00));
            write(conn.fd,close_frame.data(),close_frame.size());
            close_connection(epfd,conn.fd);
            return;
        }else if(opcode==0x1){
            printf("Received text (payload_len=%zu): %.*s\n"
                , payload_len, (int)payload_len, payload.data());
            //原样发回
            send_ws_text(conn.fd,payload);
        }else{
            fprintf(stderr,"Unsupported opcode: %d\n",opcode);
            close_connection(epfd,conn.fd);
            return;
        }
    }
}

void handle_get(Conn& conn, int epfd) {
    if(conn.state!=ParseState::DONE||conn.method!="GET")return;

    std::string path=conn.path;
    //默认首页
    if(path=="/"){
        path="/index.html";
    }

    //路径中不得包含".."
    if(path.find("..")!=std::string::npos){
        std::string resp=make_response(404,"Not Found",
        "text/plain","");
        write(conn.fd,resp.c_str(),resp.size());
        close_connection(epfd,conn.fd);
        return;
    }

    //构造实际文件路径
    std::string filepath="web"+path;

    //尝试以二进制模式打开文件
    std::ifstream file(filepath, std::ios::binary);
    if(!file.is_open()){
        std::string resp=make_response(404,"Not Found",
        "text/plain","");
        write(conn.fd,resp.c_str(),resp.size());
        close_connection(epfd,conn.fd);
        return;
    }

    //读取文件
    std::string body((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    std::string ctype=get_content_type(path);
    std::string resp=make_response(200,"OK",ctype,body);
    write(conn.fd,resp.c_str(),resp.size());
    close_connection(epfd,conn.fd);
}

void handle_post(Conn& conn, int epfd) {
    if(conn.state!=ParseState::DONE||conn.method!="POST")return;

    if(conn.path=="/register"){
        std::cout<<"=== POST /register ===\n";
        std::cout<<"body: "<<conn.body<<"\n";

        std::string resp=make_response(200,"OK","text/plain","registered (fake)");
        write(conn.fd,resp.c_str(),resp.size());
        close_connection(epfd,conn.fd);
    }else{
        std::string resp = make_response(404, "Not Found", "text/plain", "");
        write(conn.fd, resp.c_str(), resp.size());
        close_connection(epfd, conn.fd);
    }
}

void handle_accept(int epfd,int listen_fd){
    while(true){
        sockaddr_in cli_addr;
        socklen_t cli_len=sizeof(cli_addr);
        int conn_fd=accept(listen_fd,(sockaddr*)&cli_addr,&cli_len);
        if(conn_fd<0){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                //没有新连接了，正常退出
                break;
            }
            if(errno == ECONNABORTED){
                continue; // 客户端在连接过程中断开，继续等待下一个连接
            }
            //其他错误
            fprintf(stderr,"accept: %s\n",strerror(errno));
            break;
        }
                
        printf("client connected: %s:%d fd=%d\n",
        inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port),conn_fd);

        //conn_fd设置为非阻塞模式
        set_nonblock(conn_fd);  

        conns[conn_fd].fd = conn_fd;

        epoll_event cev;
        //边缘触发模式
        cev.events=EPOLLIN|EPOLLET;
        cev.data.fd=conn_fd;
        epoll_ctl(epfd,EPOLL_CTL_ADD,conn_fd,&cev);
    }
}

void handle_read(int epfd,int fd){
    auto it =conns.find(fd);
    if(it==conns.end()){
        return ;
    }
    Conn& conn=it->second;  
    //某个客人的专线就绪，有数据可读
    char buf[1024];
    bool has_read=false; // 标记是否读到了数据，用于决定是否调用 parse
    //使用循环读取，直到读完所有数据
    //ET模式下，内核只会在“无数据->有数据”时通知一次，但是通知里到了多少字节不知道，所以需要换为循环
    //一直读到EAGIAN为止，防止残余数据不再触发通知，最后永远留在内核缓冲区里
    while(true){
        ssize_t r=read(fd,buf,sizeof(buf)); 
        if(r>0){
            // 打印收到的数据到服务端终端（方便观察 HTTP 请求报文）
            if(conn.proto==Proto::HTTP){
                fwrite(buf, 1, r, stdout);   // 直接写入标准输出，比 printf 更安全
                fflush(stdout);              // 立刻刷新，保证实时显示
            }    
            conn.rbuf.append(buf, r); // 将读取到的数据追加到连接的读缓冲区
            has_read=true;
        }else if(r==0){
            if(has_read) {
                parse(conn,epfd);
                if(conns.find(fd)==conns.end()){
                    return;
                }
            }
            
            //客户端关闭连接
            printf("client fd=%d disconnected\n",fd);
            close_connection(epfd, fd);
            return;
        }else{
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                //数据读完了，正常退出
                break;
            }
                        
            // 只有遇到 EAGAIN 之外的错误（如 ECONNRESET），才真正关闭连接
            fprintf(stderr, "read fd=%d error: %s\n", fd, strerror(errno));
            close_connection(epfd, fd);
            return;
        }
    }
    if(has_read) {
        if(conn.proto==Proto::WS){
            parse_ws_frame(conn,epfd);
            if(conns.find(fd)==conns.end()) return;

        }else{
            parse(conn,epfd); // 只有在读取到数据时才调用 parse
            //检查parse里是否触发了501或者格式错误（已经删除conn），触发了则直接返回
            if(conns.find(fd)==conns.end())return;
                
            if(conn.state==ParseState::DONE){
                auto it =conn.headers.find("upgrade");
                if(it!=conn.headers.end()&&it->second=="websocket"){
                    auto key_it = conn.headers.find("sec-websocket-key");
                    if(key_it!=conn.headers.end()){
                        std::string accept = compute_accept(conn.headers["sec-websocket-key"]);
                        std::string resp=
                            "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: "+accept+"\r\n"
                            "\r\n";
                        write(conn.fd,resp.data(),resp.size());
                        //转换为WebSocket
                        conn.proto=Proto::WS;
                        conn.rbuf.clear();
                        //不断开，保持连接
                        return;
                    }else{
                        // 缺少 key，返回 400
                        const char* resp400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                        write(conn.fd, resp400, strlen(resp400));
                        close_connection(epfd, conn.fd);
                        return;
                    }
                }

                if(conn.method=="GET"){
                    handle_get(conn,epfd);
                }else if(conn.method=="POST"){
                    handle_post(conn,epfd);
                }

                //如果handle_get里触发了404或者其他错误（已经删除conn），触发了则直接返回
                if(conns.find(fd)==conns.end()){
                    return;
                }
            }
        }
    }
}

int main(){
    int listen_fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd<0){
        fprintf(stderr,"socket() failed: %s\n",strerror(errno));
        return 1;
    }
    //防止服务器重新连接时，有旧连接残留在TIME_WAIT状态，导致bind显示“Address already in use”   
    int opt=1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(8888);

    if(bind(listen_fd,(sockaddr*)&addr,sizeof(addr))<0){
        fprintf(stderr,"bind() failed:%s\n",strerror(errno));
        close(listen_fd);
        return 1;
    }

    if(listen(listen_fd,128)<0){
        fprintf(stderr,"listen() failed:%s\n",strerror(errno));
        close(listen_fd);
        return 1;
    }
    set_nonblock(listen_fd);  //监听套接字设置为非阻塞模式

    int epfd=epoll_create1(0);
    if(epfd<0){
        fprintf(stderr,"epoll_create1() failed:%s\n",strerror(errno));
        return 1;
    }

    epoll_event ev,events[MAX_EVENTS];
    //边缘触发模式 ET
    ev.events=EPOLLIN|EPOLLET; 
    ev.data.fd=listen_fd;
    epoll_ctl(epfd,EPOLL_CTL_ADD,listen_fd,&ev);

    printf("epoll server listening on 0.0.0.0:8888 (press Ctrl+C to exit)\n");
    while(true){
        //无限等待
        int n=epoll_wait(epfd,events,MAX_EVENTS,-1);
        if(n<0){
            //被信号打断，不是报错，跳出循环
            if(errno==EINTR){
                continue;
            }
            fprintf(stderr,"epoll_wait: %s\n",strerror(errno));
            return 1;
        }

        for(int i=0;i<n;i++){
            int fd=events[i].data.fd;
            //来新客人了
            if(fd==listen_fd){
                handle_accept(epfd,listen_fd);
            }else{
                handle_read(epfd,fd);
            }
        }
    }
    close(epfd);
    close(listen_fd);
    return 0;
}