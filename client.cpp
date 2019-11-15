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
#include <stdbool.h>
#include <semaphore.h>

#include "protocol.h"
#include "file.h"


char server_address[20];
int connect_port=SERVER_PORT;

void *read_message(void *data);
bool send_message(int servSocket);
char client_name[MAX_NAME_LEN];
int isLogin=0, isExit=0;

int main(int argc, char *argv[]){
 
	int sock;
    pthread_t pid;
    char c;
    int find_n=0;// -n exist or not
    for(int i=0; i<argc; ++i){
        if(strcmp(argv[i], "-n")==0){
            find_n=1;
            break;
        }
    }
    if(!find_n){
        print_client_usage(argv[0]);
        exit(1);
    }

	sock = socket(AF_INET, PROTOCOL, 0);
    strcpy(server_address, DEFAULT_IP);
    while( (c=getopt(argc,argv,"a:p:n:")) != -1){
        switch(c){
        case 'a':
            strcpy(server_address, optarg);
            break;

        case 'p':
            connect_port=atoi(optarg);
            break;
        case 'n':
            strcpy(client_name, optarg);
            
            break;
        default:
            print_client_usage(argv[0]);
            exit(1);
            break;
        }
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    //bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(DEFAULT_IP); 
    //inet_pton(AF_INET, DEFAULT_IP, (void*)&servaddr.sin_addr.s_addr);
	servaddr.sin_port = htons(connect_port);
    bzero(&(servaddr.sin_zero),8);
    if(connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)  
    {  
        printf("can not connect to server, exit!\n");   
        exit(1);  
    } 
    fcntl(sock, F_SETFL, O_NONBLOCK);//non block
    printf("%s, Welcome to chat room\n", client_name);
    if(pthread_create(&pid, NULL, read_message, (void *)&sock)){
            printf("Client Dump: thread can not established on socknum %d!\n", sock);
    }

    //create_connect_port(CLIENT_DEFAULT_PORT, DEFAULT_IP);
    send_message(sock);

    pthread_join(pid, NULL);
    close(sock);
    
    return 0;
}

//read from server
void *read_message(void *data){
    int sock=*((int *)data);
    MesInfo recvInfo; 
    char buf[1024];
    while(!isExit){
        memset(&recvInfo, 0, sizeof(recvInfo));
        int n=recv(sock, buf, sizeof(buf), 0);
        if(n<=0){
            usleep(SLEEP_TIME);
            continue;
        }
        memcpy(&recvInfo.MesType, buf, sizeof(recvInfo.MesType));
        switch(recvInfo.MesType){
            case MesSend: 
                 memcpy(&recvInfo.fromName, buf+sizeof(recvInfo.MesType), n-sizeof(recvInfo.MesType));
                printf("\t  [%s] ==> %s\n",recvInfo.fromName, recvInfo.MesContent);
                break;
            case FetchUser:
                outputUser(buf+sizeof(recvInfo.MesType));
                break;

        }
        
        usleep(SLEEP_TIME/5);

    }
    close(sock);

}


//send message to server
bool send_message(int servSocket)
{
    int servSock = servSocket;

    int first_time=0;
    MesInfo sendInfo; 
    string Message;
    strcpy(sendInfo.fromName, client_name);
    //printf("%d",servSock);
    while(true)
    {
        memset(sendInfo.toName,0,sizeof(sendInfo.toName));
        memset(sendInfo.MesContent,0,sizeof(sendInfo.MesContent));
        //char sendBuff[1024] = {0};
        if(!isLogin)
        {
            //send my name to server
            sendInfo.MesType=UserLogin;
            send(servSock, &sendInfo, HEADER_LEN-sizeof(sendInfo.toName), 0);
            isLogin=1;
            usleep(SLEEP_TIME);

        }
        else{
            //just for test
            /* sendInfo.MesType=MesSend;
            strcpy(sendInfo.toName,"eric");
            strcpy(sendInfo.MesContent,"eric");
            send(servSock, &sendInfo, HEADER_LEN+10, 0);
            usleep(SLEEP_TIME*10);
            */
            if(first_time==0){
                first_time=1;
                print_chat_way();

            }
            putchar('$');
            char toName[MAX_NAME_LEN] = {0};
            //char Message[1000] = {0};
        
            scanf("%s", toName);
            if(strlen(toName)==0){
                usleep(SLEEP_TIME);
                continue;
            }
            //printf("toName=%s\n",toName);
            if(strcmp(toName, "all")==0){
                sendInfo.MesType=FetchUser;
                if(send(servSock, &sendInfo, HEADER_LEN-sizeof(sendInfo.toName), 0)<=0){
                     cout<<"message  send  in failure\n";
                }
                usleep(SLEEP_TIME);
                continue ;

            }
            else if(strcmp(toName, "quit")==0){
                sendInfo.MesType=UserLeave;
                if(send(servSock, &sendInfo,  HEADER_LEN-sizeof(sendInfo.toName), 0)<=0){
                     cout<<"message  send  in failure\n";
                }
                usleep(SLEEP_TIME);
                break;
            }
            else{
                sendInfo.MesType=MesSend;
                strcpy(sendInfo.toName, toName);
                // input what do you want to say :
                //gets_s(Message, MAX_MES_LEN);
                getline(cin, Message);
                //cout<<Message<<"  over"<<endl;
                strcpy(sendInfo.MesContent, Message.c_str());
                if(send(servSock, &sendInfo, HEADER_LEN+Message.size(), 0) <= 0){
                    cout<<"message  send  in failure\n";
                }
            }
            usleep(SLEEP_TIME);
            //cout<<Message<<"  over"<<endl;

        }
    }
    printf("\n\n\t%s exit chat room\n", client_name);//\tAll inputs are invalid now\n
    close(servSocket);
    isExit=1;
    //exit(1);
    return true;
}

