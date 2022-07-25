#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "stcp_server.h"
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include "../common/constants.h"

/*面向应用层的接口*/

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

void stcp_server_init(int conn) {
	pthread_t seghandle;
	printf("hello\n");
  	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    	memset(&ServerTCB[i], 0 , sizeof(server_tcb_t));
    	ServerTCB[i].is_used = NOT_USED;
    	pthread_mutex_t* sendBuf_mutex;
	  	sendBuf_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	  	assert(sendBuf_mutex!=NULL);
	  	pthread_mutex_init(sendBuf_mutex,NULL);
    	ServerTCB[i].bufMutex = sendBuf_mutex;
  	}
  	Sockfd = conn;
  	printf("start seghandler\n");
  	pthread_create(&seghandle, NULL, (void*)seghandler, NULL);
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) {
	int ret = -1;
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    	if(ServerTCB[i].is_used == NOT_USED){
      		ret = i;
      		ServerTCB[i].state = CLOSED;
      		ServerTCB[i].server_portNum = server_port;
      		ServerTCB[i].is_used = USED;
      		break;
    	}
  	}
  	return ret;
}

int gettcb(unsigned int server_port){
  int ret = -1;
  for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if(ServerTCB[i].server_portNum == server_port && ServerTCB[i].is_used == USED) return i;
  }
  return ret;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd) {
	printf("ready to STCP accept\n");
  	if(ServerTCB[sockfd].is_used == NOT_USED) return -1;
  	ServerTCB[sockfd].state = LISTENING;
	while(1){
		sleep(ACCEPT_POLLING_INTERVAL/(double)1000000000);
		if(ServerTCB[sockfd].state == CONNECTED) break;
	}
	return 1;
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) {
	return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd) {
	if(ServerTCB[sockfd].is_used == NOT_USED) return -1;
  	pthread_mutex_destroy(ServerTCB[sockfd].bufMutex);
  	memset(&ServerTCB[sockfd], 0 , sizeof(server_tcb_t));
  	ServerTCB[sockfd].is_used = NOT_USED;
	return 1;
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
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
			//printf("%d %d\n",packet.header.src_port,packet.header.dest_port);
			//printf("%d\n",packet.header.type);
      		int tcbnum = gettcb(packet.header.dest_port);
			//printf("%d\n",ServerTCB[tcbnum].client_portNum);
      		switch (ServerTCB[tcbnum].state)
      		{
      			case CLOSED:
        			break;
      			case LISTENING:
        			if(packet.header.type == SYN){
          				printf("SYN received\n");
          				ServerTCB[tcbnum].state = CONNECTED;
						ServerTCB[tcbnum].client_portNum = packet.header.src_port;
						seg_t SYNACKpacket;
  						memset(&SYNACKpacket, 0 , sizeof(seg_t));
  						SYNACKpacket.header.src_port = ServerTCB[tcbnum].server_portNum;
  						SYNACKpacket.header.dest_port = ServerTCB[tcbnum].client_portNum;
  						SYNACKpacket.header.type = SYNACK;
  						SYNACKpacket.header.length = 24;
  						printf("sending SYNACK packet\n");
  						sip_sendseg(Sockfd,&SYNACKpacket);
        			}
        			else{
          				printf("server in LISTENING, no SYN receive\n");
        			}
        			break;
      			case CONNECTED:
					if(packet.header.type == SYN){
          				printf("SYN received\n");
          				ServerTCB[tcbnum].state = CONNECTED;
						ServerTCB[tcbnum].client_portNum = packet.header.src_port;
						seg_t SYNACKpacket;
  						memset(&SYNACKpacket, 0 , sizeof(seg_t));
  						SYNACKpacket.header.src_port = ServerTCB[tcbnum].server_portNum;
  						SYNACKpacket.header.dest_port = ServerTCB[tcbnum].client_portNum;
  						SYNACKpacket.header.type = SYNACK;
  						SYNACKpacket.header.length = 24;
  						printf("sending SYNACK packet\n");
  						sip_sendseg(Sockfd,&SYNACKpacket);
        			}
					else if(packet.header.type == FIN){
						printf("FIN received\n");
          				ServerTCB[tcbnum].state = CLOSEWAIT;
						seg_t FINACKpacket;
  						memset(&FINACKpacket, 0 , sizeof(seg_t));
  						FINACKpacket.header.src_port = ServerTCB[tcbnum].server_portNum;
  						FINACKpacket.header.dest_port = ServerTCB[tcbnum].client_portNum;
  						FINACKpacket.header.type = FINACK;
  						FINACKpacket.header.length = 24;
  						printf("sending FINACK packet\n");
  						sip_sendseg(Sockfd,&FINACKpacket);
						Close_wait = clock();
					}
        			else{
          				printf("unknown packet kind\n");
        			}
        			break;
      			case CLOSEWAIT:
				  	if(clock() - Close_wait >= CLOSEWAIT_TIMEOUT){
						ServerTCB[tcbnum].state = CLOSED;
					}
        			else if(packet.header.type == FIN){
						printf("FIN received\n");
          				ServerTCB[tcbnum].state = CLOSEWAIT;
						seg_t FINACKpacket;
  						memset(&FINACKpacket, 0 , sizeof(seg_t));
  						FINACKpacket.header.src_port = ServerTCB[tcbnum].server_portNum;
  						FINACKpacket.header.dest_port = ServerTCB[tcbnum].client_portNum;
  						FINACKpacket.header.type = FINACK;
  						FINACKpacket.header.length = 24;
  						printf("sending FINACK packet\n");
  						sip_sendseg(Sockfd,&FINACKpacket);
					}
        			else{
          				printf("unknown packet kind\n");
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
