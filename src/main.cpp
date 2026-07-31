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

#include"connection.h"
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
}

std::string get_content_type(const std::string& path){
    if(path.size()>=5&&path.substr(path.size()-5)==".html"){
        return "text/html";
    }
    if(path.size()>=4&&path.substr(path.size()-4)==".css"){
        return "text/css";
    }
    if(path.size()>=3&&path.substr(path.size()-3)==".js"){
        return "application/javascript";
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
            fwrite(buf, 1, r, stdout);   // 直接写入标准输出，比 printf 更安全
            fflush(stdout);              // 立刻刷新，保证实时显示
                        
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
        parse(conn,epfd); // 只有在读取到数据时才调用 parse
        //检查parse里是否触发了501或者格式错误（已经删除conn），触发了则直接返回
        if(conns.find(fd)==conns.end()){
            return;
        }

        if(conn.state==ParseState::DONE){
            handle_get(conn,epfd);
            //如果handle_get里触发了404或者其他错误（已经删除conn），触发了则直接返回
            if(conns.find(fd)==conns.end()){
                return;
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