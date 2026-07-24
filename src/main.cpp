#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<cstring>
#include<cstdio>
#include<cerrno>
#include<unistd.h>
#include<arpa/inet.h>
#include<fcntl.h>

//fd转换为非阻塞模式
void set_nonblock(int fd){
    int flags=fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flags|O_NONBLOCK);
}

constexpr int MAX_EVENTS=64;

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

                    epoll_event cev;
                    //边缘触发模式
                    cev.events=EPOLLIN|EPOLLET;
                    cev.data.fd=conn_fd;
                    epoll_ctl(epfd,EPOLL_CTL_ADD,conn_fd,&cev);
                }
            }else{
                //某个客人的专线就绪，有数据可读
                char buf[1024];
                
                //使用循环读取，直到读完所有数据
                //ET模式下，内核只会在“无数据->有数据”时通知一次，但是通知里到了多少字节不知道，所以需要换为循环
                //一直读到EAGIAN为止，防止残余数据不再触发通知，最后永远留在内核缓冲区里
                while(true){
                    ssize_t r=read(fd,buf,sizeof(buf)); 
                    if(r>0){
                        // 打印收到的数据到服务端终端（方便观察 HTTP 请求报文）
                        fwrite(buf, 1, r, stdout);   // 直接写入标准输出，比 printf 更安全
                        fflush(stdout);              // 立刻刷新，保证实时显示
                        
                        write(fd,buf,r);
                    }else if(r==0){
                        //客户端关闭连接
                        printf("client fd=%d disconnected\n",fd);
                        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                        close(fd);
                        break;
                    }else{
                        if(errno==EAGAIN||errno==EWOULDBLOCK){
                            //数据读完了，正常退出
                            break;
                        }
                        
                        // 只有遇到 EAGAIN 之外的错误（如 ECONNRESET），才真正关闭连接
                        fprintf(stderr, "read fd=%d error: %s\n", fd, strerror(errno));
                        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                        close(fd);
                        break;
                    }
                }
            }
        }
    }
    close(epfd);
    close(listen_fd);
    return 0;
}