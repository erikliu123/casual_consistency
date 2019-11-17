#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <arpa/inet.h> //C can neglect
#include <netinet/in.h>
#include <fcntl.h>
#include <assert.h>
using namespace std;


#define SERVER_PORT 8890
#define CLIENT_DEFAULT_PORT 1234
#define DEFAULT_IP "127.0.0.1"

#define MAX_NAME_LEN 32
#define MAX_MES_LEN 1024
#define PEER_IP_LENGTH 30
#define PEER_STRING_LENGTH 30 

#define SLEEP_TIME 10000 //10ms
#define MAX_DEPENDENT_ID 10000

#define DELIM "#"

//Default Protocol
#define PROTOCOL SOCK_STREAM

#define  HEADER_LEN (sizeof(struct header))
 //==================================
 //for client chat
const short  MesLogin   =  1001;
//const short  MesSend   =  1002;
const short  FetchUser =  1010;

 /*
 ======================================
              Message Type
 ======================================
    login in, user send 
 **************************************
*/
enum MessageHeader{UserConnect=0, UserLogin, MesSend, UserLeave,
     NewUserAdd, ServerDepend, PostMes,
      ASK_ID, ASK_SERVER, ADD_SERVER,
     GET_DEPEND, CHECK_DEPEND,
     REPLICATE_WRITE};
typedef short datacenter_id;
//User Struct
typedef struct UserInfo
{
    //short     userId;
    char      userName[32];
    //struct sockaddr_in clnt_addr;
    int       sockId;
    //long         userAddr;//ipv4 address
    //struct UserInfo     *pNext;
}UserInfo;



//Client and server Message Struct
typedef struct MesInfo
{
    short        MesType;
    union{
        //client interconnect
        struct{
                
                char         fromName[MAX_NAME_LEN];
                char         toName[MAX_NAME_LEN];
                char         MesContent[MAX_MES_LEN];
                short int    chat_timestamp;
        };
        //replicate write
        struct{
            short int timestamp;
            datacenter_id id;
            char username[MAX_NAME_LEN];
            char key[MAX_MES_LEN];
            // dependency replicate write requires
            struct{
             //char check_username[MAX_NAME_LEN];
             short int number;//how many dependencies
             short int depend_timestamps[100];
             datacenter_id depend_centerids[100];

            };
        };
        //Apply to GET depend
        struct{ 
            char get_username[MAX_NAME_LEN];
            short int get_begin_timestamp;
            short int get_check_timestamp;
            //char check_key[MAX_MES_LEN];
        };
        //REPLY depend
       /* struct{
             char check_username[MAX_NAME_LEN];
             short int number;//dependent timestamps
             short int depend_timestamps[100];
             datacenter_id depend_centerids[100];

        };*/

        short int port;
        datacenter_id dc_id;//for NewUser location
        char reserved[MAX_NAME_LEN+MAX_NAME_LEN+MAX_MES_LEN];
    };
    
}MesInfo;

//Server Message
/* typedef struct ServerMesInfo
{
    short        MesType;

}MesInfo;*/
struct header{
    short        MesType;
    char         fromName[MAX_NAME_LEN];
    char         toName[MAX_NAME_LEN];
};


char *client_message[]={
    "I 've lost my keys ",
    "I just found it!",
    "Nice to hear that",
    "bye"
};
void print_usage(char *str){

    printf("Valid Usage: \n%s -a ip_address -p bind_port\n", str);
    
}
void print_client_usage(char *str){
    printf("Valid Usage: \n%s -n your_name -a ip_address -p bind_port\n", str);
    printf("-n can't ommit!\n");
    
}

void print_replicas_usage (char *str){
    printf("Valid Usage: \n%s -p bind_port\n", str);
    printf("-p can't ommit!\n");
}

void outputUser(char *str){
    //printf("str=%s\n",str);
    char *ptr = NULL;
	
    char chBuffer[1024] ;
	
	char *pchStrTmpIn = NULL;
	char *pchTmp = NULL;
	strncpy(chBuffer ,str ,strlen(str) + 1);
    pchTmp = chBuffer;
     printf("Online Users:\n");
	while(NULL != (pchTmp = strtok_r( pchTmp, DELIM, &pchStrTmpIn)))
	{
		printf("%s\n",pchTmp);
	
		pchTmp = NULL;
	}

}

void print_chat_way(){

    printf("=============================================================\n"
        "\tchat example: \n"
        "\tsend message to someone (Tom let's go home) or \n"
        "\tfetch online people (all) or\n"
        "\texit (quit)\n"
           "=============================================================\n");

}

int getpeer_information(int fd, char* ipaddr, char* string_addr)
 {
     struct sockaddr_in name;
     socklen_t namelen = sizeof(struct sockaddr_in);
     struct hostent* result;
 
     assert(fd >= 0);
     assert(ipaddr != NULL);
     assert(string_addr != NULL);
 
     /*
      * Clear the memory.
      */
     memset(ipaddr, '\0', PEER_IP_LENGTH);
     memset(string_addr, '\0', PEER_STRING_LENGTH);
  
    if (getpeername(fd, (struct sockaddr *)&name, &namelen) != 0) {
         return -1;
     } else {
         strncpy(ipaddr,
            inet_ntoa(*(struct in_addr *)&name.sin_addr.s_addr),
             PEER_IP_LENGTH);
        }
  
     result = gethostbyaddr((char *)&name.sin_addr.s_addr, 4, AF_INET);
     if (result) {
         strncpy(string_addr, result->h_name, PEER_STRING_LENGTH);
         return 0;
     } else {
         return -1;
     }
  }

  int getpeer_sockaddr(int fd,  struct sockaddr_in *name)
 {

     socklen_t namelen = sizeof(struct sockaddr_in);
     struct hostent* result;
 
     assert(fd >= 0);
     

    if (getpeername(fd, (struct sockaddr *)name, &namelen) != 0) {
         return -1;
    }
    return 0; 
     
 }

int create_client_port(int listen_port, char *ip_addr) {
	int sock = socket(AF_INET, PROTOCOL, IPPROTO_TCP);
    //printf("sock=%d\n", sock);
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  //fill with dummy 0s
    serv_addr.sin_family = AF_INET;  //ipv4 address
    
    //TODO
    serv_addr.sin_addr.s_addr = INADDR_ANY; //inet_addr(INADDR_ANY);  //LISTEN TO what IP address
    serv_addr.sin_port = htons(listen_port);  //listen port
    bzero(&(serv_addr.sin_zero),8);


    if(bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
        printf("can't bind port [%d]", serv_addr.sin_port);
        exit(1);
        
    }
    //listen clients
    if(listen(sock,20)==-1)
    {
        perror("listen fail");
        exit(1);
    }
    printf(">>>listeninig port %d\n", listen_port);
    return sock;
}

void init_mutex_lock(pthread_mutex_t *mutex){
    	pthread_mutexattr_t mattr;
		int ret;/* initialize an attribute to default value */
		ret = pthread_mutexattr_init(&mattr);
		pthread_mutex_init(mutex, &mattr);
	
}

void analyze(char *str, int n){
    for(int i=0;i<n;++i){
        if(i>8 && i%8==0){
            putchar('\n');
        }
        printf("%x\t", *(str+i)&0xff);
    }
    putchar('\n');
}

