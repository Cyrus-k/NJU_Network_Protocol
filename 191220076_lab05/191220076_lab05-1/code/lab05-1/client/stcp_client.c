#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include "stcp_client.h"

/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
bool isSYNACK = false;
bool isFINACK = false;

void stcp_client_init(int conn) {
  pthread_t seghandle;
  printf("hello\n");
  for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    memset(&ClientTCB[i], 0 , sizeof(client_tcb_t));
    ClientTCB[i].is_used = NOT_USED;
    pthread_mutex_t* sendBuf_mutex;
	  sendBuf_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	  assert(sendBuf_mutex!=NULL);
	  pthread_mutex_init(sendBuf_mutex,NULL);
    ClientTCB[i].bufMutex = sendBuf_mutex;
  }
  Sockfd = conn;
  printf("start seghandler\n");
  pthread_create(&seghandle, NULL, (void*)seghandler, NULL);
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
  int ret = -1;
  for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if(ClientTCB[i].is_used == NOT_USED){
      ret = i;
      ClientTCB[i].state = CLOSED;
      ClientTCB[i].client_portNum = client_port;
      ClientTCB[i].is_used = USED;
      break;
    }
  }
  return ret;
}

int gettcb(unsigned int client_port){
  int ret = -1;
  for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if(ClientTCB[i].client_portNum == client_port && ClientTCB[i].is_used == USED) return i;
  }
  return ret;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, unsigned int server_port) {
  printf("ready to STCP connect\n");
  if(ClientTCB[sockfd].is_used == NOT_USED) return -1;
  int retry_count = 0;
  ClientTCB[sockfd].server_portNum = server_port;
  seg_t SYNpacket;
  memset(&SYNpacket, 0x00 , sizeof(seg_t));
  SYNpacket.header.src_port = ClientTCB[sockfd].client_portNum;
  SYNpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
  SYNpacket.header.type = SYN;
  SYNpacket.header.length = 24;
  printf("sending SYN packet\n");
  printf("SRCPORT = %d,DESTPORT = %d\n",SYNpacket.header.src_port,SYNpacket.header.dest_port);
  sip_sendseg(Sockfd,&SYNpacket);
  ClientTCB[sockfd].state = SYNSENT;
  clock_t start;
  start = clock();
  while(retry_count <= SYN_MAX_RETRY){
    if((clock() - start)*1000000 >= SYN_TIMEOUT){
      start = clock();
      pthread_mutex_lock(&mutex_syn);
      if(isSYNACK == false){
        pthread_mutex_unlock(&mutex_syn);
        printf("timeout\n");
        memset(&SYNpacket, 0x00 , sizeof(seg_t));
        SYNpacket.header.src_port = ClientTCB[sockfd].client_portNum;
        SYNpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
        SYNpacket.header.type = SYN;
        SYNpacket.header.length = 24;
        printf("resending SYN packet\n");
        sip_sendseg(Sockfd,&SYNpacket);
        retry_count++;
      }else 
      {
        pthread_mutex_unlock(&mutex_syn);
        break;
      }
    }
  }
  if(retry_count > SYN_MAX_RETRY){ 
    ClientTCB[sockfd].state = CLOSED;
    printf("try enough\n");
    pthread_mutex_lock(&mutex_syn);
    isSYNACK = false;
    pthread_mutex_unlock(&mutex_syn);
    return -1;
  }
  else{
    ClientTCB[sockfd].state = CONNECTED;
    printf("send SYN and receive SYN ACK successfully\n");
    pthread_mutex_lock(&mutex_syn);
    isSYNACK = false;
    pthread_mutex_unlock(&mutex_syn);
    return 1;
  }
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
  printf("ready to STCP disconnect\n");
  if(ClientTCB[sockfd].is_used == NOT_USED) return -1;
  int retry_count = 0;
  seg_t FINpacket;
  memset(&FINpacket, 0 , sizeof(seg_t));
  FINpacket.header.src_port = ClientTCB[sockfd].client_portNum;
  FINpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
  FINpacket.header.type = FIN;
  FINpacket.header.length = 24;
  printf("sending FIN packet\n");
  sip_sendseg(Sockfd,&FINpacket);
  ClientTCB[sockfd].state = FINWAIT;
  clock_t start;
  start = clock();
  while(retry_count <= FIN_MAX_RETRY){
    if((clock() - start)*1000000 >= FIN_TIMEOUT){
      start = clock();
      pthread_mutex_lock(&mutex_fin);
      if(isFINACK == false){
        pthread_mutex_unlock(&mutex_fin);
        printf("timeout\n");
        memset(&FINpacket, 0 , sizeof(seg_t));
        FINpacket.header.src_port = ClientTCB[sockfd].client_portNum;
        FINpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
        FINpacket.header.type = FIN;
        FINpacket.header.length = 24;
        printf("resending FIN packet\n");
        sip_sendseg(Sockfd,&FINpacket);
        retry_count++;
      }else 
      {
        pthread_mutex_unlock(&mutex_fin);
        break;
      }
    }
  }
  if(retry_count > FIN_MAX_RETRY){ 
    ClientTCB[sockfd].state = CLOSED;
    printf("try enough\n");
    pthread_mutex_lock(&mutex_fin);
    isFINACK = false;
    pthread_mutex_unlock(&mutex_fin);
    return -1;
  }
  else{
    ClientTCB[sockfd].state = CLOSED;
    printf("send FIN and receive FIN ACK successfully\n");
    pthread_mutex_lock(&mutex_fin);
    isFINACK = false;
    pthread_mutex_unlock(&mutex_fin);
    return 1;
  }
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
  if(ClientTCB[sockfd].is_used == NOT_USED) return -1;
  pthread_mutex_destroy(ClientTCB[sockfd].bufMutex);
  //free(ClientTCB[sockfd].bufMutex);
  memset(&ClientTCB[sockfd], 0 , sizeof(client_tcb_t));
  ClientTCB[sockfd].is_used = NOT_USED;
  return 1;
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {
  int ret;
  seg_t packet;
  memset(&packet,0,sizeof(seg_t));
  while(ret = sip_recvseg(Sockfd,&packet) > 0){ 
    if(packet.header.dest_port == 0){
      printf("actually lose\n");
    }
    else{
      int tcbnum = gettcb(packet.header.dest_port);
      switch (ClientTCB[tcbnum].state)
      {
      case CLOSED:
        break;
      case SYNSENT:
        if(packet.header.type == SYNACK && packet.header.src_port == ClientTCB[tcbnum].server_portNum){
          printf("SYNACK received\n");
          pthread_mutex_lock(&mutex_syn);
          isSYNACK = true;
          pthread_mutex_unlock(&mutex_syn);
        }
        else{
          printf("client in SYNSENT, no SYNACK receive\n");
        }
        break;
      case CONNECTED:
        break;
      case FINWAIT:
        if(packet.header.type == FINACK && packet.header.src_port == ClientTCB[tcbnum].server_portNum){
          printf("FINACK received\n");
          pthread_mutex_lock(&mutex_fin);
          isFINACK = true;
          pthread_mutex_unlock(&mutex_fin);
        }
        else{
          printf("client in FINWAIT, no FINACK receive\n");
        }
        break;
      default:
        break;
      }
    }
    memset(&packet,0,sizeof(seg_t));
  }
  //close(Sockfd);
}



