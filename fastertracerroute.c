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
#include <netinet/udp.h>
#include <stdbool.h>
#include <signal.h>

#define ICMP_ID "XYZ" //3 letter code for identifying icmp messages
#define ICMP_MSG_SIZE 7 //3 bytes for code and 4 bytes for int
#define BATCH_SIZE 10

bool reached;
sem_t ttlmutex, batchmutex;
sem_t thread_sync[BATCH_SIZE];
char* thread_msg[BATCH_SIZE];
char* thread_ip[BATCH_SIZE];
char buf[7]; //3 bytes for code and 4 bytes for int
//struct sockaddr_in servaddr;
struct addrinfo* infoptr = NULL;

void* icmpthread();
void* udpthread(void*);

int main(int argc,char* argv[]){
    // thread_sync = (sem_t*)malloc(sizeof(sem_t));
    // thread_msg = (char**)malloc(sizeof(char*));
    // thread_ip = (char**)malloc(sizeof(char*));

    bool reached = false;
    sem_init(&ttlmutex,0,0); //for setting ttl values
    sem_init(&batchmutex,0,BATCH_SIZE);//for batchsize mutexes
    //thread synchronisation mutexes and other things
    for(int i=0;i<BATCH_SIZE;i++) {
        sem_init(&thread_sync[i],0,0);
        thread_msg[i] = (char*)malloc(56*sizeof(char));
        thread_ip[i] = (char*)malloc(sizeof(struct sockaddr));
    }

    strcpy(buf,ICMP_ID);

    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    int res = getaddrinfo(argv[1],"8089",&hints,&infoptr);
    if(res!=0) printf("getaddrinfo error.\n");
    //if(infoptr->ai_family==AF_INET) printf("Hurray!IPv4.\n");

    //memset(&servaddr,0,sizeof(servaddr));
    //servaddr.sin_family = AF_INET;

    // sockfd = socket(infoptr->ai_family,infoptr->ai_socktype,infoptr->ai_protocol);
    // if(sockfd<0) printf("socket creation error.\n");
    // if(connect(sockfd,infoptr->ai_addr,infoptr->ai_addrlen)!=0) printf("connect error.\n");

    // icmpfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    // if(icmpfd==-1) {printf("icmp socket opening error.\n");exit(1);}

    pthread_t threads[BATCH_SIZE];
    pthread_t icmpthreadid;

    printf("Testing for max 30 hops.\n");
    for(int i=0;i<31;i++){
        if(reached) break;
        if(!reached){
            sem_wait(&batchmutex);
            if(i==0){
                //pthread_t threadid;
                pthread_create(&icmpthreadid,NULL,icmpthread,NULL);
            }
            else{
                //pthread_t threadid;
                int ttl = i;
                pthread_create(&threads[i%BATCH_SIZE],NULL,udpthread,(void*)&ttl);
                sem_wait(&ttlmutex);
            }
        }
    }
    //pthread_join(icmpthreadid,NULL);
    //for(int i=0;i<BATCH_SIZE;i++) pthread_join(threads[i],NULL);
    //alarm(10);
    //pause();
}

void closeprog(int sig){
    exit(1);
}

void* icmpthread(){
    int icmpfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    if(icmpfd==-1) {printf("icmp socket opening error.\n");exit(1);}
    //recvfrom()
    struct msghdr msg;
    struct sockaddr src_addr;
    socklen_t src_addr_len = sizeof(struct sockaddr_in);
    unsigned char icmpbuf[56];

    signal(SIGALRM,closeprog);
    while(1){
        alarm(10);
        //int b = recvmsg(icmpfd,&msg,0);
        //if(reached) break;
        memset((void*)&src_addr,0,src_addr_len);
        int b = recvfrom(icmpfd,(void*)icmpbuf,sizeof(icmpbuf),0,(struct sockaddr*)&src_addr,&src_addr_len);
        //printf("rec'd %d bytes\n",b);
        //printf("IP: %s\n",inet_ntoa(((struct sockaddr_in*)&src_addr)->sin_addr));
        struct iphdr *ip_hdr = (struct iphdr *)icmpbuf;
        // printf("IP header is %d bytes.\n", ip_hdr->ihl*4);
        
        // for (int i = 0; i < b; i++) {
        //   printf("%02X%s", (uint8_t)icmpbuf[i], (i + 1)%16 ? " " : "\n");
        // }
        
        //printf("\n");
        struct icmphdr *icmp_hdr = (struct icmphdr *)((char *)ip_hdr + (4 * ip_hdr->ihl));
        //printf("ICMP msgtype=%d, code=%d\n", icmp_hdr->type, icmp_hdr->code);
        //printf("\nPort is %d.\n",icmp_hdr->)

        
        //void* hopmsg = (void*)((void*)icmp_hdr + 36);//size of icmp header is 8 bytes 
        // if(!(*(char*)hopmsg=='X' && *(char*)(hopmsg+1)=='Y' && *(char*)(hopmsg+2)=='Z')){
        //     printf("\n%c %c %c Not our message.\n",(*(char*)hopmsg),(*(char*)(hopmsg+1)),(*(char*)(hopmsg+2)));
        // }
        struct udphdr *udph = (struct udphdr*)(icmpbuf+48);
        int thread = ntohs(udph->uh_sport)%BATCH_SIZE;
        //printf("\nPort is %d and thread is %d\n",ntohs(udph->uh_sport),thread);
        memcpy(thread_msg[thread],icmpbuf,56);
        thread_ip[thread] = memcpy(thread_ip[thread],&src_addr,sizeof(struct sockaddr));
        //printf("Bla Bla %s.\n",inet_ntoa(((struct sockaddr_in*)(thread_ip[thread]))->sin_addr));
        //strcpy(thread_msg[thread],(char*)icmpbuf);
        //strcpy(thread_ip[thread],(char*)&src_addr);
        sem_post(&thread_sync[thread]);

        // if(icmp_hdr->type==11){
        //     printf("\nHop\tIP: %s\n",inet_ntoa(((struct sockaddr_in*)&src_addr)->sin_addr));
        // }
        if(icmp_hdr->type==3){
            //printf("Reached IP:\t%s\n",inet_ntoa(((struct sockaddr_in*)&src_addr)->sin_addr));
            reached = true;
        }
        
    }
}

void* udpthread(void* arg){
    int ttl = *(int*)arg;
    sem_post(&ttlmutex);

    int sockfd = socket(infoptr->ai_family,infoptr->ai_socktype,infoptr->ai_protocol);
    if(sockfd<0) printf("socket creation error.\n");
    //if(connect(sockfd,infoptr->ai_addr,infoptr->ai_addrlen)!=0) printf("connect error.\n");
    
    struct sockaddr_in udpaddr;
    memset(&udpaddr,0,sizeof(udpaddr));
    udpaddr.sin_family = AF_INET;
    udpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    udpaddr.sin_port = htons(ttl);
    if(bind(sockfd,(struct sockaddr*)&udpaddr,sizeof(struct sockaddr))<0) {
        printf("%s\n",strerror(errno));
        printf("bind error %d %d.\n",ttl,udpaddr.sin_port);
    }
    
    //printf("recieved %d.\n",ttl);

    // int sockfd = socket(infoptr->ai_family,infoptr->ai_socktype,infoptr->ai_protocol);
    // if(sockfd<0) printf("socket creation error.\n");
    // if(connect(sockfd,infoptr->ai_addr,infoptr->ai_addrlen)!=0) printf("connect error.\n");

    struct sockaddr* sa;
    sa = malloc(infoptr->ai_addrlen);
    memcpy(sa,infoptr->ai_addr,infoptr->ai_addrlen);
    setsockopt(sockfd,IPPROTO_IP,IP_TTL,(void*)&ttl,sizeof(ttl));
    int b=0;
    *(int*)(buf+3)=ttl;
    for(int i=0;i<3;i++){
        b = sendto(sockfd,(const char*)buf,sizeof(buf),0,infoptr->ai_addr,infoptr->ai_addrlen);
        if(b<0){
            printf("%s\n",strerror(errno));
        }
    }
    //printf("bytes %d %d\n",ttl,b);

    //printf("reached here %d.\n",ttl);
    sem_wait(&thread_sync[ttl%BATCH_SIZE]);
    //printf("reached here too %d.\n",ttl);
    struct iphdr *ip_hdr = (struct iphdr *)thread_msg[ttl%BATCH_SIZE];
    struct icmphdr *icmp_hdr = (struct icmphdr *)((char *)ip_hdr + (4 * ip_hdr->ihl));
    if(icmp_hdr->type==11){
        printf("%d\t%s\n",ttl,inet_ntoa(((struct sockaddr_in*)thread_ip[ttl%BATCH_SIZE])->sin_addr));
    }
    if(icmp_hdr->type==3){
        printf("%d\t***\n",ttl);
    }
    //printf("reachead here 2.\n");
    sem_post(&batchmutex);
}