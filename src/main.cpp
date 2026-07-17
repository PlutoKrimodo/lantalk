#include<sys/socket.h>
#include<netinet/in.h>
#include<cstring>
#include<cstdio>
#include<cerrno>
#include<unistd.h>

int main(){
    int listen_fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd<0){
        fprintf(stderr,"socket() failed: %s\n",strerror(errno));
        return 1;
    }

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

    printf("listening on 0.0.0.8888 (press Ctrl+C to exit)\n");
    pause();
    close(listen_fd);
    return 0;
}