#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/epoll.h>

#define MAX_EVENTS 1
#define BUF_SIZE 64
int main(){
    int epfd,ret;
    struct epoll_event ev,events[MAX_EVENTS];
    char buf[BUF_SIZE];
    ssize_t nread;

    epfd=epoll_create1(0);
    if(epfd==-1){
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events=EPOLLIN;
    ev.data.fd=0;

    if(epoll_ctl(epfd,EPOLL_CTL_ADD,0,&ev)==-1){
        perror("epoll_ctl: add stdin");
        close(epfd);
        exit(EXIT_FAILURE);
    }
    printf("epoll 已启动，正在监听 stdin (fd 0)，请输入内容 (Ctrl+D 退出):\n");

    while(1){
        //无限等待
        ret=epoll_wait(epfd,events,MAX_EVENTS,-1);
        if(ret==-1){
            perror("epoll_wait");
            break;
        }
        for(int i=0;i<ret;++i){
            if(events[i].data.fd==0){
                nread=read(0,buf,sizeof(buf)-1);
                if(nread==-1){
                    perror("read");
                    continue;
                }else if(nread==0){
                    printf("\n检测到EOF，退出\n");
                    close(epfd);
                    exit(EXIT_SUCCESS);
                }else{
                    buf[nread]='\0';
                    printf("读到 %zd 字节：%s",nread,buf);
                }
            }
        }
    }
    close(epfd);
    return 0;
}