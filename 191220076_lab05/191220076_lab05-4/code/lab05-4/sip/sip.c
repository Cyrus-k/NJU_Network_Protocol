//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 30
#define LISTENQ 8
/**************************************************************/
//声明全局变量
/**************************************************************/
bool connect_to_stcp = false;

int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

bool checksamenode(char* name,allnode* tempnode){
  	allnode* cur = tempnode;
  	while(cur != NULL){
    	if(strcmp(name,cur->name) == 0)
      		return true;
    	cur = cur->next;
  	}
  	return false;
}

void addnode(char* name,allnode* tempnode){
  	allnode* tail = tempnode;
  	while(tail->next != NULL){
    	tail = tail->next;
	}
  	allnode* temp = (allnode*)malloc(sizeof(allnode));
 	char ip[20];
  	sip_hostname_to_ip(name , ip);
  	temp->nodeID = topology_getNodeIDfromip(ip);
  	strcpy(temp->name,name);
  	temp->next = NULL;
  	tail->next = temp;
}

void destroytable(){
  	allnode* cur;
  	while(mynodetable){
    	cur = mynodetable;
    	mynodetable = mynodetable->next;
    	free(cur);
  	}
}

allnode* get_allnode(int* allnum){
  	int num = 0;
  	char name1[20];
  	char name2[20];
  	char distance[20];
  	FILE* f = fopen("../topology/topology.dat", "r");
 // struct allnode_* tempno = (struct allnode_*)malloc(sizeof(struct allnode_));
  	allnode* tempnodetable = (allnode*)malloc(sizeof(allnode));
  	assert(tempnodetable != NULL);
  	strcpy(tempnodetable->name,"start");
  	tempnodetable->next = NULL;
  
  	while(!feof(f)){
    	fscanf(f,"%s",name1);
    	fscanf(f,"%s",name2);
    	int end = fscanf(f,"%s",distance);
    	if(end == -1) break;
    	if(checksamenode(name1,tempnodetable) == false){
      		num++;
      		addnode(name1,tempnodetable);
    	}
    	if(checksamenode(name2,tempnodetable) == false){
      		num++;
      		addnode(name2,tempnodetable);
    	}
  	}
  	fclose(f);
  	*allnum = num;
  	return tempnodetable;
}

int sip_hostname_to_ip(char * hostname , char* ip)
{
	struct hostent *he;
	struct in_addr **addr_list;
	int i;
	if ((he = gethostbyname(hostname)) == NULL) 
	{
		// get the host info
		herror("gethostbyname");
		return 1;
	}

	addr_list = (struct in_addr **) he->h_addr_list;
	for(i = 0; addr_list[i] != NULL; i++) 
	{
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		return 0;
	}
	
	return 1;
}

sip_nbr_entry_t* sip_nt_create()
{
  	char hostname[256];
  	gethostname(hostname,256);
  	printf("%s\n",hostname);
	char ip[100];
	sip_hostname_to_ip(hostname , ip);
  	strcpy(sip_myip,ip);
	printf("%s resolved to %s\n" , hostname , ip);
  	sip_myid = topology_getNodeIDfromip(ip);
  	char data1[100];
  	char data2[100];
  	char distance[10];
  	sip_nbr_entry_t* mytable = (sip_nbr_entry_t*) malloc(sip_neighbor_num * sizeof(sip_nbr_entry_t));
  	int pointer = 0;
  	FILE* f = fopen("../topology/topology.dat", "r");
  	if(f == NULL){
    	printf("error open\n");
  	}
  	while(!feof(f)){
    	fscanf(f,"%s",data1);
    	fscanf(f,"%s",data2);
    	int end = fscanf(f,"%s",distance);
   		 if(end == -1) break;
    	//printf("data1 = %s\n",data1);
    	//printf("data2 = %s\n",data2);
   		if(strcmp(data1,hostname) == 0){
      	mytable[pointer].conn = -1;
      	sip_hostname_to_ip(data2,mytable[pointer].nodeIP);
      	//printf("%s\n",mytable[pointer].nodeIP);
      	char temp[20];
      	strcpy(temp,mytable[pointer].nodeIP);
      	mytable[pointer].nodeID = topology_getNodeIDfromip(temp);
      	//printf("%d\n",mytable[pointer].nodeID);
      	mytable[pointer].dis = atoi(distance);
      	pointer++;
   	 	}
    	else if(strcmp(data2,hostname) == 0){
      	mytable[pointer].conn = -1;
      	sip_hostname_to_ip(data1,mytable[pointer].nodeIP);
       //printf("%s\n",mytable[pointer].nodeIP);
      	char temp[20];
      	strcpy(temp,mytable[pointer].nodeIP);
      	mytable[pointer].nodeID = topology_getNodeIDfromip(temp);
      	//printf("%d\n",mytable[pointer].nodeID);
      	mytable[pointer].dis = atoi(distance);
      	pointer++;
    	}
  	}
  //printf("pointer = %d\n",pointer);
  	fclose(f);
  	return mytable;
}


//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON() { 
	int sockfd;
    struct sockaddr_in servaddr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SON_PORT);
    servaddr.sin_addr.s_addr= inet_addr("127.0.0.1");
    int connect_rt = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (connect_rt < 0){
        return -1;
    }
	else{
		return sockfd;
	}
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	sleep(3);
	sip_pkt_t pkt;
	memset(&pkt,0,sizeof(sip_pkt_t));
	pkt.header.dest_nodeID = BROADCAST_NODEID;
	pkt.header.type = ROUTE_UPDATE;
	pkt.header.src_nodeID = sip_myid;
	pkt_routeupdate_t routepkt;
	memset(&routepkt,0,sizeof(pkt_routeupdate_t));
	routepkt.entryNum = all_num;
	pthread_mutex_lock(dv_mutex);
	for(int i = 0;i < routepkt.entryNum;i++){
		routepkt.entry[i].nodeID = dv[0].dvEntry[i].nodeID;
		routepkt.entry[i].cost = dv[0].dvEntry[i].cost;
	}
	pthread_mutex_unlock(dv_mutex);
	memcpy(pkt.data,&routepkt,sizeof(routepkt));
	pkt.header.length = sizeof(routepkt);
	if(son_sendpkt(BROADCAST_NODEID,&pkt,son_conn) > 0){
		printf("sending broadcast packet\n");
	}
	int count = 0;
	memset(&routepkt,0,sizeof(pkt_routeupdate_t));
	memset(&pkt,0,sizeof(sip_pkt_t));
	clock_t start;
  	start = clock();
	while(1){
		if((double)(clock() - start)/CLOCKS_PER_SEC >= ROUTEUPDATE_INTERVAL){
			count++;
			pkt.header.dest_nodeID = BROADCAST_NODEID;
			pkt.header.type = ROUTE_UPDATE;
			pkt.header.src_nodeID = sip_myid;
			routepkt.entryNum = all_num;
			pthread_mutex_lock(dv_mutex);
			for(int i = 0;i < routepkt.entryNum;i++){
				routepkt.entry[i].nodeID = dv[0].dvEntry[i].nodeID;
				routepkt.entry[i].cost = dv[0].dvEntry[i].cost;
			}
			pthread_mutex_unlock(dv_mutex);
			memcpy(pkt.data,&routepkt,sizeof(routepkt));
			pkt.header.length = sizeof(routepkt);
			if(son_sendpkt(BROADCAST_NODEID,&pkt,son_conn) > 0){
				printf("sending broadcast packet\n");
			}
			start = clock();
		}
	}
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	sip_pkt_t pkt;

	while(son_recvpkt(&pkt,son_conn)>0) {
		
		printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
		assert(pkt.header.dest_nodeID != 0);
		if(pkt.header.type == ROUTE_UPDATE){
			pthread_mutex_lock(dv_mutex);
			for(int i=0;i<sip_neighbor_num + 1;i++){
				if(dv[i].nodeID == pkt.header.src_nodeID){
					pkt_routeupdate_t recvpkt;
					memcpy(&recvpkt,pkt.data,sizeof(recvpkt));
					for(int j = 0;j<recvpkt.entryNum;j++){
						for(int k = 0;k<recvpkt.entryNum;k++){
							if(dv[i].dvEntry[j].nodeID == recvpkt.entry[k].nodeID){
								dv[i].dvEntry[j].cost = recvpkt.entry[k].cost;
								break;
							}
						}	
					}
					break;
				}
			}
			for(int i=0;i<all_num;i++){
				bool need_refresh_routingtable = false;
				int destid = dv[0].dvEntry[i].nodeID;
				int mincost = dv[0].dvEntry[i].cost;
				int newnextnode = dv[0].dvEntry[i].nodeID;
				if(destid != dv[0].nodeID){
					for(int j = 1;j<sip_neighbor_num+1;j++){
						int curnode = dv[j].nodeID;
						int curdistance = 0;
						int seconddistance = 0;
						for(int k = 0;k<all_num;k++){
							if(dv[0].dvEntry[k].nodeID == curnode){
								curdistance = dv[0].dvEntry[k].cost;
								break;
							}
						}
						for(int k = 0;k<all_num;k++){
							if(dv[j].dvEntry[k].nodeID == destid){
								seconddistance = dv[j].dvEntry[k].cost;
								break;
							}
						}
						if(curdistance+seconddistance < mincost)
						{
							mincost = curdistance + seconddistance;
							newnextnode = curnode;
							need_refresh_routingtable = true;
						}
					}
				}
				dv[0].dvEntry[i].cost = mincost;
				if(need_refresh_routingtable == true){
					pthread_mutex_lock(routingtable_mutex);
					routingtable_setnextnode(routingtable,destid,newnextnode);
					pthread_mutex_unlock(routingtable_mutex);
				}
			}
			pthread_mutex_unlock(dv_mutex);// finish refreshing dvtable

		}
		else if(pkt.header.type == SIP){
			if(pkt.header.dest_nodeID == sip_myid){
				seg_t sendpkt;
				memcpy(&sendpkt,pkt.data,sizeof(seg_t));
				if(forwardsegToSTCP(stcp_conn,pkt.header.src_nodeID,&sendpkt) > 0){
					printf("succeed sending to stcp\n");
				}
			}
			else{
				printf("destnodeid = %d\n",pkt.header.dest_nodeID);
				printf("transmit...............\n");
				pthread_mutex_lock(routingtable_mutex);
				int nextnodeid = routingtable_getnextnode(routingtable,pkt.header.dest_nodeID);
				pthread_mutex_unlock(routingtable_mutex);
				if(son_sendpkt(nextnodeid,&pkt,son_conn) > 0)
					printf("succeed sending to son\n");
			}
		}
		else if(pkt.header.type == DISCONNECT){
			printf("receive\n");
			int deleteid = pkt.header.src_nodeID;
			for(int i=0;i<sip_neighbor_num;i++){
				if(nct[i].nodeID == deleteid)
					nct[i].cost = INFINITE_COST;
			}
			pthread_mutex_lock(dv_mutex);
			for(int i=0;i<sip_neighbor_num+1;i++){
				if(i == 0){
					for(int j=0;j<all_num;j++){
						if(dv[i].dvEntry[j].nodeID == dv[i].nodeID)
							dv[i].dvEntry[j].cost = 0;
						else
							dv[i].dvEntry[j].cost = INFINITE_COST;
					}
					for(int j=0;j<all_num;j++){
						for(int k=0;k<sip_neighbor_num;k++){
							if(dv[i].dvEntry[j].nodeID == nct[k].nodeID)
								dv[i].dvEntry[j].cost = nct[k].cost;
						}
					}
				}
				if(dv[i].nodeID == deleteid){
					for(int j = 0;j<all_num;j++){
						dv[i].dvEntry[j].cost = INFINITE_COST;
					}
				}
				else{
					for(int j = 0;j<all_num;j++){
						if(dv[i].dvEntry[j].nodeID == deleteid)
							dv[i].dvEntry[j].cost = INFINITE_COST;
					}
				}
			}
			pthread_mutex_unlock(dv_mutex);
			pthread_mutex_lock(routingtable_mutex);
			routingtable_deletenode(routingtable,deleteid);
			pthread_mutex_unlock(routingtable_mutex);
			pthread_mutex_lock(dv_mutex);
			for(int i=0;i<all_num;i++){
				bool need_refresh_routingtable = false;
				int destid = dv[0].dvEntry[i].nodeID;
				int mincost = dv[0].dvEntry[i].cost;
				int newnextnode = dv[0].dvEntry[i].nodeID;
				if(destid != dv[0].nodeID && destid != deleteid){
					for(int j = 1;j<sip_neighbor_num+1;j++){
						int curnode = dv[j].nodeID;
						int curdistance = 0;
						int seconddistance = 0;
						for(int k = 0;k<all_num;k++){
							if(dv[0].dvEntry[k].nodeID == curnode){
								curdistance = dv[0].dvEntry[k].cost;
								break;
							}
						}
						for(int k = 0;k<all_num;k++){
							if(dv[j].dvEntry[k].nodeID == destid){
								seconddistance = dv[j].dvEntry[k].cost;
								break;
							}
						}
						if(curdistance+seconddistance < mincost)
						{
							mincost = curdistance + seconddistance;
							newnextnode = curnode;
							need_refresh_routingtable = true;
						}
					}
				}
				dv[0].dvEntry[i].cost = mincost;
				if(mincost != INFINITE_COST){
					pthread_mutex_lock(routingtable_mutex);
					routingtable_setnextnode(routingtable,destid,newnextnode);
					pthread_mutex_unlock(routingtable_mutex);
				}
			}
			pthread_mutex_unlock(dv_mutex);
			printf("*************************************\n");
			nbrcosttable_print(nct);
			dvtable_print(dv);
			routingtable_print(routingtable);

		}
		memset(&pkt,0,sizeof(sip_pkt_t));
	}
	close(son_conn);
	son_conn = -1;
	pthread_exit(NULL);
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	close(son_conn);
	close(stcp_conn);
	nbrcosttable_destroy(nct);
	dvtable_destroy(dv);
	routingtable_destroy(routingtable);
	destroytable();
	printf("sip finish\n");
	exit(0);
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP() {
	int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SIP_PORT);

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
	printf("waiting stcp connect\n");
	while (1)
	{
		struct sockaddr_in ss;
    	socklen_t slen = sizeof(ss);
    	int fd = accept(listenfd, (struct sockaddr *) &ss, &slen);
    	if (fd < 0) {
        	printf("error connect");
			return;
    	}
	
		connect_to_stcp = true;
		stcp_conn = fd;
		seg_t segpkt;
		sip_pkt_t pkt;
		int destnode = 0;
		while(1){
			if(getsegToSend(stcp_conn,&destnode,&segpkt) > 0){
				//printf("receiving stcp pkt %d\n",destnode);
				memcpy(pkt.data,&segpkt,sizeof(segpkt));
				pkt.header.type = SIP;
				pkt.header.dest_nodeID = destnode;
				pkt.header.length = segpkt.header.length + 24;
				pkt.header.src_nodeID = sip_myid;
				int nextnode = 0;
				// find routing table
				pthread_mutex_lock(routingtable_mutex);
				nextnode = routingtable_getnextnode(routingtable,destnode);
				pthread_mutex_unlock(routingtable_mutex);
				if(son_sendpkt(nextnode,&pkt,son_conn) > 0)
					printf("succeed sending to son\n");
			}
			else	
				break;
			memset(&pkt,0,sizeof(sip_pkt_t));
			memset(&segpkt,0,sizeof(seg_t));
		}
	}
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");
	sip_neighbor_num = topology_getNbrNum();
	mynodetable = get_allnode(&all_num);
	//初始化全局变量
	printf("all node num = %d sip_neighbour num = %d\n",all_num,sip_neighbor_num);
	sip_nt = sip_nt_create();
	nct = nbrcosttable_create();
	printf("nbrcostable created\n");
	dv = dvtable_create();
	printf("dvtable created\n");
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}


