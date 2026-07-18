#include<sys/socket.h>
#include<netinet/in.h>
#include<cstring>
#include<cstdio>
#include<cerrno>
#include<unistd.h>
#include<arpa/inet.h>

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

    printf("listening on 0.0.0.0:8888 (press Ctrl+C to exit)\n");
    while (true){
        sockaddr_in client_addr;
        socklen_t cli_len=sizeof(client_addr);
        int conn_fd=accept(listen_fd,(sockaddr*)&client_addr,&cli_len);
        if (conn_fd < 0) {
            fprintf(stderr, "accept() failed: %s\n", strerror(errno));
            continue;
        }
        printf("connection accepted: %s:%d\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
        while(true){
            char buf[1024];
            ssize_t n=read(conn_fd,buf,sizeof(buf));

            if(n==0){
                printf("client: %s%d closed connection\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
                break;
            }else if(n<0){
                fprintf(stderr,"read() failed: %s\n",strerror(errno));
                break;
            }
            write(conn_fd,buf,n);
        }
        close(conn_fd);
    }
    
    close(listen_fd);
    return 0;
}