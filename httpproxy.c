#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <poll.h>
#include <regex.h>

#define MAX 10
#define MAXNAMELEN 256*2
#define HTTPHEADSIZE 1024*10
#define WEBPAGESIZE 30000000

struct timespec start; 
struct timespec end;

int i;//used to count how many threads are there should be less than 10;
int cacheinfo = 0;//if cacheinfo ==1 print cache use info;


struct cache
{
    char request[MAXNAMELEN];
    char block[WEBPAGESIZE];
    int blocksize;
    int visitime;
    int used;
};
struct cache entry[10];
int cache_now;
int cache_all;

struct info
{
    char ip[30];
    char req[MAXNAMELEN];
    char state[15];
    int datalong;
    int time;
};
struct info resulttext;


int delet_entry(char * target,int longgg,char *data1)
{
         if(cacheinfo ==1)
         	printf("=====cache info:cache out of size!!!!delete Least Recently Used one=====\n");
         int min,flag,x;
         min = entry[0].visitime;
         for(x =0;x < 10;x++)
         {
             if(entry[x].visitime <= min && entry[x].used == 1)
             {
                 min = entry[x].visitime;
                 flag = x;
             }
         }
         entry[flag].used = 0;
         entry[flag].visitime = 0;
         cache_now = cache_now - entry[flag].blocksize;
         if(cacheinfo ==1)
         	printf("=====cache info :%d bytes deleted!,%d bytes left=====\n",entry[flag].blocksize,cache_all - cache_now);
         if(cache_all - cache_now > longgg)
         {
            for(x=0;x<10;x++)
            {
                if(entry[x].used == 0)
                {
                    strcpy(entry[x].request,target);
                    strcpy(entry[x].block,data1);
                    entry[x].used = 1;
                    entry[x].blocksize = longgg;
                    cache_now = cache_now + longgg;
                    if(cacheinfo ==1)
                    	printf("=====%d bytes added to cache , %d bytes left======\n", longgg ,cache_all - cache_now);
                    break;
                }
                return 0;
            }
         }
         else
         {
            delet_entry(target,longgg,data1);
         }
         return 0;
}

long toms(struct timespec time)
{
    return time.tv_sec*1000 + time.tv_nsec/1000000;
}

void extract_match(char * dest, const char * src, regmatch_t match)
{
    regoff_t o;
    for (o=0; o < match.rm_eo - match.rm_so; o++)
    {
        dest[o] = src[o + match.rm_so];
    }
    dest[o] = 0;
}

void * getdata(char * hostname, char * request, char * resourceName, size_t * length)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo *result = 0;
    if (getaddrinfo(hostname, "80", &hints, &result) != 0)
    {
        printf("Unable to resolve hostname %s\n", hostname);
        return 0;
    }
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol); 
    connect(sock, result->ai_addr, result->ai_addrlen);
    write(sock, request, strlen(request)) != strlen(request);
//==============================================================
    char buffer[1024];
    char * web;
    web = malloc((unsigned long)WEBPAGESIZE);
    web[0] = '\0';
    size_t count = 0;
    printf("connect to web...\n");
    int round;
    round =0;
    ssize_t size_buffer = 1;
    while(size_buffer)
    {
        round++;
        size_buffer = recv(sock, buffer, sizeof(buffer),0);
        strcat(web,buffer);
        count += (size_t)size_buffer;
    }
    close(sock); 
    //printf("Downloaded %d bytes from %s\n", (int)count, resourceName);
    *length = count;
    count = 0;
    return web; 
}

void * connections(void * temp)
{
    int id = *(int *)temp;

    void * data;//return value
    size_t Content_Length;//datalength

    int in_no,x;
    in_no = 0;
    char buffer[HTTPHEADSIZE];//http head
    memset(buffer, 0, sizeof(buffer));
    size_t r = recv(id, buffer, sizeof(buffer), 0);
   // printf("=====\n message info :%s\n=====\n",buffer);
    if (r <= 0)
    {
        printf("Client disconnected (%zu)\n", r);
        close(id);
        return 0;
    }
    

    regex_t reg;
    regmatch_t matches[2];
    regcomp(&reg, "GET +([^ \t]+) +HTTP/[0-9.]+ *[\r|\n]", REG_ICASE | REG_EXTENDED);
    regexec(&reg, buffer, 2, matches, 0);
    regfree(&reg);
    
    char requestTarget[MAXNAMELEN];
    extract_match(requestTarget, buffer, matches[1]);
    //printf("Processing request: %s\n", requestTarget);
    strcpy(resulttext.req,requestTarget);//info part
    clock_gettime(CLOCK_MONOTONIC, &start);//gettime 
    //==============check cache===================
 
    for(x = 0; x < 10 ;x++ )
    {
         if(strcmp(entry[x].request,requestTarget)==0 && entry[x].used == 1)
         {
             //printf("found in cache return data from cache\n");
             data = entry[x].block;
             Content_Length = entry[x].blocksize;
             entry[x].visitime++;
             in_no = 1;
             strcpy(resulttext.state,"CACHE_HIT");
             break;
         }
    }

//=================================
    if(in_no == 0)
    {
     //=============if not in the cache get data and add it to cache====================
        //printf("not found in cache ,send requset to HOST\n");
        strcpy(resulttext.state,"CACHE_MISS");
        regcomp(&reg, "Host:[ \t]+([^ \t\r\n]+)[\r|\n]", REG_ICASE | REG_EXTENDED);
        regexec(&reg, buffer, 2, matches, 0);
        regfree(&reg);

        char hostname[MAXNAMELEN];
        extract_match(hostname, buffer, matches[1]);
        //printf("Connecting to host: %s\n", hostname);
        
        data = getdata(hostname, buffer, requestTarget, &Content_Length);
        //===========add data to cache;

        if(cache_all - cache_now > Content_Length)
        {
            int flag = 0;
            for(x=0;x<10;x++)
            {
                if(entry[x].used == 0)
                {
                    flag = 1;
                    strcpy(entry[x].request,requestTarget);
                    strcpy(entry[x].block,data);
                    entry[x].used = 1;
                    entry[x].blocksize = Content_Length;
                    cache_now = cache_now + Content_Length;
                    if(cacheinfo ==1)
                    	printf("=====cache info:%d bytes added to cache , %d bytes left=====\n", Content_Length,cache_all - cache_now);
                    break;
                }
            }
            if(flag == 0)
            {
                printf("=====cache info:no entry space for new entry Least Recently Used (LRU)=====\n");
            }
        }
        // //===========cache replacement;
        else if(cache_all < Content_Length )
        {
            printf("-----WORNING!!!!:larger than all cache space did't add to cache------\n");
        }
        else
        {
            delet_entry(requestTarget,Content_Length,data);
        }
        
    }
    in_no = 0;
    //==========print output message
    write(id, data, Content_Length);
    clock_gettime(CLOCK_MONOTONIC, &end);
    resulttext.datalong = Content_Length;
    resulttext.time = toms(end) - toms(start);
    close(id);
    //free(data);
    i--;
    printf("%s|%s|%s|%d|%d\n",resulttext.ip,resulttext.req,resulttext.state,resulttext.datalong,resulttext.time);
    return 0;
}


int main(int argc, const char * argv[])
{
    //cachebuffer =(char*)malloc(atoi(argv[1]) *sizeof(char));
    memset(&resulttext.state, 0, sizeof(resulttext.state));
    cache_now = 0;
    cache_all = atoi(argv[1]);
    int y;
    for(y=0;y<10;y++)
    {
        entry[y].used = 0;
        entry[y].visitime = 0;
    }

    int listenSocket = 0;
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
        printf("socket failed!!!\n");
        return 1;
    }
    struct sockaddr_in socketAddress;
    memset(&socketAddress, 0, sizeof(socketAddress));
    
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(0);//0
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    
    bind(listenSocket, (struct sockaddr*)&socketAddress, sizeof(socketAddress));
    char * servhost;
    ushort servport;
    //host name
    servhost = (char*)malloc(MAXNAMELEN *sizeof(char));
    if( gethostname(servhost, MAXNAMELEN) < 0 )
        return -1;
    struct hostent *hp;
    if( (hp = gethostbyname(servhost)) < 0 )
        return -1;
    strcpy(servhost, hp->h_name);
    //port number
    socklen_t addrlen = sizeof(socketAddress);
    memset(&socketAddress, 0, sizeof(socketAddress));

    if (getsockname(listenSocket, (struct sockaddr *)&socketAddress, &addrlen) < 0)
    {
        printf("getsockname error\n");
        exit(-1);
    }
    servport = ntohs(socketAddress.sin_port);
    printf("started server on '%s' at '%hu'\n", servhost, servport);
    free(servhost);
    listen(listenSocket, MAX+1);
    printf("======waiting for client's request======\n");
    
    pthread_t multi[MAX];
    i=0;
    while (1)
    {    
        if(i >= MAX)
        {
            printf("more than MAX(10) threads ,no more, exit!\n");
            break;
        }
        socklen_t SL=sizeof(socketAddress);
        int clientid;
        clientid = accept(listenSocket, (struct sockaddr*)&socketAddress, &SL);

        char ipclient[30];
        inet_ntop(socketAddress.sin_family, &socketAddress.sin_addr, ipclient, sizeof(ipclient));
        strcpy(resulttext.ip,ipclient);
        
        int ret;
        ret=pthread_create(&multi[i],NULL,(void * (*)(void *))connections, &clientid);
        if(ret)
        {
            printf("create pthread error!\n");
        }
        pthread_join(multi[i],NULL);
        i++;
    }
    //free(cachebuffer);
    return 0;
}
