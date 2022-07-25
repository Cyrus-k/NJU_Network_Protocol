//�ļ���: client/stcp_client.c
//
//����: ����ļ�����STCP�ͻ��˽ӿ�ʵ�� 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "../topology/topology.h"
#include "stcp_client.h"
#include "../common/seg.h"

bool isSYNACK = false;
bool isFINACK = false;

void sendBuf_clear(client_tcb_t* clienttcb) {
	pthread_mutex_lock(clienttcb->bufMutex);
  	segBuf_t* bufPtr = clienttcb->sendBufHead;
	while(bufPtr!=clienttcb->sendBufTail) {
		segBuf_t* temp = bufPtr;
		bufPtr = bufPtr->next;
		free(temp);	
	}
	free(clienttcb->sendBufTail);
	clienttcb->sendBufunSent = 0;
	clienttcb->sendBufHead = 0;
  	clienttcb->sendBufTail = 0;
	clienttcb->unAck_segNum = 0;
	pthread_mutex_unlock(clienttcb->bufMutex);	
}

void sendBuf_timeout(client_tcb_t* clienttcb) {
  printf("dataack timeout resend!\n");
	pthread_mutex_lock(clienttcb->bufMutex);
	segBuf_t* bufPtr=clienttcb->sendBufHead;
	int i;
	for(i=0;i<clienttcb->unAck_segNum;i++) {
		sip_sendseg(sip_conn,clienttcb->server_nodeID ,(seg_t*)bufPtr);
		struct timeval currentTime;
		gettimeofday(&currentTime,NULL);
		bufPtr->sentTime = currentTime.tv_sec*1000000+ currentTime.tv_usec;
		bufPtr = bufPtr->next; 
	}
	pthread_mutex_unlock(clienttcb->bufMutex);
}

void* sendBuf_timer(void* clienttcb) {
	client_tcb_t* my_clienttcb = (client_tcb_t*) clienttcb;
	while(1) {
		select(0,0,0,0,&(struct timeval){.tv_usec = SENDBUF_POLLING_INTERVAL/1000});

		struct timeval currentTime;
		gettimeofday(&currentTime,NULL);
		
		//���unAck_segNumΪ0, ��ζ�ŷ��ͻ�������û�ж�, �˳�.
		if(my_clienttcb->unAck_segNum == 0) {
			pthread_exit(NULL);
		}
		else if(my_clienttcb->sendBufHead->sentTime>0 && my_clienttcb->sendBufHead->sentTime<currentTime.tv_sec*1000000+currentTime.tv_usec-DATA_TIMEOUT) {
			sendBuf_timeout(my_clienttcb);
		}
	}
}

void sendBuf_recvAck(client_tcb_t* clienttcb, unsigned int ack_seqnum) {
	pthread_mutex_lock(clienttcb->bufMutex);
	if(ack_seqnum > clienttcb->sendBufTail->seg.header.seq_num)
		clienttcb->sendBufTail = 0;

	segBuf_t* bufPtr= clienttcb->sendBufHead;
	while(bufPtr && bufPtr->seg.header.seq_num<ack_seqnum) {
		clienttcb->sendBufHead = bufPtr->next;
		segBuf_t* temp = bufPtr;
		bufPtr = bufPtr->next;
		free(temp);
		clienttcb->unAck_segNum--;
	}
	pthread_mutex_unlock(clienttcb->bufMutex);
}

void sendBuf_addSeg(client_tcb_t* clienttcb, segBuf_t* newSegBuf) {
	pthread_mutex_lock(clienttcb->bufMutex);
	if(clienttcb->sendBufHead == 0) {
		newSegBuf->seg.header.seq_num = clienttcb->next_seqNum;
		clienttcb->next_seqNum += newSegBuf->seg.header.length;
		newSegBuf->sentTime = 0;
		clienttcb->sendBufHead= newSegBuf;
		clienttcb->sendBufunSent = newSegBuf;
		clienttcb->sendBufTail = newSegBuf;
	} else {
		newSegBuf->seg.header.seq_num = clienttcb->next_seqNum;
		clienttcb->next_seqNum += newSegBuf->seg.header.length;
		newSegBuf->sentTime = 0;
		clienttcb->sendBufTail->next = newSegBuf;
		clienttcb->sendBufTail = newSegBuf;
		if(clienttcb->sendBufunSent == 0)
			clienttcb->sendBufunSent = newSegBuf;
	}
	pthread_mutex_unlock(clienttcb->bufMutex);
}

void sendBuf_send(client_tcb_t* clienttcb) {
	pthread_mutex_lock(clienttcb->bufMutex);
	
	while(clienttcb->unAck_segNum<GBN_WINDOW && clienttcb->sendBufunSent!=0) {
		sip_sendseg(sip_conn,clienttcb->server_nodeID ,(seg_t*)clienttcb->sendBufunSent);
		struct timeval currentTime;
		gettimeofday(&currentTime,NULL);
		clienttcb->sendBufunSent->sentTime = currentTime.tv_sec*1000+ currentTime.tv_usec;
		
		if(clienttcb->unAck_segNum ==0) {
			pthread_t timer;
			pthread_create(&timer,NULL,sendBuf_timer, (void*)clienttcb);
		}
		clienttcb->unAck_segNum++;
		if(clienttcb->sendBufunSent != clienttcb->sendBufTail)
			clienttcb->sendBufunSent= clienttcb->sendBufunSent->next;
		else
			clienttcb->sendBufunSent = 0; 
	}
	pthread_mutex_unlock(clienttcb->bufMutex);
}


/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
void stcp_client_init(int conn) 
{

    pthread_t seghandle;
  	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    	memset(&ClientTCB[i], 0 , sizeof(client_tcb_t));
    	ClientTCB[i].is_used = NOT_USED;
    	pthread_mutex_t* sendBuf_mutex;
	  	sendBuf_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	  	assert(sendBuf_mutex!=NULL);
	  	pthread_mutex_init(sendBuf_mutex,NULL);
    	ClientTCB[i].bufMutex = sendBuf_mutex;
  	}
  	sip_conn = conn;
  	printf("start seghandler\n");
  	pthread_create(&seghandle, NULL, (void*)seghandler, NULL);
}

// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_client_sock(unsigned int client_port) 
{
	int ret = -1;
  	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    	if(ClientTCB[i].is_used == NOT_USED){
      		ret = i;
      		ClientTCB[i].state = CLOSED;
			ClientTCB[i].client_portNum = client_port;
      		ClientTCB[i].client_nodeID = topology_getMyNodeID();
	    	ClientTCB[i].server_nodeID = -1;
	    	ClientTCB[i].next_seqNum = 0;
	  	
	    	ClientTCB[i].sendBufHead = 0;
	    	ClientTCB[i].sendBufunSent = 0;
	    	ClientTCB[i].sendBufTail = 0;
	    	ClientTCB[i].unAck_segNum = 0;
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

// 这个函数用于连接服务器. 它以套接字ID, 服务器节点ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器节点ID和服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
	printf("ready to STCP connect\n");
  	if(ClientTCB[sockfd].is_used == NOT_USED) return -1;
  	int retry_count = 0;
 	ClientTCB[sockfd].server_portNum = server_port;
	ClientTCB[sockfd].server_nodeID = nodeID;
  	seg_t SYNpacket;
  	memset(&SYNpacket, 0x00 , sizeof(seg_t));
  	SYNpacket.header.src_port = ClientTCB[sockfd].client_portNum;
  	SYNpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
  	SYNpacket.header.type = SYN;
  	SYNpacket.header.seq_num = 0;
  	SYNpacket.header.length = 0;
  	printf("sending SYN packet\n");
  	printf("SRCPORT = %d,DESTPORT = %d\n",SYNpacket.header.src_port,SYNpacket.header.dest_port);
  	sip_sendseg(sip_conn,ClientTCB[sockfd].server_nodeID,&SYNpacket);
  	ClientTCB[sockfd].state = SYNSENT;
  	clock_t start;
 	start = clock();
  	while(retry_count <= SYN_MAX_RETRY){
    	if((double)(clock() - start)/CLOCKS_PER_SEC >= (double)(SYN_TIMEOUT/1000000000)){
      		pthread_mutex_lock(&mutex_syn);
      		if(isSYNACK == false){
        		pthread_mutex_unlock(&mutex_syn);
        		printf("timeout\n");
        		memset(&SYNpacket, 0x00 , sizeof(seg_t));
        		SYNpacket.header.src_port = ClientTCB[sockfd].client_portNum;
        		SYNpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
        		SYNpacket.header.type = SYN;
        		SYNpacket.header.length = 0;
        		printf("resending SYN packet\n");
        		sip_sendseg(sip_conn,ClientTCB[sockfd].server_nodeID,&SYNpacket);
        		retry_count++;
      		}else 
      		{
        		pthread_mutex_unlock(&mutex_syn);
        		break;
      		}
			start = clock();
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

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目.
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中.
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动.
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生. 
// 这个函数在成功时返回1，否则返回-1. 
// stcp_client_send是一个非阻塞函数调用.
// 因为用户数据被分片为固定大小的STCP段, 所以一次stcp_client_send调用可能会产生多个segBuf
// 被添加到发送缓冲区链表中. 如果调用成功, 数据就被放入TCB发送缓冲区链表中, 根据滑动窗口的情况,
// 数据可能被传输到网络中, 或在队列中等待传输.
int stcp_client_send(int sockfd, void* data, unsigned int length) 
{
    int segNum;
	int i;
	switch(ClientTCB[sockfd].state) {
		case CLOSED:
			return -1;		
		case SYNSENT:
			return -1;
		case CONNECTED:
			//ʹ���ṩ�����ݴ�����
			segNum = length/MAX_SEG_LEN;
			if(length%MAX_SEG_LEN)
			    segNum++;
	
			for(i=0;i<segNum;i++) {
				segBuf_t* newBuf = (segBuf_t*) malloc(sizeof(segBuf_t));	
				assert(newBuf!=NULL);
				bzero(newBuf,sizeof(segBuf_t));
				newBuf->seg.header.src_port = ClientTCB[sockfd].client_portNum;
				newBuf->seg.header.dest_port = ClientTCB[sockfd].server_portNum;
				if(length%MAX_SEG_LEN!=0 && i==segNum-1)
					newBuf->seg.header.length = length%MAX_SEG_LEN;
				else
					newBuf->seg.header.length = MAX_SEG_LEN;
        //printf("%d\n",newBuf->seg.header.length);
				newBuf->seg.header.type = DATA;
				char* datatosend = (char*)data;
				memcpy(newBuf->seg.data,&datatosend[i*MAX_SEG_LEN],newBuf->seg.header.length);
				sendBuf_addSeg(&ClientTCB[sockfd],newBuf);
			}
			sendBuf_send(&ClientTCB[sockfd]);
			return 1;
		case FINWAIT:
			return -1;
		default:
			return -1;
	}
}

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
int stcp_client_disconnect(int sockfd) 
{
	printf("ready to STCP disconnect\n");
  	if(ClientTCB[sockfd].is_used == NOT_USED) return -1;
  	int retry_count = 0;
  	seg_t FINpacket;
  	memset(&FINpacket, 0 , sizeof(seg_t));
  	FINpacket.header.src_port = ClientTCB[sockfd].client_portNum;
  	FINpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
  	FINpacket.header.type = FIN;
  	FINpacket.header.length = 0;
  	printf("sending FIN packet\n");
  	sip_sendseg(sip_conn,ClientTCB[sockfd].server_nodeID,&FINpacket);
  	ClientTCB[sockfd].state = FINWAIT;
  	clock_t start;
  	start = clock();
  	while(retry_count <= FIN_MAX_RETRY){
    	if((double)(clock() - start)/CLOCKS_PER_SEC >= (double)(FIN_TIMEOUT/1000000000)){
      		start = clock();
      		pthread_mutex_lock(&mutex_fin);
      		if(isFINACK == false){
        		pthread_mutex_unlock(&mutex_fin);
        		printf("timeout\n");
        		memset(&FINpacket, 0 , sizeof(seg_t));
        		FINpacket.header.src_port = ClientTCB[sockfd].client_portNum;
        		FINpacket.header.dest_port = ClientTCB[sockfd].server_portNum;
        		FINpacket.header.type = FIN;
        		FINpacket.header.length = 0;
        		printf("resending FIN packet\n");
        		sip_sendseg(sip_conn,ClientTCB[sockfd].server_nodeID,&FINpacket);
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
    	sendBuf_clear(&ClientTCB[sockfd]);
    	return 1;
  	}
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_client_close(int sockfd) 
{
	if(ClientTCB[sockfd].is_used == NOT_USED) return -1;
 	pthread_mutex_destroy(ClientTCB[sockfd].bufMutex);
  //free(ClientTCB[sockfd].bufMutex);
  	memset(&ClientTCB[sockfd], 0 , sizeof(client_tcb_t));
  	ClientTCB[sockfd].is_used = NOT_USED;
  	return 1;
}

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	int ret;
  	seg_t packet;
	int srcnode;
  	memset(&packet,0,sizeof(seg_t));
  	while((ret = sip_recvseg(sip_conn,&srcnode,&packet)) > 0){ 
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
        			if(packet.header.type == SYNACK && packet.header.src_port == ClientTCB[tcbnum].server_portNum && srcnode == ClientTCB[tcbnum].server_nodeID){
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
        			if(packet.header.type == DATAACK && ClientTCB[tcbnum].server_portNum == packet.header.src_port && srcnode == ClientTCB[tcbnum].server_nodeID) {
						if(ClientTCB[tcbnum].sendBufHead != NULL && packet.header.ack_num >= ClientTCB[tcbnum].sendBufHead->seg.header.seq_num) {
							sendBuf_recvAck(&ClientTCB[tcbnum], packet.header.ack_num);
							sendBuf_send(&ClientTCB[tcbnum]);
						}
					}
					else {
						printf("CLIENT: IN CONNECTED, NO DATAACK SEG RECEIVED\n");
					}
        			break;
      			case FINWAIT:
        			if(packet.header.type == FINACK && packet.header.src_port == ClientTCB[tcbnum].server_portNum && srcnode == ClientTCB[tcbnum].server_nodeID){
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
}
