#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<cstring>
#include<cstdio>
#include<cerrno>
#include<unistd.h>
#include<arpa/inet.h>

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

    int epfd=epoll_create1(0);
    if(epfd<0){
        fprintf(stderr,"epoll_create1() failed:%s\n",strerror(errno));
        return 1;
    }

    epoll_event ev,events[MAX_EVENTS];
    ev.events=EPOLLIN;
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
                sockaddr_in cli_addr;
                socklen_t cli_len=sizeof(cli_addr);
                int conn_fd=accept(listen_fd,(sockaddr*)&cli_addr,&cli_len);
                if(conn_fd<0){
                    fprintf(stderr,"accept: %s\n",strerror(errno));
                    continue;
                }
                printf("client connected: %s:%d fd=%d\n",
                    inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port),conn_fd);
                
                epoll_event cev;
                cev.events=EPOLLIN;
                cev.data.fd=conn_fd;
                epoll_ctl(epfd,EPOLL_CTL_ADD,conn_fd,&cev);
            }else{
                //某个客人的专线就绪，有数据可读
                char buf[1024];
                ssize_t r=read(fd,buf,sizeof(buf));
                if(r>0){
                    write(fd,buf,r);
                }else if(r==0){
                    //客户端关闭连接
                    printf("client fd=%d disconnected\n",fd);
                    epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                    close(fd);
                }else{
                    //r<0
                    fprintf(stderr,"read: %s\n",strerror(errno));
                    //r==0
                    epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                    close(fd);
                }
            }
        }
    }
    close(epfd);
    close(listen_fd);
    return 0;
}