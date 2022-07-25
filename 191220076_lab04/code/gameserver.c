#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

//最大文本行长度
#define MAX_LINE 4096
//服务器监听端口号
#define SERV_PORT 3000
//允许保存的未处理连接请求数目
#define    LISTENQ        1024
#define NUM_THREADS 5

typedef struct Map_Client_* Map_Client;
typedef struct Battle_* Battle;
typedef struct Ranklist_* Ranklist;
enum Client_state{online,request,battle};

struct Map_Client_{
    pthread_t tid;
    char username[12];
    enum Client_state state;
    int sockfd;
    Map_Client next;
};
Map_Client Map = NULL;
char updatename[12];
bool isrefresh = false;
bool log;
pthread_mutex_t mutex_map;

struct Battle_{
    bool player1ready;
    bool player2ready;
    char player1[12];
    int sockfd1;
    int Hp1;
    int move1;
    char player2[12];
    int sockfd2;
    int Hp2;
    int move2;
    bool finish;
    Battle next;
};
Battle allbattle = NULL;
pthread_mutex_t mutex_battle;

struct User_State{
    char username[12];
    enum Client_state state;
};

struct Ranklist_{
    int init;
    char name[12];
    double winrate;
    int wingame;
    int allgame;
};
struct Ranklist_ ranklist[15];
pthread_mutex_t mutex_rank;

struct Log_Packet_data{
    char kind;//00:login 01:logout 02:get broadcast 03:challenge 04:reply to challenge 05:battle 06:send msg 07: get rank
    char name[12];
    char oppname[12];
    char move;// 1 win 2 2 win 3 3 win 1
    char msg[50];
};

struct Server_Packet_data{
    char kind;//00:get info 01:reply to login 02:broadcast 03:challenge name illegal 04:send challenge request 05:send challenge reply 06:battle result 07 send msg 08 send ranklist
    char log;//(01)01 allow 02 false (02) 01 login 02 log out (03) 01 legal 02 illegal (06)finish or not
    char oppname[12];
    char accept;//(05)01 acc 02 not (06)01 win 02 lose 03 draw
    int num;
    struct User_State userstate[15];
    char msg[50];
    struct Ranklist_ rank[15];
};

int cmp(const void* a, const void* b){
    Ranklist temp1 = (Ranklist)a;
    Ranklist temp2 = (Ranklist)b;
    if(temp1->init > temp2->init) return -1;
    else if(temp1->init < temp2->init) return 1;
    else{
        if(temp1->init == 0) return 1;
        else{
            if(temp1->winrate > temp2->winrate) return -1;
            else if(temp1->winrate < temp2->winrate) return 1;
            else{
                if(temp1->wingame >= temp2->wingame) return -1;
                else return 1;
            }
        }
    }
}

void AddPlayer(char* name){
    for(int i=0;i<15;i++){
        if(ranklist[i].init == 0){
            ranklist[i].init = 1;
            ranklist[i].allgame = 0;
            ranklist[i].wingame = 0;
            ranklist[i].winrate = 0;
            strcpy(ranklist[i].name,name);
            break;
        }
    }
}

void ChangeRank(char* name1,char* name2){
    bool find1,find2;
    find1 = false;
    find2 = false;
    for(int i=0;i<15;i++){
        if(strcmp(ranklist[i].name,name1) == 0){
            find1 = true;
            ranklist[i].allgame++;
            ranklist[i].wingame++;
            ranklist[i].winrate = (double)ranklist[i].wingame/ranklist[i].allgame;
        }
        else if(strcmp(ranklist[i].name,name2) == 0){
            find2 = true;
            ranklist[i].allgame++;
            ranklist[i].winrate = (double)ranklist[i].wingame/ranklist[i].allgame;
        } 
        if((find1 && find2) == true) break;  
    }
    qsort(ranklist,15,sizeof(ranklist[0]),cmp);
}

void AddBattle(char* name1,char* name2,int fd1,int fd2){
    Battle node = (Battle) malloc (sizeof(struct Battle_));
    node->player1ready = false;
    node->player2ready = false;
    node->finish = false;
    strcpy(node->player1,name1);
    strcpy(node->player2,name2);
    node->sockfd1 = fd1;
    node->sockfd2 = fd2;
    node->Hp1 = 100;
    node->Hp2 = 100;
    node->next = allbattle;
    node->move1 = 0;
    node->move2 = 0;
    allbattle = node;
}

void SetBattle(char* name,char move){
    Battle node = allbattle;
    while(node != NULL){
        if(strcmp(node->player1,name) == 0){
            node->player1ready = true;
            if(move == 0x01){
                node->move1 = 1;
            }
            else if(move == 0x02){
                node->move1 = 2;
            }
            else if(move == 0x03){
                node->move1 = 3;
            }
            break;
        }
        else if(strcmp(node->player2,name) == 0){
            node->player2ready = true;
            if(move == 0x01){
                node->move2 = 1;
            }
            else if(move == 0x02){
                node->move2 = 2;
            }
            else if(move == 0x03){
                node->move2 = 3;
            }
            break;
        }
        node = node->next;
    }
}

void DeleteMapName(char* name){
    Map_Client node = Map;
    if(node == NULL) return;
    if(strcmp(node->username,name) == 0){
        Map_Client ptr = node;
        Map = Map->next;
        free(ptr);
        return;
    }
    else{
        Map_Client prenode = Map;
        while(node != NULL){
            if(strcmp(name,node->username) == 0){
                Map_Client ptr = node;
                
                prenode->next = node->next;
                free(ptr);
            }
            prenode = node;
            node = node->next;
        }
    }
}

void DeleteBattleName(char* name){
    Battle node = allbattle;
    if(node == NULL) return;
    if(strcmp(node->player1,name) == 0){
        Battle ptr = node;
        allbattle = allbattle->next;
        free(ptr);
        return;
    }
    else{
        Battle prenode = allbattle;
        while(node != NULL){
            if(strcmp(name,node->player1) == 0){
                Battle ptr = node;
                prenode->next = node->next;
                free(ptr);
                break;
            }
            prenode = node;
            node = node->next;
        }
    }
}

bool CheckMapName(char* name){
    Map_Client node = Map;
    while(node != NULL){
        if(strcmp(name,node->username) == 0)
            return true;
        node = node->next;
    }
    return false;
}

bool IsLegalChallenge(char* name){
    Map_Client node = Map;
    while(node != NULL){
        if(strcmp(name,node->username) == 0 && node->state == online)
            return true;
        node = node->next;
    }
    return false;
}

void ChangeState(char* name,int kind){
    Map_Client node = Map;
    while(node != NULL){
        if(strcmp(name,node->username) == 0)
        {
            if(kind == 0)
                node->state = online;
            else if(kind == 1)
                node->state = request;
            else if(kind == 2)
                node->state = battle;
            break;
        }
        node = node->next;
    }
}

int GetSocketfd(char* name){
    Map_Client node = Map;
    while(node != NULL){
        if(strcmp(name,node->username) == 0){
            return node->sockfd;
        }
        node = node->next;
    }
    return 0;
}

void listen_client(int fd) {
    char recvbuffer[MAX_LINE];
    char sendbuffer[MAX_LINE];
    int recvfd;
    char username[12];
    while(recvfd = recv(fd, recvbuffer, MAX_LINE,0) > 0){
        struct Log_Packet_data recv_data;
        memset(&recv_data, 0 , sizeof(struct Log_Packet_data));
        memcpy(&recv_data,recvbuffer,sizeof(struct Log_Packet_data));
        if(recv_data.kind == 0x00){
            pthread_mutex_lock(&mutex_map);
            if(CheckMapName(recv_data.name) == true){
                isrefresh = false;
                struct Server_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
                packet.kind = 0x01;
                packet.log = 0x02;
                memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
                send(fd,sendbuffer,sizeof(sendbuffer),0);
            }
            else{
                Map_Client newnode = (Map_Client) malloc (sizeof(struct Map_Client_));
                newnode->tid = pthread_self();
                newnode->sockfd = fd;
                newnode->state = online;
                strcpy(newnode->username,recv_data.name);
                newnode->next = Map;
                Map = newnode;
                printf("player %s logs in\n",recv_data.name);
                strcpy(username,recv_data.name);
                isrefresh = true;
                struct Server_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
                packet.kind = 0x01;
                packet.log = 0x01;
                memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
                strcpy(updatename,username);
                log = true;
                send(fd,sendbuffer,sizeof(sendbuffer),0);
                pthread_mutex_lock(&mutex_rank);
                AddPlayer(username);
                pthread_mutex_unlock(&mutex_rank);
            }
            pthread_mutex_unlock(&mutex_map);
        }//login
        else if(recv_data.kind == 0x01){
            pthread_mutex_lock(&mutex_map);
            printf("player %s logs out\n",recv_data.name);
            strcpy(username,recv_data.name);
            strcpy(updatename,recv_data.name);
            isrefresh = true;
            log = false;
            DeleteMapName(username);
            memset(username,0,12);
            pthread_mutex_unlock(&mutex_map);
        }//logout
        else if(recv_data.kind == 0x02){
            printf("player %s wants all information\n",recv_data.name);
            struct Server_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
            packet.kind = 0x00;
            pthread_mutex_lock(&mutex_map);
            Map_Client node = Map;
            int count = 0;
            while(node != NULL){
                strcpy(packet.userstate[count].username,node->username);
                packet.userstate[count].state = node->state;
                node = node->next;
                count++;
            }
            packet.num = count;
            pthread_mutex_unlock(&mutex_map);
            memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
            send(fd,sendbuffer,sizeof(sendbuffer),0);
        }
        else if(recv_data.kind == 0x03){
            printf("player %s wants to challenge player %s\n",recv_data.name,recv_data.oppname);
            bool succeedchallenge;
            pthread_mutex_lock(&mutex_map);
            ChangeState(recv_data.name,1);
            succeedchallenge = IsLegalChallenge(recv_data.oppname);
            pthread_mutex_unlock(&mutex_map);
            if( succeedchallenge == true){
                ChangeState(recv_data.oppname,1);
                printf("send challenge request to player %s\n",recv_data.oppname);
                struct Server_Packet_data packet1;
                memset(&packet1, 0x00 , sizeof(struct Server_Packet_data));
                packet1.kind = 0x03;
                packet1.log = 0x01;
                memcpy(sendbuffer,&packet1,sizeof(struct Server_Packet_data));
                send(fd,sendbuffer,sizeof(sendbuffer),0);
                memset(sendbuffer,0,MAX_LINE);
                int oppsockfd = GetSocketfd(recv_data.oppname);
                struct Server_Packet_data packet2;
                memset(&packet2, 0x00 , sizeof(struct Server_Packet_data));
                packet2.kind = 0x04;
                strcpy(packet2.oppname,username);
                memcpy(sendbuffer,&packet2,sizeof(struct Server_Packet_data));
                send(oppsockfd,sendbuffer,sizeof(sendbuffer),0);
                memset(sendbuffer,0,MAX_LINE);
            }
            else{
                printf("player %s cannot be challenged\n",recv_data.oppname);
                struct Server_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
                packet.kind = 0x03;
                packet.log = 0x02;
                memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
                send(fd,sendbuffer,sizeof(sendbuffer),0);
            }
        }
        else if(recv_data.kind == 0x04){
            if(recv_data.move == 0x01){
                printf("player %s accepts challenge from player %s\n",recv_data.name,recv_data.oppname);
                pthread_mutex_lock(&mutex_map);
                ChangeState(recv_data.name,2);
                ChangeState(recv_data.oppname,2);
                int oppsockfd = GetSocketfd(recv_data.oppname);
                pthread_mutex_unlock(&mutex_map);
                struct Server_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
                packet.kind = 0x05;
                packet.accept = 0x01;
                strcpy(packet.oppname,recv_data.name);
                memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
                send(oppsockfd,sendbuffer,sizeof(sendbuffer),0);
                pthread_mutex_lock(&mutex_battle);
                AddBattle(recv_data.name,recv_data.oppname,fd,oppsockfd);
                pthread_mutex_unlock(&mutex_battle);
            }
            else if(recv_data.move == 0x02){
                printf("player %s rejects challenge from player %s\n",recv_data.name,recv_data.oppname);
                pthread_mutex_lock(&mutex_map);
                ChangeState(recv_data.name,0);
                ChangeState(recv_data.oppname,0);
                int oppsockfd = GetSocketfd(recv_data.oppname);
                pthread_mutex_unlock(&mutex_map);
                struct Server_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
                packet.kind = 0x05;
                packet.accept = 0x02;
                strcpy(packet.oppname,recv_data.name);
                memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
                send(oppsockfd,sendbuffer,sizeof(sendbuffer),0);
            }
        }
        else if(recv_data.kind == 0x05){
            pthread_mutex_lock(&mutex_battle);
            SetBattle(recv_data.name,recv_data.move);
            pthread_mutex_unlock(&mutex_battle);
        }
        else if(recv_data.kind == 0x06){
            printf("player %s wants to send message to player %s\n",recv_data.name,recv_data.oppname);
            bool canchat;
            pthread_mutex_lock(&mutex_map);
            canchat = CheckMapName(recv_data.oppname);
            pthread_mutex_unlock(&mutex_map);
            if(canchat == true){
                printf("send message to player %s\n",recv_data.oppname);
                int oppsockfd = GetSocketfd(recv_data.oppname);
                struct Server_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
                packet.kind = 0x07;
                strcpy(packet.oppname,username);
                strcpy(packet.msg,recv_data.msg);
                memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
                send(oppsockfd,sendbuffer,sizeof(sendbuffer),0);
            }
            else{
                printf("player %s does not exist\n",recv_data.oppname);
            }
        }
        else if(recv_data.kind = 0x07){
            printf("player %s wants ranklist\n",recv_data.name);
            struct Server_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Server_Packet_data));
            packet.kind = 0x08;
            pthread_mutex_lock(&mutex_rank);
            for(int i=0;i<15;i++){
                if(ranklist[i].init == 1){
                    packet.rank[i].init = ranklist[i].init;
                    packet.rank[i].wingame = ranklist[i].wingame;
                    packet.rank[i].allgame = ranklist[i].allgame;
                    packet.rank[i].winrate = ranklist[i].winrate;
                    strcpy(packet.rank[i].name,ranklist[i].name);
                }
            }
            pthread_mutex_unlock(&mutex_rank);
            memcpy(sendbuffer,&packet,sizeof(struct Server_Packet_data));
            send(fd,sendbuffer,sizeof(sendbuffer),0);
        }
        else{
            printf("wrong format\n");
        }
        memset(sendbuffer,0,MAX_LINE);
        memset(recvbuffer,0,MAX_LINE);
    }
    if (recvfd == 0 || recvfd == -1){
        if(strlen(username) != 0){
        printf("user %s has accidentally disconnected\n",username);
        pthread_mutex_lock(&mutex_map);
        isrefresh = true;
        strcpy(updatename,username);
        log = false;
        DeleteMapName(username);
        memset(username,0,12);
        pthread_mutex_unlock(&mutex_map);
        }
    }
}

int tcp_server_listen(int port) {
    int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    int rt1 = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rt1 < 0) {
        printf("bind failed ");
    }

    int rt2 = listen(listenfd, LISTENQ);
    if (rt2 < 0) {
        printf("listen failed ");
    }

    signal(SIGPIPE, SIG_IGN);

    return listenfd;
}

void thread_run(void *arg) {
    pthread_detach(pthread_self());
    int fd = (int) arg;
    
    listen_client(fd);
}

void thread_broadcast(){
    while(1){
    pthread_mutex_lock(&mutex_map);
    char sendbuffer[MAX_LINE];
    if(isrefresh == true){
        struct Server_Packet_data packet;
        memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
        packet.kind = 0x02;
        if(log == true) packet.log = 0x01;
        else packet.log = 0x02;
        strcpy(packet.oppname,updatename);
        memcpy(sendbuffer,&packet,sizeof(struct Log_Packet_data));
        Map_Client node = Map;
        while(node != NULL){
            if(strcmp(node->username,updatename) == 0){
            }
            else{
                send(node->sockfd,sendbuffer,sizeof(sendbuffer),0);
            }
            node = node->next;
        }
    }
    isrefresh = false;
    memset(sendbuffer,0,MAX_LINE);
    pthread_mutex_unlock(&mutex_map);
    sleep(1);
    }
}

void thread_battle(){
    bool isfinish;
    char sendbuffer[MAX_LINE];
    while(true){
        pthread_mutex_lock(&mutex_battle);
        if(allbattle == NULL)
            isfinish = true;
        else
            isfinish = false;
        pthread_mutex_unlock(&mutex_battle);
        if(isfinish == true)
            sleep(2);
        else{
            int whowin;
            pthread_mutex_lock(&mutex_battle);
            Battle node = allbattle;
            while(node != NULL){
                char tempname[12];
                char tempoppname[12];
                bool needremove = false;
                if(node->player1ready == true && node->player2ready == true){
                    strcpy(tempname,node->player1);
                    strcpy(tempoppname,node->player2);
                    if(node->move1 == 1){
                        if(node->move2 == 2){
                            printf("player %s wins, player %s loses\n",node->player1,node->player2);
                            node->Hp2 = node->Hp2 - 50;
                            whowin = 1;
                        }
                        else if(node->move2 == 3){
                            printf("player %s wins, player %s loses\n",node->player2,node->player1);
                            node->Hp1 = node->Hp1 - 50;
                            whowin = 2;
                        }
                        else{
                            printf("draw\n");
                            whowin = 0;
                        }
                    }
                    else if(node->move1 == 2){
                        if(node->move2 == 3){
                            printf("player %s wins, player %s loses\n",node->player1,node->player2);
                            node->Hp2 = node->Hp2 - 50;
                            whowin = 1;
                        }
                        else if(node->move2 == 1){
                            printf("player %s wins, player %s loses\n",node->player2,node->player1);
                            node->Hp1 = node->Hp1 - 50;
                            whowin = 2;
                        }
                        else{
                            printf("draw\n");
                            whowin = 0;
                        }
                    }
                    else if(node->move1 == 3){
                        if(node->move2 == 1){
                            printf("player %s wins, player %s loses\n",node->player1,node->player2);
                            node->Hp2 = node->Hp2 - 50;
                            whowin = 1;
                        }
                        else if(node->move2 == 2){
                            printf("player %s wins, player %s loses\n",node->player2,node->player1);
                            node->Hp1 = node->Hp1 - 50;
                            whowin = 2;
                        }
                        else{
                            printf("draw\n");
                            whowin = 0;
                        }
                    }
                    if(node->Hp1 == 0 || node->Hp2 == 0) node->finish = true;
                    node->move1 = 0;
                    node->move2 = 0;
                    node->player1ready = false;
                    node->player2ready = false;
                    struct Server_Packet_data packet1,packet2;
                    memset(&packet1, 0x00 , sizeof(struct Server_Packet_data));
                    memset(&packet2, 0x00 , sizeof(struct Server_Packet_data));
                    packet1.kind = 0x06;
                    packet2.kind = 0x06;
                    if(whowin == 1){
                        packet1.accept = 0x01;
                        packet2.accept = 0x02;
                    }
                    else if(whowin == 2){
                        packet2.accept = 0x01;
                        packet1.accept = 0x02;
                    }
                    else{
                        packet1.accept = 0x03;
                        packet2.accept = 0x03;
                    }
                    if(node->finish == true){
                        packet1.log = 0x01;
                        packet2.log = 0x01;
                        needremove = true;
                        if(whowin == 1){
                            printf("player %s finally wins\n",node->player1);
                            pthread_mutex_lock(&mutex_rank);
                            ChangeRank(node->player1,node->player2);
                            pthread_mutex_unlock(&mutex_rank);
                        }
                        else if(whowin == 2){
                            printf("player %s finally wins\n",node->player2);
                            pthread_mutex_lock(&mutex_rank);
                            ChangeRank(node->player2,node->player1);
                            pthread_mutex_unlock(&mutex_rank);
                        }
                    }
                    memcpy(sendbuffer,&packet1,sizeof(struct Server_Packet_data));
                    send(node->sockfd1,sendbuffer,sizeof(sendbuffer),0);
                    memset(sendbuffer,0,MAX_LINE);
                    memcpy(sendbuffer,&packet2,sizeof(struct Server_Packet_data));
                    send(node->sockfd2,sendbuffer,sizeof(sendbuffer),0);
                    memset(sendbuffer,0,MAX_LINE);
                }
                node = node->next;
                if(needremove){
                    DeleteBattleName(tempname);
                    pthread_mutex_lock(&mutex_map);
                    ChangeState(tempname,0);
                    ChangeState(tempoppname,0);
                    pthread_mutex_unlock(&mutex_map);
                }
            }
            pthread_mutex_unlock(&mutex_battle);
        }
    }
}

int main (int argc, char **argv)
{
    pthread_t tid;
    pthread_t broadcast;
    pthread_t battle;
    pthread_mutex_init(&mutex_map,NULL);
    pthread_mutex_init(&mutex_battle,NULL);
    pthread_mutex_init(&mutex_rank,NULL);
    int listener_fd = tcp_server_listen(SERV_PORT);
    printf("%s\n","Server running...waiting for connections.");
    pthread_mutex_lock(&mutex_rank);
    for(int i=0;i<15;i++)
        ranklist[i].init = 0;
    pthread_mutex_unlock(&mutex_rank);
    pthread_create(&broadcast, NULL, (void*)&thread_broadcast, NULL);
    pthread_create(&battle, NULL, (void*)&thread_battle, NULL);
    while (1) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        int fd = accept(listener_fd, (struct sockaddr *) &ss, &slen);
        if (fd < 0) {
            printf("error connect");
        } else {
            pthread_create(&tid, NULL, (void*)&thread_run, (void*) fd);
        }
    }
    pthread_mutex_destroy(&mutex_map);
    pthread_mutex_destroy(&mutex_battle);
    pthread_mutex_destroy(&mutex_rank);
}