#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

sem_t mutex,send_mutex;
int sockfd, icmpfd;
char buf[10];
//struct sockaddr_in servaddr;
struct addrinfo* infoptr = NULL;

void* icmpthread();
void* udpthread(void*);

int main(){
    sem_init(&mutex,0,0);
    sem_init(&send_mutex,0,1);

    strcpy(buf,"YYYYYY");

    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    int res = getaddrinfo("www.twitter.com","8089",&hints,&infoptr);
    if(res!=0) printf("getaddrinfo error.\n");
    if(infoptr->ai_family==AF_INET) printf("Hurray!IPv4.\n");

    //memset(&servaddr,0,sizeof(servaddr));
    //servaddr.sin_family = AF_INET;

    sockfd = socket(infoptr->ai_family,infoptr->ai_socktype,infoptr->ai_protocol);
    if(sockfd<0) printf("socket creation error.\n");
    if(connect(sockfd,infoptr->ai_addr,infoptr->ai_addrlen)!=0) printf("connect error.\n");

    icmpfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
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
    pause();
}

void* icmpthread(){
    //recvfrom()
    struct msghdr msg;
    struct sockaddr src_addr;
    socklen_t src_addr_len = sizeof(struct sockaddr_in);
    unsigned char icmpbuf[1000];
    while(1){
        //int b = recvmsg(icmpfd,&msg,0);
        memset((void*)&src_addr,0,src_addr_len);
        int b = recvfrom(icmpfd,(void*)icmpbuf,sizeof(icmpbuf),0,(struct sockaddr*)&src_addr,&src_addr_len);
        printf(" rec'd %d bytes\n",b);
        printf("IP: %s\n",inet_ntoa(((struct sockaddr_in*)&src_addr)->sin_addr));
        struct iphdr *ip_hdr = (struct iphdr *)icmpbuf;
        printf("IP header is %d bytes.\n", ip_hdr->ihl*4);
        for (int i = 0; i < b; i++) {
          printf("%02X%s", (uint8_t)icmpbuf[i], (i + 1)%16 ? " " : "\n");
            }
        printf("\n");
        struct icmphdr *icmp_hdr = (struct icmphdr *)((char *)ip_hdr + (4 * ip_hdr->ihl));
        printf("ICMP msgtype=%d, code=%d\n", icmp_hdr->type, icmp_hdr->code);;
    }
}

void* udpthread(void* arg){
    int ttl = *(int*)arg;
    sem_post(&mutex);
    printf("recieved %d.\n",ttl);

    //send datagram
    sem_wait(&send_mutex);
    struct sockaddr* sa;
    sa = malloc(infoptr->ai_addrlen);
    memcpy(sa,infoptr->ai_addr,infoptr->ai_addrlen);
    setsockopt(sockfd,IPPROTO_IP,IP_TTL,(void*)&ttl,sizeof(ttl));
    int b=0;
    b = sendto(sockfd,(const char*)buf,sizeof(buf),0,infoptr->ai_addr,infoptr->ai_addrlen);
    if(b<0){
        printf("%s\n",strerror(errno));
    }
    printf("bytes %d %d\n",ttl,b);
    sem_post(&send_mutex);
}