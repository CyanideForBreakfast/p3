#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <semaphore.h>
#include <unistd.h>

sem_t mutex;

void* icmpthread();
void* udpthread(void*);

int main(){
    sem_init(&mutex,0,0);

    struct addrinfo hints, *infoptr;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    int res = getaddrinfo("www.twitter.com",NULL,&hints,&infoptr);
    if(res!=0) printf("getaddrinfo error.\n");

    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(connect(sockfd,infoptr->ai_addr,infoptr->ai_addrlen)!=0) printf("connect error.\n");

    int icmpfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    if(icmpfd==-1) {printf("icmp socket opening error.\n");exit(1);}

    for(int i=0;i<10;i++){
        if(i==0){
            pthread_t threadid;
            pthread_create(&threadid,NULL,icmpthread,NULL);
        }
        else{
            pthread_t threadid;
            int ttl = i;
            pthread_create(&threadid,NULL,udpthread,(void*)&ttl);
            sem_wait(&mutex);
        }
    }
}

void* icmpthread(){

}

void* udpthread(void* arg){
    int ttl = *(int*)arg;
    sem_post(&mutex);
    printf("recieved %d.\n",ttl);
}