#include <sys/mman.h>
#include <unistd.h>
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

#define MAX_DOMAINS 20
#define MAX_HOPS 30
#define STRING_LEN_IP 20

char buf[100];
void* domain_hop_port;
void* domain_hop_ip;
int total_domains;
int* finished;
int finished_num;

void findPath(char*,int);
void icmpSniffer();

int main(){
    finished_num=0;

    total_domains = 0;    
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;
    domain_hop_port = mmap(NULL,MAX_DOMAINS*MAX_HOPS*sizeof(int),protection,visibility,-1,0);
    domain_hop_ip = mmap(NULL,MAX_DOMAINS*MAX_HOPS*STRING_LEN_IP*sizeof(char),protection,visibility,-1,0);
    char domain_names[MAX_DOMAINS][100];
    FILE* fptr = fopen("domains.txt","r");
    if(fptr==NULL) {printf("%s\n",strerror(errno));exit(1);}
    int a = fscanf(fptr,"%s\n",domain_names[total_domains++]);
    while(a!=0 && a!=EOF && (!feof(fptr))){
        fscanf(fptr,"%s",domain_names[total_domains++]);
    }
    total_domains--;
    //for(int i=0;i<total_domains;i++) printf("%s\n",domain_names[i]);
    printf("Total domains: %d\n",total_domains);

    finished = (int*)(malloc(total_domains*sizeof(int)));
    for(int i=0;i<total_domains;i++) finished[i] = 0;

    //fill all ip_strings with some dummy value
    for(int i=0;i<total_domains;i++){
        for(int j=0;j<MAX_HOPS;j++) strcpy((domain_hop_ip+MAX_HOPS*STRING_LEN_IP*i+j*STRING_LEN_IP),"XYZ");
    }
    if(fork()==0) {icmpSniffer();exit(1);}
    sleep(1);//maybe replace with mutex

    for(int i=0;i<total_domains;i++){
        if(fork()==0) {
           //printf("%d.\n",i);
           findPath(domain_names[i],i);
           exit(1);
           break;
        }
    }

    //while(finished_num<total_domains-1);
    sleep(2);
    printf("Calculating common path...\n");
    sleep(3);
    // for(int i=0;i<total_domains;i++){
    //     for(int j=0;j<MAX_HOPS;j++){
    //         printf("%s ",(char*)(domain_hop_ip+MAX_HOPS*STRING_LEN_IP*i+STRING_LEN_IP*j));
    //     }
    //     printf("\n");
    // }
    bool match = true;
    for(int j=0;j<MAX_HOPS;j++){
        for(int i=0;i<total_domains-1;i++){
            if(strcmp(domain_hop_ip+MAX_HOPS*STRING_LEN_IP*i+j*STRING_LEN_IP,"XYZ")==0){
                match=false;
                break;
            }
            if(strcmp(domain_hop_ip+MAX_HOPS*STRING_LEN_IP*i+j*STRING_LEN_IP,domain_hop_ip+MAX_HOPS*STRING_LEN_IP*(i+1)+j*STRING_LEN_IP)!=0){
                match=false;
                break;
            }
        }
        if(!match) break;
        printf("%s\n",(char*)(domain_hop_ip+j*STRING_LEN_IP));
    }

    return 0;
}
void findPath(char* domain_name,int domain_num){
    printf("%s.\n",domain_name);

    struct addrinfo hints,*infoptr;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    int res = getaddrinfo(domain_name,"8089",&hints,&infoptr);
    if(res!=0) printf("getaddrinfo error.\n");

    for(int i=0;i<MAX_HOPS;i++){
        int sockfd = socket(infoptr->ai_family,infoptr->ai_socktype,infoptr->ai_protocol);
        if(sockfd<0) printf("socket creation error.\n");
        struct sockaddr_in udpaddr;
        memset(&udpaddr,0,sizeof(udpaddr));
        udpaddr.sin_family = AF_INET;
        udpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        for(int j=5000;j<50000;j++){
            udpaddr.sin_port = htons(j);
            if(bind(sockfd,(struct sockaddr*)&udpaddr,sizeof(struct sockaddr))==0){
                //printf("port assigned %s %d %d\n",domain_name,i,j);
                *(int*)(domain_hop_port+MAX_HOPS*4*domain_num+4*i) = j;
                break;
            }
        }
        int ttl=i+1;
        setsockopt(sockfd,IPPROTO_IP,IP_TTL,(void*)&ttl,sizeof(int));
        char buf[10];
        strcpy(buf,"hello");
        int b = sendto(sockfd,(const char*)buf,sizeof(buf),0,infoptr->ai_addr,infoptr->ai_addrlen);
        if(b<0){
            printf("%s\n",strerror(errno));
        }
        
    }

    pause();
}

void icmpSniffer(){
    int icmpfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    if(icmpfd==-1) {
        printf("ICMP raw socket opening error.\nTry with priveledged user.\n");
        exit(1);
    }
    //recvfrom()
    struct msghdr msg;
    struct sockaddr src_addr;
    socklen_t src_addr_len = sizeof(struct sockaddr_in);
    unsigned char icmpbuf[56];

    //signal(SIGALRM,closeprog);
    while(1){
        //printf("going in.\n");
        //alarm(10);
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
        //search for sender
        bool found = false;
        int d,h;
        for(int i=0;i<total_domains;i++){
            for(int j=0;j<MAX_HOPS;j++){
                if(*(int*)(domain_hop_port+MAX_HOPS*4*i+4*j)==ntohs(udph->uh_sport)){
                    found = true;
                    d=i;h=j;
                    break;
                }
            }
            if(found) break;
        }
        if(!found) {
            //printf("not found :(\n");
            continue;
        }

        if(icmp_hdr->type==11){
            //printf("writing ip.\n");
            strcpy((domain_hop_ip+MAX_HOPS*STRING_LEN_IP*d+STRING_LEN_IP*h),inet_ntoa(((struct sockaddr_in*)&src_addr)->sin_addr));                
        }
        if(icmp_hdr->type==3){
            if(finished[d]==0){
                //printf("finished.\n");
                finished[d]=1;
                finished_num++;
            }
        }

        //int thread = ntohs(udph->uh_sport)%BATCH_SIZE;
        //printf("\nPort is %d and thread is %d\n",ntohs(udph->uh_sport),thread);
        //memcpy(thread_msg[thread],icmpbuf,56);
        //thread_ip[thread] = memcpy(thread_ip[thread],&src_addr,sizeof(struct sockaddr));
        //printf("Bla Bla %s.\n",inet_ntoa(((struct sockaddr_in*)(thread_ip[thread]))->sin_addr));
        //strcpy(thread_msg[thread],(char*)icmpbuf);
        //strcpy(thread_ip[thread],(char*)&src_addr);
        //sem_post(&thread_sync[thread]);

        // if(icmp_hdr->type==11){
        //     printf("\nHop\tIP: %s\n",inet_ntoa(((struct sockaddr_in*)&src_addr)->sin_addr));
        // }
        if(icmp_hdr->type==3){
            //printf("Reached IP:\t%s\n",inet_ntoa(((struct sockaddr_in*)&src_addr)->sin_addr));
            //reached = true;
        }
        
    }
}