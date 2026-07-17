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
    printf("socket created, fd=%d\n",listen_fd);
    close(listen_fd);
    return 0;
}