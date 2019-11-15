#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
//#include "protocol.h"
#include "servers_protocol.h"
#include "file.h"

void *handle_message(void *data);//communicate with client


void debugs(){
    printf("1234\n");
}

int main(int argc, char *argv[]){
 
	int sock;
    pthread_t pid[100];
    int count=0;
    int find_p=0;// -p port exist or not
    for(int i=1; i<argc; ++i){
        if(strcmp(argv[i], "-p")==0){
            find_p=1;
            break;
        }
    }
    if(!find_p){
        print_replicas_usage(argv[0]);
        exit(1);
    }
    char c;
    while( (c=getopt(argc,argv,"n:p:")) != -1){
        switch(c){

        case 'p':
            myport=atoi(optarg);
            break;
        case 'n':
            ;
            break;
        default:
            print_client_usage(argv[0]);
            exit(1);
            break;
        }
    }
    /*` connect to master   */
    int master_socket= socket(AF_INET, PROTOCOL, 0);;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    //bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(DEFAULT_IP); 
	servaddr.sin_port = htons(SERVER_PORT);
    bzero(&(servaddr.sin_zero),8);
    //connect to master
    if(connect(master_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)  
    {  
        printf("can not connect to server, exit!\n");   
        exit(1);  
    }  
    master_sockfd=master_socket;
    servers[0]={master_sockfd, SERVER_PORT};
    pthread_t server_pid;
    /*` create a socket and  listen to clients    */
    sock=create_client_port(myport, NULL);
    hasOpenMyPort=1;
    if(pthread_create(&server_pid, NULL, handle_message, (void *)&master_socket)){
            printf("Client Dump: thread can not established on socknum %d!\n", sock);
    }


    while (1)
    {
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_size = sizeof(clnt_addr);

        socklen_t sock_len;
        int clnt_sock = accept(sock, (struct sockaddr*)&clnt_addr, &sock_len);
        if(clnt_sock==-1){
            //usleep(SLEEP_TIME);
            continue ;
        }
        
        if(pthread_create(&pid[count++], NULL, handle_message, (void *)&clnt_sock)){
            printf("ERROR: thread can not established on socknum %d!\n", clnt_sock);
        }
        printf(">>>>>accpeted new request, thread established on socknum %d!\n", clnt_sock);
        

    }
    for(int i=0; i<count; ++i){
        pthread_join(pid[i], NULL);
    }
    close(sock);
    
    return 0;
}

// ====================================================================================================
//  1. ask master give it a unique number to distinguish it from others
//  2. get other data center infomation. At the mean time, let others know a new server is participate
//  3. update dependency relationship
//
// =====================================================================================================

