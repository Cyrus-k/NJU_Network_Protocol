#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>

#define MAXLINE 4096
#define SERV_PORT 3000 /*port*/

char username[];
char opponame[] = "\0";
bool isrename = true;
pthread_mutex_t mutex_rename;

bool legalchallenge = false;
pthread_mutex_t mutex_challenge;
pthread_mutex_t mutex_listen_read;

bool bechallenged = false;
pthread_mutex_t mutex_bechalllenge;

int inbattle = 1;
pthread_mutex_t mutex_battle;

bool battlefinish = false;
pthread_mutex_t mutex_finish;

int Hp = 100;
pthread_mutex_t mutex_hp;

enum Client_state{online,request,battle};
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

struct Log_Packet_data{
    char kind;//00:login 01:logout 02:get broadcast 03:challenge 04:reply to challenge 05:battle 06:send msg 07: get rank
    char name[12];
    char oppname[12];
    char move; //(04) 01 accept 02 donot // 1 win 2 2 win 3 3 win 1
    char msg[50];
};

struct Server_Packet_data{
    char kind;//00:get info 01:reply to login 02:broadcast 03:challenge name illegal 04:send challenge request
    char log;//(01)01 allow 02 false (02) 01 login 02 log out (03) 01 legal 02 illegal
    char oppname[12];
    char accept;
    int num;
    struct User_State userstate[15];
    char msg[50];
    struct Ranklist_ rank[15];
};

void PageChall(int sockfd){
    system("clear");
    printf("you are in battle now\n");
    printf("you have three choices to make,\n'scissors' stands for scissors,\n'paper' stands for paper,\n'stone' stands for stone,\nchoose one to move\n");
    pthread_mutex_lock(&mutex_hp);
    Hp = 100;
    pthread_mutex_unlock(&mutex_hp);
    while(true){
        char inputstr[MAXLINE] = "\0";
        char sendline[MAXLINE];
        gets(inputstr);
        if(strcmp(inputstr,"scissors") == 0){
            struct Log_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
            packet.kind = 0x05;
            strcpy(packet.name, username);
            packet.move = 0x01;
            memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
            send(sockfd,sendline,sizeof(sendline),0);
            printf("waiting judgement\n");
            pthread_mutex_lock(&mutex_listen_read);
            pthread_mutex_lock(&mutex_finish);
            if(battlefinish == true){
                pthread_mutex_unlock(&mutex_finish);
                pthread_mutex_unlock(&mutex_listen_read);
                
                break;
            }
            battlefinish = false;
            pthread_mutex_unlock(&mutex_finish);
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else if(strcmp(inputstr,"paper") == 0){
            struct Log_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
            packet.kind = 0x05;
            strcpy(packet.name, username);
            packet.move = 0x02;
            memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
            send(sockfd,sendline,sizeof(sendline),0);
            printf("waiting judgement\n");
            pthread_mutex_lock(&mutex_listen_read);
            pthread_mutex_lock(&mutex_finish);
            if(battlefinish == true){
                pthread_mutex_unlock(&mutex_finish);
                pthread_mutex_unlock(&mutex_listen_read);
                
                break;
            }
            battlefinish = false;
            pthread_mutex_unlock(&mutex_finish);
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else if(strcmp(inputstr,"stone") == 0){
            struct Log_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
            packet.kind = 0x05;
            strcpy(packet.name, username);
            packet.move = 0x03;
            memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
            send(sockfd,sendline,sizeof(sendline),0);
            printf("waiting judgement\n");
            pthread_mutex_lock(&mutex_listen_read);
            pthread_mutex_lock(&mutex_finish);
            if(battlefinish == true){
                pthread_mutex_unlock(&mutex_finish);
                pthread_mutex_unlock(&mutex_listen_read);
                
                break;
            }
            battlefinish = false;
            pthread_mutex_unlock(&mutex_finish);
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else   
            printf("illegal move\n");
        memset(sendline,0,sizeof(sendline));
    }
}

void PageLogin(int sockfd){
    printf("input your name(your name should not be longer than 10 words)\n");
    printf("User name:\n");
    while(true){
        char inputstr[MAXLINE] = "\0";
        char sendline[MAXLINE];
        gets(inputstr);
        if(strlen(inputstr) > 0 && strlen(inputstr) <= 10){
            strcpy(username,inputstr);
            struct Log_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
            packet.kind = 0x00;
            strcpy(packet.name, username);
            memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
            if(send(sockfd,sendline,sizeof(sendline),0))
            {
                printf("send login\n");
            }
            pthread_mutex_lock(&mutex_listen_read);
            pthread_mutex_lock(&mutex_rename);
            if(isrename == false){
                pthread_mutex_unlock(&mutex_rename);
                pthread_mutex_unlock(&mutex_listen_read);
            
                break;
            }
            isrename = true;
            pthread_mutex_unlock(&mutex_rename);
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else    
            printf("illegal input\n");
        memset(sendline,0,sizeof(sendline));
        
    }
}

void PageBattle(int sockfd){
    printf("Hello %s\n",username);
    printf("if you want to see players' state, input 'get';\nif you want to challenge,input 'challenge';\nif you want to log out,input 'quit';\nif you want to clear,input 'clear'\n");
    printf("if you want to send message, input 'chat'\n");
    printf("if you want to send message, input 'getrank'\n");
    while(true){
        bool bechallen;
        char inputstr[MAXLINE] = "\0";
        char sendline[MAXLINE];
        char oppname[12];
        gets(inputstr);
        pthread_mutex_lock(&mutex_bechalllenge);
        bechallen = bechallenged;
        strcpy(oppname,opponame);
        pthread_mutex_unlock(&mutex_bechalllenge);
        if(strcmp(inputstr,"get") == 0 && bechallen == false){
            struct Log_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
            packet.kind = 0x02;
            strcpy(packet.name, username);
            memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
            if(send(sockfd,sendline,sizeof(sendline),0))
            {
                printf("send get information\n");
            }
        }
        else if(strcmp(inputstr,"challenge") == 0 && bechallen == false){
            printf("please input your opponent name(no longer than 10 words)\n");
            while(true){
                char challengestr[MAXLINE] = "\0";
                gets(challengestr);
                if(strlen(challengestr) > 0 && strlen(challengestr) <= 10 && strcmp(challengestr,username) != 0){
                    struct Log_Packet_data packet;
                    memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
                    packet.kind = 0x03;
                    strcpy(packet.name, username);
                    strcpy(packet.oppname,challengestr);
                    memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
                    if(send(sockfd,sendline,sizeof(sendline),0))
                    {
                        printf("send challenge request\n");
                    }
                    pthread_mutex_lock(&mutex_listen_read);
                    pthread_mutex_lock(&mutex_challenge);
                    if(legalchallenge == true){
                        pthread_mutex_unlock(&mutex_challenge);
                        pthread_mutex_unlock(&mutex_listen_read);
                        break;
                    }
                    legalchallenge = false;
                    pthread_mutex_unlock(&mutex_challenge);
                    pthread_mutex_unlock(&mutex_listen_read);
                }
                else
                    printf("illegal input\n");
            }
            printf("waiting reply\n");
            int localbattle;
            while(true){     
                pthread_mutex_lock(&mutex_battle);
                localbattle = inbattle;
                pthread_mutex_unlock(&mutex_battle);
                
                if(localbattle !=1) break;
            }
            if(localbattle == 2){
                PageChall(sockfd);
                printf("Hello %s\n",username);
                printf("if you want to see players' state, input 'get';\nif you want to challenge,input 'challenge';\nif you want to log out,input 'quit';\nif you want to clear,input 'clear'\n");
                printf("if you want to send message, input 'chat'\n");
                printf("if you want to send message, input 'getrank'\n");
            }
            
            pthread_mutex_lock(&mutex_battle);
            inbattle = 1;
            pthread_mutex_unlock(&mutex_battle);
        }
        else if(strcmp(inputstr,"quit") == 0 && bechallen == false){
            struct Log_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
            packet.kind = 0x01;
            strcpy(packet.name, username);
            memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
            if(send(sockfd,sendline,sizeof(sendline),0))
            {
                printf("send logout\n");
            }
            break;
        }
        else if(strcmp(inputstr,"clear") == 0 && bechallen == false){
            system("clear");
            printf("if you want to see players' state, input 'get';\nif you want to challenge,input 'challenge';\nif you want to log out,input 'quit';\nif you want to clear,input 'clear'\n");
            printf("if you want to send message, input 'chat'\n");
            printf("if you want to send message, input 'getrank'\n");
            continue;
        }
        else if((strcmp(inputstr,"yes") == 0 || strcmp(inputstr,"no") == 0) && bechallen == true){
            if(strcmp(inputstr,"yes") == 0){
                struct Log_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
                packet.kind = 0x04;
                packet.move = 0x01;
                strcpy(packet.name, username);
                strcpy(packet.oppname,oppname);
                memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
                if(send(sockfd,sendline,sizeof(sendline),0))
                {
                    printf("send challenge reply\n");
                }
                pthread_mutex_lock(&mutex_battle);
                inbattle = 2;
                pthread_mutex_unlock(&mutex_battle);
                pthread_mutex_lock(&mutex_bechalllenge);
                bechallenged = false;
                pthread_mutex_unlock(&mutex_bechalllenge);
                PageChall(sockfd);
                
                printf("Hello %s\n",username);
                printf("if you want to see players' state, input 'get';\nif you want to challenge,input 'challenge';\nif you want to log out,input 'quit';\nif you want to clear,input 'clear'\n");
                printf("if you want to send message, input 'chat'\n");
                printf("if you want to send message, input 'getrank'\n");            
            }
            else if(strcmp(inputstr,"no") == 0){
                struct Log_Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
                packet.kind = 0x04;
                packet.move = 0x02;
                strcpy(packet.name, username);
                strcpy(packet.oppname,oppname);
                memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
                if(send(sockfd,sendline,sizeof(sendline),0))
                {
                    printf("send challenge reply\n");
                }
                pthread_mutex_lock(&mutex_battle);
                inbattle = 3;
                pthread_mutex_unlock(&mutex_battle);
                pthread_mutex_lock(&mutex_bechalllenge);
                bechallenged = false;
                pthread_mutex_unlock(&mutex_bechalllenge);
            }

        }
        else if(strcmp(inputstr,"chat") == 0 && bechallen == false){
            printf("please input player name you want to send(no longer than 10 words)\n");
            while(true){
                char challengestr[MAXLINE] = "\0";
                gets(challengestr);
                if(strlen(challengestr) > 0 && strlen(challengestr) <= 10 && strcmp(challengestr,username) != 0){
                    struct Log_Packet_data packet;
                    memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
                    packet.kind = 0x06;
                    strcpy(packet.name, username);
                    strcpy(packet.oppname,challengestr);
                    printf("please input your message,remember no longer than 45 words\n");
                    char msgbuffer[MAXLINE] = "\0";
                    gets(msgbuffer);
                    strcpy(packet.msg,msgbuffer);
                    memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
                    if(send(sockfd,sendline,sizeof(sendline),0))
                    {
                        printf("send msg\n");
                    }
                    break;
                }
                else
                    printf("illegal name\n");
            }
        }
        else if(strcmp(inputstr,"getrank") == 0 && bechallen == false){
            struct Log_Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Log_Packet_data));
            packet.kind = 0x07;
            strcpy(packet.name, username);
            memcpy(sendline,&packet,sizeof(struct Log_Packet_data));
            if(send(sockfd,sendline,sizeof(sendline),0))
            {
                printf("send get rank\n");
            }
        }
        else    
            printf("illegal input\n");
        memset(sendline,0,sizeof(sendline));
    }
}

void thread_listen(void* arg){
    int sockfd = (int)arg;
    char recvline[MAXLINE];
    int recvfd;
    pthread_mutex_lock(&mutex_listen_read);
    while(recvfd = recv(sockfd, recvline, MAXLINE,0) > 0){ 
        struct Server_Packet_data recv_data;
        memset(&recv_data, 0 , sizeof(struct Server_Packet_data));
        memcpy(&recv_data,recvline,sizeof(struct Server_Packet_data));
        if(recv_data.kind == 0x01){
            if(recv_data.log == 0x01){
                system("clear");
                printf("you have successfully logged in\n");
                
                pthread_mutex_lock(&mutex_rename);
                isrename = false;
                pthread_mutex_unlock(&mutex_rename);
                pthread_mutex_unlock(&mutex_listen_read);
            }
            else if(recv_data.log == 0x02){
                printf("this name has existed already\n");
                printf("User name:\n");
                pthread_mutex_lock(&mutex_rename);
                isrename = true;
                pthread_mutex_unlock(&mutex_rename);
                pthread_mutex_unlock(&mutex_listen_read);
            }
        }//login
        else if(recv_data.kind == 0x00){
            printf("               Name       State\n");
            for(int i = 0;i < recv_data.num;i++){
                printf("Player%d        ",i+1);
                printf("%s      ",recv_data.userstate[i].username);
                switch (recv_data.userstate[i].state)
                {
                case online:
                    printf("online\n");
                    break;
                case battle:
                    printf("battle\n");
                    break;
                case request:
                    printf("request\n");
                    break;
                default:
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_listen_read);
        }//get info
        else if(recv_data.kind == 0x02){
            if(recv_data.log == 0x01){
                printf("Player %s logs in\n",recv_data.oppname);
            }
            else if(recv_data.log == 0x02){
                printf("Player %s logs out\n",recv_data.oppname);
            }
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else if(recv_data.kind == 0x03){
            if(recv_data.log == 0x01){
                printf("legal challenge name\n");
                pthread_mutex_lock(&mutex_challenge);
                legalchallenge = true;
                pthread_mutex_unlock(&mutex_challenge);
            }
            else if(recv_data.log == 0x02){
                printf("cannot challenge,please input again\n");
                pthread_mutex_lock(&mutex_challenge);
                legalchallenge = false;
                pthread_mutex_unlock(&mutex_challenge);
            }
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else if(recv_data.kind == 0x04){
            printf("player %s wants to challenge you\n",recv_data.oppname);
            pthread_mutex_lock(&mutex_bechalllenge);
            bechallenged = true;
            strcpy(opponame,recv_data.oppname);
            pthread_mutex_unlock(&mutex_bechalllenge);
            pthread_mutex_unlock(&mutex_listen_read);     
        }
        else if(recv_data.kind == 0x05){
            if(recv_data.accept == 0x01){
                printf("player %s accepts your challenge\n",recv_data.oppname);
                pthread_mutex_lock(&mutex_battle);
                inbattle = 2;
                pthread_mutex_unlock(&mutex_battle);
            }
            else if(recv_data.accept == 0x02){
                printf("player %s rejects your challenge\n",recv_data.oppname);
                pthread_mutex_lock(&mutex_battle);
                inbattle = 3;
                pthread_mutex_unlock(&mutex_battle);
            }
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else if(recv_data.kind == 0x06){
            if(recv_data.accept == 0x01){
                printf("you win this turn\n");
                printf("your HP is %d now\n",Hp);
            }
            else if(recv_data.accept == 0x02){
                printf("you lose this turn\n");
                pthread_mutex_lock(&mutex_hp);
                Hp = Hp - 50;
                pthread_mutex_unlock(&mutex_hp);
                printf("your HP is %d now\n",Hp);
            }
            else if(recv_data.accept == 0x03){
                printf("draw this turn\n");
                printf("your HP is %d now\n",Hp);
            }
            if(recv_data.log == 0x01){
                printf("battle finish!!!!!!!!!!!\n");
                if(recv_data.accept == 0x01){
                    printf("you win this battle\n");
                }
                else if(recv_data.accept == 0x02){
                    printf("you lose this battle\n");
                }
                pthread_mutex_lock(&mutex_finish);
                battlefinish = true;
                pthread_mutex_unlock(&mutex_finish);
            }
            else{
                pthread_mutex_lock(&mutex_finish);
                battlefinish = false;
                pthread_mutex_unlock(&mutex_finish);
            }
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else if(recv_data.kind == 0x07){
            printf("player %s sends you a message: %s\n",recv_data.oppname,recv_data.msg);
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else if(recv_data.kind == 0x08){
            printf("               Name         Winrate         Wingame         Allgame\n");
            for(int i=0;i<15;i++){
                if(recv_data.rank[i].init == 1){
                    printf("Player%d        ",i+1);
                    printf("%s         %.2f%%             %d             %d\n",recv_data.rank[i].name,recv_data.rank[i].winrate*100,recv_data.rank[i].wingame,recv_data.rank[i].allgame);
                }
            }
            pthread_mutex_unlock(&mutex_listen_read);
        }
        else{

        }

        memset(recvline, 0, MAXLINE);
        sleep(1);
        pthread_mutex_lock(&mutex_listen_read);
    }
    if (recvfd == 0 || recvfd == -1){
        printf("server closed!\n");
    }
}

void thread_send(void* arg){
    int sockfd = (int)arg;
    char sendline[MAXLINE];
    printf("Welcome\n");
    while(true){
        char inputstr[MAXLINE];
        printf("if u want to login in, input \"login\", if u want to exit, input \"exit\"\n");
        gets(inputstr);
        if(strcmp(inputstr,"login") == 0){
            PageLogin(sockfd);
            //system("clear");
            PageBattle(sockfd);
            system("clear");
        }
        else if(strcmp(inputstr,"exit") == 0){
            printf("thank you for using\n");
            char string[10] = {0};
            sprintf(string,"%d",sockfd);
            char str[] = "connect ";
            strcat(str,string);
            strcpy(sendline,str);
            if(send(sockfd,sendline,sizeof(sendline),0)){
                close(sockfd);
            }
            break;
        }
        else{
            printf("illegal input,please input again\n");
        }
        memset(sendline, 0, MAXLINE);
        //pthread_mutex_lock(&mutex_listen_read);
        //pthread_mutex_unlock(&mutex_listen_read);
    }
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;
    
    
    if (argc != 2) {
        perror("Usage: TCPClient <IP address of the server>");
        exit(1);
    }
    sockfd = socket (AF_INET, SOCK_STREAM, 0);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr= inet_addr(argv[1]);
    servaddr.sin_port = htons(SERV_PORT);
    
    pthread_t listen;
    pthread_t send;
    pthread_mutex_init(&mutex_rename,NULL);
    pthread_mutex_init(&mutex_listen_read,NULL);
    pthread_mutex_init(&mutex_challenge,NULL);
    pthread_mutex_init(&mutex_bechalllenge,NULL);
    pthread_mutex_init(&mutex_battle,NULL);
    pthread_mutex_init(&mutex_finish,NULL);
    pthread_mutex_init(&mutex_hp,NULL);
    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    
    pthread_create(&send, NULL, (void*)&thread_send, (void*)sockfd);
    pthread_create(&listen, NULL, (void*)&thread_listen, (void*)sockfd);

    pthread_join(send, NULL);
    //pthread_join(tid2, NULL);
    pthread_mutex_destroy(&mutex_rename);
    pthread_mutex_destroy(&mutex_listen_read);
    pthread_mutex_destroy(&mutex_challenge);
    pthread_mutex_destroy(&mutex_bechalllenge);
    pthread_mutex_destroy(&mutex_battle);
    pthread_mutex_destroy(&mutex_finish);
    pthread_mutex_destroy(&mutex_hp);
    return 0;
}