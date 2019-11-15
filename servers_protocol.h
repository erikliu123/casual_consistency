#include <string>
#include <arpa/inet.h> //C can neglect
#include <netinet/in.h>
#include <map>
#include <vector>
#include <set>
#include <iterator>
#include <string.h>
#include <algorithm>
#include <pthread.h>
#include "protocol.h"

using namespace std;


/* 
#define FINDUSER "F"  //F#(NAME)#
#define ADD_DEPEND "A"//A#(KEY)#(TIMESTAMP, 4 bit)#datacenter id#
#define CHECK_DEPEND "C"//C#key#

#define ASK_ID "I"
#define ADD_SERVER "N"
*/
#define MAX_READ_LEN 1024*2
#define WRITE 1
#define READ 0
struct Dependency
{
    string key;
    string value;//neglect in chat
    int timestamp;
    datacenter_id id;
    int isWrite;//write dependency is what we focus on
     bool operator < (const Dependency &b)const{
        return timestamp<b.timestamp;
    }
};

struct MessageItem{

    short int timestamp;
    datacenter_id id;
    char username[MAX_NAME_LEN];
    char key[MAX_MES_LEN];//conversation

    bool operator < (const MessageItem &b)const{
        return timestamp<b.timestamp;
    }
};
struct dependRecord{
    int timestamp;
    datacenter_id id;
    bool operator <(const dependRecord &b)const{
        if(timestamp!=b.timestamp) timestamp<b.timestamp;
        else return id<b.id;
    }

};
//for each remote user
struct CommitMessage{
    int currentTimeStamp;
    int maxTimeStamp;
    set<dependRecord> dependTimeStamp;//required timestamp
    vector<Dependency> waitCommit;
    vector<Dependency> blockQueue;

};

struct ClientDepend{
    //string client_name;
    int localTime;
    vector<Dependency> dep;
    
};
map<string, CommitMessage> checkList;//username  -> waitqueue/commitqueue
map<string, ClientDepend > dependList;//username -> dependency list

//server infomation, mainly for replicate write consistency
struct ServerInfo{
    //struct sockaddr_in servaddr;
    int fd;//file descriptor
    int port;//server's open port
    short int curtime;
    set<short int>timestamps;  //very important to judge dependency requirements
    bool operator ==(const ServerInfo &b)const{
        return fd==b.fd && port==b.port;
    }

};
//======================================
//Global Varibles
//vector<MessageItem> commitQueue, waitQueue;
map<datacenter_id, ServerInfo> servers;
map<string,int>userList; 
map<string,datacenter_id>userLocation; 
int current_time=0;
pthread_t server_pid[1000];
int pid_count=0;
datacenter_id center_id;

datacenter_id global_id=0;//only master can use, allocate replica's ids
pthread_mutex_t mutex;

int isLogin=0;
int hasOpenMyPort=0;
short int myport;
int master_sockfd;
int master_enable;
//=====================
//function definition

void *handle_message(void *data);
void *server_message(void *data);//communicate with master and other replicas
int connect_server(struct sockaddr_in *addr, int port);
void *read_other_server_message(void *data);


int getUserNames(char *str){
    int len=0;
    for(map<string,datacenter_id>::iterator it=userLocation.begin(); it!=userLocation.end(); ++it){      
        strcpy(str+len, it->first.c_str());
        len+=strlen(it->first.c_str());
        strcpy(str+len, DELIM);
        ++len;

    }
    return len;
}
void addQueue(MesInfo *mes){
    dependRecord records;
    for(int i=0; i<mes->number; ++i){
        records.timestamp=mes->depend_timestamps[i];
        records.id=mes->depend_centerids[i];
        checkList[mes->username].dependTimeStamp.insert(records);
    }
}
void commitMessage(char *usr){
    //checkList[usr].waitCommit
}

void commit(char *usrname){
    //just 
    int can_commit=1;
    sort(checkList[usrname].blockQueue.begin(), checkList[usrname].blockQueue.end());

    for(set<dependRecord>::iterator it=checkList[usrname].dependTimeStamp.begin(); it!=checkList[usrname].dependTimeStamp.end();++it){
        //if(servers[checkList[usrname].blockQueue[i].id].timestamps.find(blockQueue[i].id))
        if(servers[it->id].curtime >= it->timestamp){
            ;
        }
        else{
            can_commit=0;
            break;
        }
    }
    if(can_commit){
        printf(">>>>>>>>>can commit!\n");
        for(int i=0;i<checkList[usrname].blockQueue.size(); ++i){
            printf("\t\t%s\n",checkList[usrname].blockQueue[i].key);
        }

    }

}

void sendUserLocations(char *str){
    short int temp=NewUserAdd;
    MesInfo sendInfo; 
    sendInfo.MesType=NewUserAdd;
    sendInfo.dc_id=center_id;

    strcpy(sendInfo.toName, str);
    //printf("%s", )
    for(map<datacenter_id, ServerInfo>::iterator it=servers.begin(); it!=servers.end(); ++it){       
        
        if(it->first == center_id) continue;
        send(it->second.fd, &sendInfo, sizeof(sendInfo), 0);
    }
    return ;
}
void updateServerCurtime(ServerInfo *server){
    int i=server->curtime+1;
    for(;i<65534;++i){       
        if(server->timestamps.find(i)!=server->timestamps.end()){
            continue;
        }
        
    }
    server->curtime=i;

}
//post message to other servers
void postUserMessage(MesInfo *recvInfo){
    
    MesInfo sendInfo; 
    assert(recvInfo != NULL);
    memcpy(&sendInfo, recvInfo, sizeof(sendInfo));
    sendInfo.MesType=PostMes;
    if(userLocation.find(recvInfo->toName)!=userLocation.end()){
        send(servers[userLocation[recvInfo->toName]].fd, &sendInfo, sizeof(sendInfo), 0);
    }
    else{
        printf("----can't post message!\----n");
    }
    return ;
}


//buf should mov 2 bytes when called
void add_servers(char *buf, int len){
    //fomat portnumber(16 bits)+datecenter id(16 bit)+struct sockaddr_in;
    int begin=0;
    struct sockaddr_in addr;
    printf("buf=%x, len=%d\n", buf, len);
    while(begin<len){
        short int port;
        datacenter_id dataid;
        memcpy(&port, buf+begin, sizeof(port));
         printf("port=%d\n", port);
        assert(port>80);
        memcpy(&dataid, buf+begin+sizeof(port), sizeof(datacenter_id));
        memcpy(&addr, buf+begin+sizeof(port)+sizeof(datacenter_id), sizeof(sockaddr_in));
        if(servers.find(dataid)!=servers.end() && servers[dataid].port==port){
            begin+=(sizeof(port)+sizeof(datacenter_id)+sizeof(sockaddr_in));
            continue;
        }
        //add new server
        servers[dataid].port=port;
        servers[dataid].timestamps.clear();
        servers[dataid].curtime=0;
        //connect to new server
        if(dataid!=center_id){// don't connect myself{
            int fd=connect_server(&addr, port);
            servers[dataid].fd=fd;//must record for replicate_write
        }
        begin+=(sizeof(port)+sizeof(datacenter_id)+sizeof(sockaddr_in));
    }

}

int send_servers(MesInfo *mes){
    int len=0;
    struct sockaddr_in addr;
    assert(mes!=NULL);
    mes->MesType=ADD_SERVER;
    ////fomat: portnumber(16 bits)+datecenter id(16 bit)+struct sockaddr_in;
    for(map<datacenter_id, ServerInfo>:: iterator it=servers.begin(); it!=servers.end(); ++it){
        short int port=it->second.port;
        datacenter_id dataid=it->first;
        memcpy(mes->reserved+len, &port, sizeof(short int));
        memcpy(mes->reserved+len+sizeof(short int), &dataid, sizeof(datacenter_id));
        printf("%d %d\n",port,dataid);
        getpeer_sockaddr(it->second.fd, &addr);
        memcpy(mes->reserved+len + sizeof(datacenter_id) + sizeof(short int), &addr, sizeof(addr));
        len+=sizeof(short int)+sizeof(addr) + sizeof(datacenter_id);

    }

    return len;   
}

int send_replicate_write(Dependency client_dep, char *username){
    
    MesInfo *mes=(MesInfo *)malloc(sizeof(MesInfo));
    assert(mes!=NULL);
    
    mes->MesType=REPLICATE_WRITE;
    mes->id=center_id;
    mes->timestamp=client_dep.timestamp;
    strcpy(mes->username, username);
    strcpy(mes->key,client_dep.key.c_str());
    
    for(map<datacenter_id, ServerInfo>:: iterator it=servers.begin(); it!=servers.end(); ++it){
        if(it->first != center_id)
            send(it->second.fd, mes, sizeof(MesInfo), 0);

    }
    free(mes);
    return 0;
    
}
int connect_server(struct sockaddr_in *servaddr, int port){
    int sock = socket(AF_INET, PROTOCOL, 0);
    
    servaddr->sin_port=htons(port);
    servaddr->sin_family=AF_INET;
    //servaddr->sin_addr.s_addr = inet_addr(DEFAULT_IP); 
    if(connect(sock, (struct sockaddr*)servaddr, sizeof(struct sockaddr_in)) < 0)  
    {  
        printf("can not connect to server, exit!\n");   
        exit(1);  
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);//non block
    printf("connect to port [%d] successfully\n", port); //servaddr->sin_port is peer port
    if(pthread_create(&server_pid[pid_count++], NULL, handle_message, (void *)&sock)){
            printf("Client Dump: thread can not established on socknum %d!\n", sock);
    }
    return sock;
}

//read from server, useless, merge into Func handle_message
void *read_other_server_message(void *data){
    int sock=*((int *)data);
    MesInfo recvInfo; 
    char buf[MAX_READ_LEN];
    while(true){
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
//get from other servers 
int getDependency(MessageItem *item){
    MesInfo mes;
    mes.MesType=GET_DEPEND;
    //add item to waitCommit
    if(checkList.find(item->username)==checkList.end()){
        CommitMessage temp;
        temp.currentTimeStamp=0;
        temp.maxTimeStamp=item->timestamp;
        temp.waitCommit.clear();
        checkList[item->username]=temp;
    }
    
    mes.get_begin_timestamp=checkList[item->username].currentTimeStamp;
    mes.get_check_timestamp=item->timestamp;
    strcpy(mes.get_username, item->username);
    send(servers[item->id].fd, &mes, sizeof(MesInfo), 0);

}
//local record user's behavior
Dependency addDependency(MesInfo *recvInfo, int iswrite){
    assert(recvInfo!=NULL);
    Dependency client_dep;
    client_dep.id=center_id;
    string strs=string(recvInfo->fromName)+":";
    client_dep.key=strs+recvInfo->MesContent;
    if(iswrite){   
        client_dep.isWrite=WRITE;
        client_dep.timestamp=current_time+1; //dependList[recvInfo->fromName].localTime+1;
        //write message
        dependList[recvInfo->fromName].localTime=++current_time;//update current timestamp
        dependList[recvInfo->fromName].dep.push_back(client_dep);
    }else{
        //read meassage
        client_dep.isWrite=READ;
        client_dep.timestamp=current_time;
        dependList[recvInfo->toName].localTime=current_time;//update current timestamp
        dependList[recvInfo->toName].dep.push_back(client_dep);
    }
    return client_dep;
}
void replyDependency(MesInfo *recvInfo){
    string name=string(recvInfo->get_username);
    MesInfo mesinfo;
    memset(&mesinfo, 0, sizeof(mesinfo));
    mesinfo.MesType=CHECK_DEPEND;
    strcpy(mesinfo.check_username, recvInfo->get_username);
    int len=0;
    for(vector<Dependency>::iterator it=dependList[name].dep.begin(); it!=dependList[name].dep.end();++it){
        if(/*it->isWrite==WRIIE && */it->timestamp>recvInfo->get_begin_timestamp && it->timestamp <= recvInfo->get_check_timestamp){
            mesinfo.depend_timestamps[mesinfo.number]=it->timestamp;
            mesinfo.depend_centerids[mesinfo.number++]=it->id;
        }
    }
    memcpy(recvInfo, &mesinfo, sizeof(mesinfo));

}

//handle local clients infomation

void *handle_message(void *data){
    int clnt_sock=*((int *)data);
    int cnt=0;
    char client_name[40];//client name
    int len;
    MesInfo recvInfo; 
    UserInfo info;
    int stop=0;
    char buf[MAX_READ_LEN];
    //vector<MessageItem>  temp; 
    vector<Dependency> temp;
    Dependency client_dep;
    MessageItem message_item;
    char *buf_pointer=(char *)&recvInfo;
    while(!hasOpenMyPort || !isLogin){
        if(!hasOpenMyPort){
            usleep(SLEEP_TIME);
            continue;
        }
         //++cnt;
        if(!isLogin)
        {
            //send my name to server
            /* strcpy(buf, ASK_ID);
            memcpy(buf+1, &myport, sizeof(myport));
            send(master_sock, buf, 1+sizeof(myport), 0);*/
            recvInfo.MesType=ASK_ID;
            recvInfo.port=myport;

            send(master_sockfd, &recvInfo, sizeof(recvInfo.MesType)+sizeof(recvInfo.port), 0);
            isLogin=1;
            usleep(SLEEP_TIME);
            continue ;

        }
    }
    while(!stop){ 
        ++cnt;
        if((len=recv(clnt_sock, &recvInfo, sizeof(recvInfo),0))<=0){
            //printf("len=%d\t",len);
            usleep(SLEEP_TIME/5);
            continue;
        }
        else{
            printf("recvid=%d len=%d \n", recvInfo.MesType,len);
            string strs;
            switch (recvInfo.MesType)
            {
            case ASK_ID://new replicas is participate
            if(master_enable){
                //pthread_mutex_lock(&mutex);
                ++global_id;
                //recvInfo.port=global_id;
                //memset(recvInfo.fromName, 0, sizeof(struct MesInfo)-sizeof(recvInfo.MesType));
                
                servers[global_id].fd=clnt_sock;
                servers[global_id].port=recvInfo.port;
                //printf("global id=%d, port=%d\n",global_id, recvInfo.port);
                memcpy(buf_pointer, &recvInfo.MesType, sizeof(recvInfo.MesType));
                memcpy(buf_pointer+sizeof(recvInfo.MesType), &global_id, sizeof(datacenter_id));
                send(clnt_sock, buf_pointer, sizeof(recvInfo.MesType)+sizeof(datacenter_id), 0);
                
                usleep(SLEEP_TIME);
                //send all server's info
                recvInfo.MesType=ADD_SERVER;//
                len=send_servers(&recvInfo);
                //pthread_mutex_unlock(&mutex);
                }
                else{
                    //index=buf;
                    //memcpy(&center_id, index+2, sizeof(center_id));
                    center_id=recvInfo.port;      
                    //analyze(index, 4);
                    //recvInfo.port=myport;  
                    recvInfo.MesType=ASK_SERVER;//retrieve others IP and ports
                    send(master_sockfd, &recvInfo, sizeof(recvInfo.MesType), 0);
                     
                }
                break;
            case UserLogin:
                info.sockId=clnt_sock;
                printf("User [%s]  log in\n",recvInfo.fromName);
                strcpy(client_name, recvInfo.fromName);
                //userList.push_back()
                userList[client_name]=clnt_sock;
                userLocation[client_name]=center_id;
                temp.clear();
                dependList[client_name].localTime=0;
                dependList[client_name].dep=temp;
                sendUserLocations(client_name);
                break;
            case FetchUser:
                memcpy(buf, &recvInfo.MesType, sizeof(recvInfo.MesType));
                len=getUserNames(buf+2);
                send(userList[recvInfo.fromName], buf, len+2, 0);

                break;
            case UserLeave:
                userList.erase(client_name);
                //TODO: inform other server
                printf("User [%s] is offline\n",client_name);
                stop=1;
                break;
            case MesSend://dependency just happens here, and repliacte write
                //build up dependency relation
                //TODO: mutex lock
                client_dep=addDependency(&recvInfo, WRITE);
                if(userList.find(recvInfo.toName)!=userList.end()){
                    
                    send(userList[recvInfo.toName], &recvInfo, len, 0);
                    addDependency(&recvInfo, READ);
                    //dependency update, read
                    printf("User [%s] --> User [%s]: %s\n",recvInfo.fromName, recvInfo.toName, recvInfo.MesContent);
                }
                else{
                    printf("No user[%s] exists, pass the message other servers", recvInfo.toName);
                    //TODO:
                    postUserMessage(&recvInfo);
                }
                //TODO:inform other server to replicate
                send_replicate_write(client_dep, recvInfo.fromName);
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
                    addDependency(&recvInfo, READ);
                    send(userList[recvInfo.toName], &recvInfo, sizeof(recvInfo), 0);
                }
                break;
            
            case ASK_SERVER:
                if(master_enable){
                    recvInfo.MesType=ADD_SERVER;//
                    len=send_servers(&recvInfo);
                    //printf("ask server request receives\n");
                    //inform all because new server is added
                    for(map<datacenter_id,ServerInfo>:: iterator it=servers.begin(); it!=servers.end(); ++it){
                        send(it->second.fd, &recvInfo, sizeof(recvInfo.MesType)+len, 0);
                    }
                }
                 
                break;
            case ADD_SERVER:
                 add_servers(buf_pointer+2, len-sizeof(recvInfo.MesType));
                break;
            //dependency part
            case GET_DEPEND:
                //get user, key, timestamp, then get it's depenentcy 
                recvInfo.MesType=CHECK_DEPEND;
                replyDependency(&recvInfo);
                send(clnt_sock, &recvInfo, sizeof(recvInfo), 0);
                break;
            case CHECK_DEPEND:
                //check and commit
                addQueue(&recvInfo);
                commit(recvInfo.check_username);


                break;

            case REPLICATE_WRITE:
                //message_item.id=recvInfo.id;
                //strcpy(message_item.key, recvInfo.key);
                //strcpy(message_item.username, recvInfo.username);
                //message_item.timestamp=recvInfo.timestamp;
                client_dep.key=string(recvInfo.key);
                client_dep.id=recvInfo.id;
                client_dep.isWrite=WRITE;
                client_dep.timestamp=recvInfo.timestamp;
                if(recvInfo.timestamp == current_time + 1){// satisfy
                   ++current_time;
                   checkList[recvInfo.username].blockQueue.push_back(client_dep);
                   //commitQueue.push_back(message_item);
                   //commit write
                   printf(">>>>>>>>>> (time,datacenterid)<%d,%d> %s\n\t\t Commit\n",recvInfo.timestamp,recvInfo.id,recvInfo.key);
                }
                
                else{
                    //current_time=max(current_time, recvInfo.timestamp);
                    //ask dependency lists
                    //TODO: optimize, unecessary to send it every time
                    getDependency(&message_item);
                    ////check key, recvInfo.timestamp < current_time 
                    printf(">>>>>>>>>> (time,datacenterid)<%d,%d> %s\n\t\t Not satisfy dependency, wait\n",message_item.timestamp,message_item.id,message_item.key);
                    //waitQueue.push_back(message_item);
                }
                servers[client_dep.id].timestamps.insert(client_dep.timestamp);
                updateServerCurtime(&servers[client_dep.id]);
                break;
            
            default:
                break;
            }
        }
        //write(clnt_sock, str, sizeof(str));
        usleep(SLEEP_TIME);

    }
    close(clnt_sock);

}



//rule with replicas and master
void *server_message(void *data){
    int master_sock=*((int *)data);
    int cnt=0;
    char buf[MAX_READ_LEN];
    MesInfo recvInfo; 
    string sendStr;
    char *index;
    while(!hasOpenMyPort || !isLogin){
        if(!hasOpenMyPort){
            usleep(SLEEP_TIME);
            continue;
        }
         ++cnt;
        if(!isLogin)
        {
            //send my name to server
            /* strcpy(buf, ASK_ID);
            memcpy(buf+1, &myport, sizeof(myport));
            send(master_sock, buf, 1+sizeof(myport), 0);*/
            recvInfo.MesType=ASK_ID;
            recvInfo.port=myport;

            send(master_sock, &recvInfo, sizeof(recvInfo.MesType)+sizeof(recvInfo.port), 0);
            isLogin=1;
            usleep(SLEEP_TIME);
            continue ;

        }
    }
    while(1){
        int n=recv(master_sock, buf, sizeof(buf), 0);
        if(n<=0){
            usleep(SLEEP_TIME);
            continue;
        }
        else{
            printf("recvid=%d,len=%d\n",recvInfo.MesType, n);
            memcpy(&recvInfo, buf, n);
            switch(recvInfo.MesType){
                
                case ASK_ID:
                    index=buf;
                    memcpy(&center_id, index+2, sizeof(center_id));
                    //center_id=recvInfo.port;      
                    //analyze(index, 4);
                    recvInfo.MesType=ASK_SERVER;//retrieve others IP and ports
                    send(master_sock, &recvInfo, sizeof(recvInfo.MesType), 0);
                     
                    break;
                case ASK_SERVER:

                    break;

                case ADD_SERVER://port number(16),
                    add_servers(buf+2, n-sizeof(recvInfo.MesType));
                    break;
                case NewUserAdd:
                    userLocation[recvInfo.toName]=recvInfo.dc_id;
                    printf("<<< get other server's new user info %s %d \n",recvInfo.toName,recvInfo.dc_id);
                    break;
                case PostMes://from other domain's message, send message to local user
                    recvInfo.MesType=MesSend;
                    if(userList.count(recvInfo.toName)==0){
                        ;
                    }
                    else{
                        //TODO:dependency update
                        send(userList[recvInfo.toName], &recvInfo, sizeof(recvInfo), 0);
                    }
                    
                break;

            }
        
        }
        
        usleep(SLEEP_TIME);

    }
    //wait server thread complete
    for(int i=0; i<pid_count; ++i){
        pthread_join(server_pid[i], NULL);
    }

}
