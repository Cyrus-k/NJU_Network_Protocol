#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define MAX_SIZE 1024
#define DEST_PORT 4321
#define DEST_IP "47.105.85.28"

int Flag = 1;
char* name;
enum Weather{overcast = 0x00,sunny = 0x01,cloudy = 0x02,rain = 0x03,fog = 0x04,rainstorm = 0x05,thunderstorm = 0x06,breeze = 0x07,snow = 0x08};

struct Packet_data{
    char step;
    char days;
    char data[30];
    char day;
};

struct Recv_data
{
    char head[2];
    char data[30];
    char weather[20];
    char tail[75];
};

void Page2toPage1(int sockfd);

void getweather(char* p, int kind){
    //printf("%x %d %x %x %x %x\n",p[0],p[1],p[2],p[3],p[4],p[5]);
    enum Weather nowweather = overcast;
    int year = (int)p[0]*16*16 + (int)p[1] + 256;
    printf("City: %s  Today is: %d/%02d/%02d  Weather information is as follows:\n",name,year,(int)p[2],(int)p[3]);
    switch (p[5])
    {
    case overcast:
        if(kind == 1)
            printf("Today's Weather is: overcast;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: overcast;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: overcast;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case sunny:
        if(kind == 1)
            printf("Today's Weather is: sunny;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: sunny;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: sunny;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case cloudy:
        if(kind == 1)
            printf("Today's Weather is: cloudy;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: cloudy;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: cloudy;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case rain:
        if(kind == 1)
            printf("Today's Weather is: rain;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: rain;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: rain;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case fog:
        if(kind == 1)
            printf("Today's Weather is: fog;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: fog;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: fog;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case rainstorm:
        if(kind == 1)
            printf("Today's Weather is: rainstorm;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: rainstorm;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: rainstorm;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case thunderstorm:
        if(kind == 1)
            printf("Today's Weather is: thunderstorm;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: thunderstorm;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: thunderstorm;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case breeze:
        if(kind == 1)
            printf("Today's Weather is: breeze;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: breeze;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: breeze;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    case snow:
        if(kind == 1)
            printf("Today's Weather is: sandstorm;  Temp:%d\n",(int)p[6]);
        else if(kind == 2)
            printf("The 1th day's Weather is: sandstorm;  Temp:%d\n",(int)p[6]);
        else
            printf("The %dth day's Weather is: sandstorm;  Temp:%d\n",(int)p[4],(int)p[6]);
        break;
    default:
        break;
    }
    if (kind == 2){
        switch (p[7])
        {
            case overcast:
                printf("The 2th day's Weather is: overcast;  Temp:%d\n",(int)p[8]);
                break;
            case sunny:
                printf("The 2th day's Weather is: sunny;  Temp:%d\n",(int)p[8]);
                break;
            case cloudy:
                printf("The 2th day's Weather is: cloudy;  Temp:%d\n",(int)p[8]);
                break;
            case rain:
                printf("The 2th day's Weather is: rain;  Temp:%d\n",(int)p[8]);
                break;
            case fog:
                printf("The 2th day's Weather is: fog;  Temp:%d\n",(int)p[8]);
                break;
            case rainstorm:
                printf("The 2th day's Weather is: rainstorm;  Temp:%d\n",(int)p[8]);
                break;
            case thunderstorm:
                printf("The 2th day's Weather is: thunderstorm;  Temp:%d\n",(int)p[8]);
                break;
            case breeze:
                printf("The 2th day's Weather is: breeze;  Temp:%d\n",(int)p[8]);
                break;
            case snow:
                printf("The 2th day's Weather is: sandstorm;  Temp:%d\n",(int)p[8]);
                break;
            default:
                break;
        }
        switch (p[9])
        {
            case overcast:
                printf("The 3th day's Weather is: overcast;  Temp:%d\n",(int)p[10]);
                break;
            case sunny:
                printf("The 3th day's Weather is: sunny;  Temp:%d\n",(int)p[10]);
                break;
            case cloudy:
                printf("The 33th day's Weather is: cloudy;  Temp:%d\n",(int)p[10]);
                break;
            case rain:
                printf("The 3th day's Weather is: rain;  Temp:%d\n",(int)p[10]);
                break;
            case fog:
                printf("The 3th day's Weather is: fog;  Temp:%d\n",(int)p[10]);
                break;
            case rainstorm:
                printf("The 3th day's Weather is: rainstorm;  Temp:%d\n",(int)p[10]);
                break;
            case thunderstorm:
                printf("The 3th day's Weather is: thunderstorm;  Temp:%d\n",(int)p[10]);
                break;
            case breeze:
                printf("The 3th day's Weather is: breeze;  Temp:%d\n",(int)p[10]);
                break;
            case snow:
                printf("The 3th day's Weather is: sandstorm;  Temp:%d\n",(int)p[10]);
                break;
            default:
                break;
        }
    }
}

void Page1toPage2(int sockfd){
    char recvbuff[MAX_SIZE];
    char sendbuff[MAX_SIZE];
    system("clear");
    printf("Please enter the given number to query\n1.today\n2.three days from today\n3.custom day by yourself\n(r)back,(c)cls,(#)exit\n===================================================\n");
    while(1){
        char* input;
        char input_sen[MAX_SIZE];
        gets(input_sen);
        if(strlen(input_sen)>0){
        input = strtok(input_sen," ");
        if(input!=NULL){
        if(strcmp(input,"1") == 0){
            memset(sendbuff,0,sizeof(sendbuff));
            struct Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Packet_data));
            packet.step = 0x02;
            packet.days = 0x01;
            packet.day = 0x01;
            strcpy(packet.data, name);
            memcpy(sendbuff,&packet,sizeof(struct Packet_data));
            if(send(sockfd,sendbuff,sizeof(sendbuff),0))
            {
                    //printf("%s\n",packet.data);
                //printf("send2 succeed\n");
            }
            memset(recvbuff,0,sizeof(recvbuff));
            int recvfd = recv(sockfd, recvbuff, MAX_SIZE,0);
            if (recvfd == 0 || recvfd == -1){
                printf("error receive!\n");
            }
            else{
                struct Recv_data recv_data;
                memset(&recv_data, 0 , sizeof(struct Recv_data));
                memcpy(&recv_data,recvbuff,sizeof(struct Recv_data));
                //printf("%x , %x",recv_data.head[0],recv_data.head[1]);
                if(recv_data.head[0] == 0x03 && recv_data.head[1] == 0x41){
                    getweather(recv_data.weather,1);
                }
                else{
                    printf("wrong packet\n");
                }
            }
        }
        else if(strcmp(input,"2") == 0){
            memset(sendbuff,0,sizeof(sendbuff));
            struct Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Packet_data));
            packet.step = 0x02;
            packet.days = 0x02;
            packet.day = 0x03;
            strcpy(packet.data, name);
            memcpy(sendbuff,&packet,sizeof(struct Packet_data));
            if(send(sockfd,sendbuff,sizeof(sendbuff),0))
            {
                    //printf("%s\n",packet.data);
                //printf("send2 succeed\n");
            }
            memset(recvbuff,0,sizeof(recvbuff));
            int recvfd = recv(sockfd, recvbuff, MAX_SIZE,0);
            if (recvfd == 0 || recvfd == -1){
                printf("error receive!\n");
            }
            else{
                struct Recv_data recv_data;
                memset(&recv_data, 0 , sizeof(struct Recv_data));
                memcpy(&recv_data,recvbuff,sizeof(struct Recv_data));
                //printf("%x , %x",recv_data.head[0],recv_data.head[1]);
                if(recv_data.head[0] == 0x03 && recv_data.head[1] == 0x42){
                    getweather(recv_data.weather,2);
                }
                else{
                    printf("wrong packet\n");
                }
            }

        }
        else if(strcmp(input,"3") == 0){
            char* input_day;
            while(1){
                printf("Please enter the day number(below 10, e.g. 1 means today):");
                char input_str[MAX_SIZE];
                gets(input_str);
                if(strlen(input_str)>0){
                    input_day = strtok(input_str," ");
                    if(input_day != NULL){
                    if(input_day[0] >= '1' && input_day[0] <= '9' && strlen(input_day)<=1){
                        //printf("right\n");
                        break;
                    }
                    else{
                        printf("Input error\n");
                    }
                    }
                }
            }
            memset(sendbuff,0,sizeof(sendbuff));
            struct Packet_data packet;
            memset(&packet, 0x00 , sizeof(struct Packet_data));
            packet.step = 0x02;
            packet.days = 0x01;
            packet.day = input_day[0] - '0';
            strcpy(packet.data, name);
            memcpy(sendbuff,&packet,sizeof(struct Packet_data));
            if(send(sockfd,sendbuff,sizeof(sendbuff),0))
            {
                //printf("send2 succeed\n");
            }
            int recvfd = recv(sockfd, recvbuff, MAX_SIZE,0);
            if (recvfd == 0 || recvfd == -1){
                printf("error receive!\n");
            }
            else{
                struct Recv_data recv_data;
                memset(&recv_data, 0 , sizeof(struct Recv_data));
                memcpy(&recv_data,recvbuff,sizeof(struct Recv_data));
                //printf("%x , %x",recv_data.head[0],recv_data.head[1]);
                if(recv_data.head[0] == 0x03 && recv_data.head[1] == 0x41){
                    //printf("nice recv\n");
                    getweather(recv_data.weather,3);
                }
                else{
                    printf("wrong packet\n");
                }
            }
        }
        else if(strcmp(input,"c") == 0){
            system("clear");
            printf("Please enter the given number to query\n1.today\n2.three days from today\n3.custom day by yourself\n(r)back,(c)cls,(#)exit\n===================================================\n");
            continue;
        }
        else if(strcmp(input,"#") == 0){
            if (close(sockfd) == 0){
                //printf("connect close\n");
            }
            break;
        }
        else if(strcmp(input,"r") == 0){
            Page2toPage1(sockfd);
            break;
        }
        else{
            printf("input error!\n");
            continue;
        }
        }
        }
    }
}

void Page2toPage1(int sockfd){
    system("clear");
    char recvbuffer[MAX_SIZE];
        char sendbuffer[MAX_SIZE];
        printf("Welcome to NJUCS Weather Forecast Demo Program!\n");
        printf("Please input City Name in Chinese pinyin(e.g. nanjing or beijing)\n");
        printf("(c)cls,(#)exit\n");
        while(1){
            char input_str[MAX_SIZE];
            gets(input_str);
            if(strlen(input_str)>0){
            name = strtok(input_str," ");
            if(name != NULL){
            //scanf("%s",name);
            //printf("%s\n",name);
            //printf("%d\n",sizeof(nanjing));
            if (strcmp(name,"c") == 0){
                system("clear");
                printf("Welcome to NJUCS Weather Forecast Demo Program!\n");
                printf("Please input City Name in Chinese pinyin(e.g. nanjing or beijing)\n");
                printf("(c)cls,(#)exit\n");
                continue;
            }
            else if(strcmp(name,"#") == 0){
                if (close(sockfd) == 0){
                    //printf("connect close\n");
                }
                break;
            }
            else{
                memset(sendbuffer,0,sizeof(sendbuffer));
                struct Packet_data packet;
                memset(&packet, 0x00 , sizeof(struct Packet_data));
                packet.step = 0x01;
                strcpy(packet.data, name);
                //printf("%d\n",sizeof(*packet));
                //printf("%s\n",packet->data);
                //printf("%p\n",packet);
                memcpy(sendbuffer,&packet,sizeof(struct Packet_data));
                if(send(sockfd,sendbuffer,sizeof(sendbuffer),0))
                {
                    //printf("%s\n",packet.data);
                    //printf("send succeed\n");
                }
                memset(recvbuffer,0,sizeof(recvbuffer));
                int recvfd = recv(sockfd, recvbuffer, MAX_SIZE,0);
                if (recvfd == 0 || recvfd == -1){
                    printf("error receive!\n");
                }
                else{
                    //printf("recv = %d\n",recvfd);
                    struct Recv_data recv_data;
                    memset(&recv_data, 0 , sizeof(struct Recv_data));
                    memcpy(&recv_data,recvbuffer,sizeof(struct Recv_data));
                    //printf("%x\n",recv_data.head[0]);
                    
                    //printf("%x\n",recv_data.data[0]);
                    if(recv_data.head[0] == 0x02){
                        printf("Sorry, Server does not have weather information for city %s!\n",recv_data.data);
                        printf("Welcome to NJUCS Weather Forecast Demo Program!\n");
                        printf("Please input City Name in Chinese pinyin(e.g. nanjing or beijing)\n");
                        printf("(c)cls,(#)exit\n");
                    }
                    else{
                        Flag = 2;
                        Page1toPage2(sockfd);
                        break;
                    }
                }
            }
            }
            }
        }
}

int main(int argc, char* argv[]){
    int sockfd;
    struct sockaddr_in servaddr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DEST_PORT);
    servaddr.sin_addr.s_addr= inet_addr(DEST_IP);
    int connect_rt = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (connect_rt < 0){
        printf("connect failed\n");
    }
    else{
        Page2toPage1(sockfd);
    }
    return 0;
}