//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include <stdbool.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 10
#define LISTENQ 3

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 
int neighbor_num;
bool connect_to_sip = false;

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg) {
	int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(myip);
    server_addr.sin_port = htons(CONNECTION_PORT);
    //int on = 1;
    //setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    int rt1 = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rt1 < 0) {
        printf("bind failed ");
    }

    int rt2 = listen(listenfd, LISTENQ);
    if (rt2 < 0) {
        printf("listen failed ");
    }
	printf("Son%d Server running\n",myid);
	int num_connect = 0;
	for(int i =0; i<neighbor_num;i++){
		if(myid < nt[i].nodeID)
			num_connect++;
	}
	printf("%d\n",num_connect);
	while(1){
		if(num_connect == 0) break;
		struct sockaddr_in ss;
    	socklen_t slen = sizeof(ss);
    	int fd = accept(listenfd, (struct sockaddr *) &ss, &slen);
    	if (fd < 0) {
        	printf("error connect");
    	}
		printf("receive request\n");
		for(int i =0; i<neighbor_num;i++){
			if(strcmp(inet_ntoa(ss.sin_addr),nt[i].nodeIP) == 0){
				num_connect--;
				nt[i].conn = fd;
			}
		}
	}
	printf("all connect\n");
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	for(int i = 0;i<neighbor_num;i++){
		if(myid > nt[i].nodeID){
			int sockfd;
    		struct sockaddr_in servaddr;
    		sockfd = socket(AF_INET, SOCK_STREAM, 0);
    		bzero(&servaddr, sizeof(servaddr));
    		servaddr.sin_family = AF_INET;
    		servaddr.sin_port = htons(CONNECTION_PORT);
    		servaddr.sin_addr.s_addr= inet_addr(nt[i].nodeIP);
    		int connect_rt = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
			nt[i].conn = sockfd;
    		if (connect_rt < 0){
				printf("error connect\n");
        		return -1;
    		}
		}
	}
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
	int no = (int) arg;
	sip_pkt_t pkt;
	while(1){
		if(recvpkt(&pkt,nt[no].conn) > 0){
			while(1){
				if(connect_to_sip == true) break;
			}
			if(forwardpktToSIP(&pkt,sip_conn) > 0){
				printf("succeed sending to sip\n");
			}
		}
		else break;
		memset(&pkt,0,sizeof(sip_pkt_t));
	}
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SON_PORT);

    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    int rt1 = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rt1 < 0) {
        printf("bind failed ");
		return;
    }

    int rt2 = listen(listenfd, LISTENQ);
    if (rt2 < 0) {
        printf("listen failed ");
		return;
    }
	printf("waiting sip connect\n");
	struct sockaddr_in ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listenfd, (struct sockaddr *) &ss, &slen);
    if (fd < 0) {
        printf("error connect");
		return;
    }
	connect_to_sip =true;
	sip_conn = fd;
	sip_pkt_t pkt;
	int nextnode = 0;
	while(1){
		if(getpktToSend(&pkt,&nextnode,sip_conn) > 0){
			pkt.header.src_nodeID = myid;
			if(nextnode == BROADCAST_NODEID){
				printf("broadcasting\n");
				for(int i = 0;i<neighbor_num;i++){
					if(sendpkt(&pkt,nt[i].conn)){
						printf("succeed sending to neighbour\n");
					}
				}
			}
			else{
				int next_son;
				for(int i = 0;i<neighbor_num;i++){
					if(nt[i].nodeID == nextnode)
						next_son = nt[i].conn;
				}
				if(sendpkt(&pkt,next_son) > 0){
					printf("succeed sending to neighbour\n");
				}
			}
		}
		else	
			break;
		memset(&pkt,0,sizeof(sip_pkt_t));
	}
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	close(sip_conn);
	for(int i = 0;i<neighbor_num;i++){
		close(nt[i].conn);
	}
	//free(nt);
	printf("finish son\n");
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = get_neightbor();
	neighbor_num = nbrNum;
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}
	
	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();
	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	printf("neighbor connect finish\n");
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)i);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
