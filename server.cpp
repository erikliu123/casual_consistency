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
#include "servers_protocol.h"
#include "file.h"


void *handle_message(void *data);
int send_servers(MesInfo *mes);

//datacenter_id global_id=0;
//pthread_mutex_t mutex;
void *master_handle_message(void *data);
void *client_thread(void *data);
//map<datacenter_id, int>
pthread_t pid[100];
int main(int argc, char *argv[]){
 
	int sock=create_client_port(SERVER_PORT, NULL);
    hasOpenMyPort=1;
    isLogin=1;
    master_enable=1;
    pthread_t  client_pid;
    int count=0;
    center_id=0;
    init_mutex_lock(&mutex);
    //servers[0]=SERVER_PORT;
         //communicate with clients and other server
    if(pthread_create(&client_pid, NULL, client_thread, (void *)&sock)){
            printf("client thread can't establish!\n", sock);
    }
   
    
    while(!isQuit){
        printf("$");
        string mes;
        cin>>mes;
        
        if(mes=="quit"||mes=="q"){
            isQuit=1;
        }
        else if(mes=="print"){
            //print dependency
            //for()
            printDependency();
        }
    }
    for(int i=0; i<client_count; ++i){
        pthread_join(pid[i], NULL);
    }
    close(sock);
    

    return 0;
}

void *client_thread(void *data){
    int sock=*(int *)data;
    
    while (!isQuit)
    {
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_size = sizeof(clnt_addr);

        socklen_t sock_len;
        int clnt_sock = accept(sock, (struct sockaddr*)&clnt_addr, &sock_len);
        if(clnt_sock==-1){
            usleep(SLEEP_TIME);
            continue ;
        }
        
        if(pthread_create(&pid[client_count++], NULL, handle_message, (void *)&clnt_sock)){
            printf("ERROR: thread can not established on socknum %d!\n", clnt_sock);
        }
        printf(">>>>>accpeted new request, thread established on socknum %d!\n", clnt_sock);
        usleep(SLEEP_TIME);

    }
     close(sock);
}
//handle clients infomation
void *master_handle_message(void *data){
    int clnt_sock=*((int *)data);
    int cnt=0;
    char client_name[40];//client name
    int len;
    MesInfo recvInfo; 
    UserInfo info;
    int stop=0;
    char buf[1024];
    while(!stop){ 
        ++cnt;

        if((len=recv(clnt_sock, &recvInfo, sizeof(recvInfo),0))<=0){
            //printf("len=%d\t",len);
            usleep(SLEEP_TIME/5);
            continue;
        }
        else{
            printf("recvid=%d len=%d \n", recvInfo.MesType,len);
            switch (recvInfo.MesType)
            {
            //ASK_ID  different between master and replicas
            case ASK_ID://new replicas is participate
                pthread_mutex_lock(&mutex);
                ++global_id;
                //recvInfo.port=global_id;
                //memset(recvInfo.fromName, 0, sizeof(struct MesInfo)-sizeof(recvInfo.MesType));
                memcpy(buf, &recvInfo.MesType, sizeof(recvInfo.MesType));
                memcpy(buf+sizeof(recvInfo.MesType), &global_id, sizeof(datacenter_id));
                //printf("global id=%d, port=%d\n",global_id, recvInfo.port);
                send(clnt_sock, buf, sizeof(recvInfo.MesType)+sizeof(datacenter_id), 0);
                
                servers[global_id].fd=clnt_sock;
                servers[global_id].port=recvInfo.port;
                usleep(SLEEP_TIME);
                //send all server's info
                recvInfo.MesType=ADD_SERVER;//
                len=send_servers(&recvInfo);
                pthread_mutex_unlock(&mutex);
                break;

            case UserLogin:
                info.sockId=clnt_sock;
                printf("User [%s]  log in\n",recvInfo.fromName);
                strcpy(client_name, recvInfo.fromName);
                userList[client_name]=clnt_sock;
                userLocation[client_name]=center_id;
                sendUserLocations(client_name);
                break;
            case FetchUser:
                memcpy(buf, &recvInfo.MesType, sizeof(recvInfo.MesType));
                len=getUserNames(buf+2);
                send(userList[recvInfo.fromName], buf, len+2, 0);

                break;
            case UserLeave:
                userList.erase(client_name);
                userLocation.erase(client_name);
                stop=1;
                break;
            case MesSend:
                if(userList.find(recvInfo.toName)!=userList.end()){
                    send(userList[recvInfo.toName], &recvInfo, len, 0);
                    printf("User [%s] --> User [%s]: %s\n",recvInfo.fromName, recvInfo.toName, recvInfo.MesContent);
                }
                else{
                    printf("No user[%s] exists, pass the message other servers", recvInfo.toName);
                    //TODO: PostMes
                    postUserMessage(&recvInfo);   
                }

                break;
            
            //for replicas part
            case NewUserAdd: 
                userLocation[recvInfo.toName]=recvInfo.dc_id;
                printf("<<< get other server's new user info %s %d \n",recvInfo.toName,recvInfo.dc_id);
                break;
            case PostMes:
                recvInfo.MesType=MesSend;
                if(userList.count(recvInfo.toName)==0){
                    ;
                }
                else{
                    send(userList[recvInfo.toName], &recvInfo, sizeof(recvInfo), 0);
                }
                
                break;
            
            

            case ASK_SERVER:
                recvInfo.MesType=ADD_SERVER;//
                len=send_servers(&recvInfo);
                //printf("ask server request receives\n");
                //inform all because new server is added
                for(map<datacenter_id,ServerInfo>:: iterator it=servers.begin(); it!=servers.end(); ++it){
                    send(it->second.fd, &recvInfo, sizeof(recvInfo.MesType)+len, 0);
                }
            default:
                break;
            }
        }
        //write(clnt_sock, str, sizeof(str));
        usleep(SLEEP_TIME);
    }
    close(clnt_sock);

}


