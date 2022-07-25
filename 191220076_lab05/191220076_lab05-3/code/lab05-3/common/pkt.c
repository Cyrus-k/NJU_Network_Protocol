// 文件名: common/pkt.c

#include "pkt.h"
enum RecvFsm{PKTSTART1,PKTSTART2,PKTRECV,PKTSTOP};
// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
  int ret = 0;
	char sendline[MAXLINE];
	char tempbuf[MAXLINE];
  sendpkt_arg_t sendpacket;
	sip_pkt_t temp = *pkt;
	//unsigned short checknum = checksum(&temp);
	//temp.header.checksum = checknum;
	//printf("get %d\n",temp.header.dest_port);
  sendpacket.pkt = temp;
  sendpacket.nextNodeID = nextNodeID;
  memcpy(tempbuf,&sendpacket,sizeof(sendpkt_arg_t));
	//printf("%d\n",tempbuf[4]);	
  char head[3] = "!&";
	char tail[3] = "!#";
	memcpy(sendline,head,sizeof(head));
	memcpy(sendline+2,tempbuf,sizeof(sendpkt_arg_t));
	memcpy(sendline+2+sizeof(sendpkt_arg_t),tail,sizeof(tail));
	//printf("%s\n",sendline);
	//printf("good\n");
	if(send(son_conn,sendline,sizeof(sendline),0))
    {
        //printf("send information\n");
		ret = 1;
    }
	memset(sendline,0,sizeof(sendline));
	memset(tempbuf,0,sizeof(tempbuf));
	return ret;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
  char recvline[MAXLINE];
	memset(recvline,0,sizeof(recvline));
	int recv_used = 0;
	int result;
	enum RecvFsm readyrecv = PKTSTART1;
	int finishreading = 0;
	while (1) {
        char ch;
		//printf("waiting\n");
        result = recv(son_conn, &ch, 1, 0);
        //断开连接或者出错
		//printf("has something\n");
        if (result == 0) {
			return -1;
            break;
        } else if (result == -1) {
            printf("read error\n");
			return -1;
            break;
        }
        switch (readyrecv)
		{
		case PKTSTART1:
		{
			if(ch == '!') readyrecv = PKTSTART2;
			break;
		}
		case PKTSTART2:
		{
			if(ch == '&') readyrecv = PKTRECV;
			else if(ch == '!'){

			}
			else	readyrecv = PKTSTART1;
			break;
		}
		case PKTRECV:
		{
			recvline[recv_used++] = ch;
			if(ch == '!') readyrecv = PKTSTOP;
			break;
		}
		case PKTSTOP:
		{
			if(ch == '#') {
				recvline[recv_used - 1] = '\0';
				finishreading = 1;
			}
			else if(ch == '!'){
				recvline[recv_used++] = ch;
			}
			else{
				recvline[recv_used++] = ch;
				readyrecv = PKTRECV;
			}
			break;
		}
		default:
			break;
		}
		if(finishreading == 1) break;
    }
	//printf("finish listening\n");
	memcpy(pkt,recvline,sizeof(sip_pkt_t));
	memset(recvline,0,sizeof(recvline));
	return 1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
  sendpkt_arg_t* recvpacket = (sendpkt_arg_t*)malloc(sizeof(sendpkt_arg_t));
  char recvline[MAXLINE];
	memset(recvline,0,sizeof(recvline));
	int recv_used = 0;
	int result;
	enum RecvFsm readyrecv = PKTSTART1;
	int finishreading = 0;
	while (1) {
        char ch;
		//printf("waiting\n");
        result = recv(sip_conn, &ch, 1, 0);
        //断开连接或者出错
		//printf("has something\n");
        if (result == 0) {
			return -1;
            break;
        } else if (result == -1) {
            printf("read error\n");
			return -1;
            break;
        }
        switch (readyrecv)
		{
		case PKTSTART1:
		{
			if(ch == '!') readyrecv = PKTSTART2;
			break;
		}
		case PKTSTART2:
		{
			if(ch == '&') readyrecv = PKTRECV;
			else if(ch == '!'){

			}
			else	readyrecv = PKTSTART1;
			break;
		}
		case PKTRECV:
		{
			recvline[recv_used++] = ch;
			if(ch == '!') readyrecv = PKTSTOP;
			break;
		}
		case PKTSTOP:
		{
			if(ch == '#') {
				recvline[recv_used - 1] = '\0';
				finishreading = 1;
			}
			else if(ch == '!'){
				recvline[recv_used++] = ch;
			}
			else{
				recvline[recv_used++] = ch;
				readyrecv = PKTRECV;
			}
			break;
		}
		default:
			break;
		}
		if(finishreading == 1) break;
    }
	//printf("finish listening\n");
	memcpy(recvpacket,recvline,sizeof(sendpkt_arg_t));
  memcpy(pkt,&(recvpacket->pkt),sizeof(sip_pkt_t));
  *nextNode = recvpacket->nextNodeID;
	memset(recvline,0,sizeof(recvline));
	free(recvpacket);
	return 1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
  int ret = 0;
	char sendline[MAXLINE];
	char tempbuf[MAXLINE];
	sip_pkt_t temp = *pkt;
	//unsigned short checknum = checksum(&temp);
	//temp.header.checksum = checknum;
	//printf("get %d\n",temp.header.dest_port);
  memcpy(tempbuf,&temp,sizeof(sip_pkt_t));
	//printf("%d\n",tempbuf[4]);	
  char head[3] = "!&";
	char tail[3] = "!#";
	memcpy(sendline,head,sizeof(head));
	memcpy(sendline+2,tempbuf,sizeof(sip_pkt_t));
	memcpy(sendline+2+sizeof(sip_pkt_t),tail,sizeof(tail));
	//printf("%s\n",sendline);
	//printf("good\n");
	if(send(sip_conn,sendline,sizeof(sendline),0))
    {
        //printf("send information\n");
		ret = 1;
    }
	memset(sendline,0,sizeof(sendline));
	memset(tempbuf,0,sizeof(tempbuf));
	return ret;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
  int ret = 0;
	char sendline[MAXLINE];
	char tempbuf[MAXLINE];
	sip_pkt_t temp = *pkt;
	//unsigned short checknum = checksum(&temp);
	//temp.header.checksum = checknum;
	//printf("get %d\n",temp.header.dest_port);
  memcpy(tempbuf,&temp,sizeof(sip_pkt_t));
	//printf("%d\n",tempbuf[4]);	
  char head[3] = "!&";
	char tail[3] = "!#";
	memcpy(sendline,head,sizeof(head));
	memcpy(sendline+2,tempbuf,sizeof(sip_pkt_t));
	memcpy(sendline+2+sizeof(sip_pkt_t),tail,sizeof(tail));
	//printf("%s\n",sendline);
	//printf("good\n");
	if(send(conn,sendline,sizeof(sendline),0))
    {
        //printf("send information\n");
		ret = 1;
    }
	memset(sendline,0,sizeof(sendline));
	memset(tempbuf,0,sizeof(tempbuf));
	return ret;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.


int recvpkt(sip_pkt_t* pkt, int conn)
{
  char recvline[MAXLINE];
	memset(recvline,0,sizeof(recvline));
	int recv_used = 0;
	int result;
	enum RecvFsm readyrecv = PKTSTART1;
	int finishreading = 0;
	while (1) {
        char ch;
		//printf("waiting\n");
        result = recv(conn, &ch, 1, 0);
        //断开连接或者出错
		//printf("has something\n");
        if (result == 0) {
			return -1;
            break;
        } else if (result == -1) {
            printf("read error\n");
			return -1;
            break;
        }
        switch (readyrecv)
		{
		case PKTSTART1:
		{
			if(ch == '!') readyrecv = PKTSTART2;
			break;
		}
		case PKTSTART2:
		{
			if(ch == '&') readyrecv = PKTRECV;
			else if(ch == '!'){

			}
			else	readyrecv = PKTSTART1;
			break;
		}
		case PKTRECV:
		{
			recvline[recv_used++] = ch;
			if(ch == '!') readyrecv = PKTSTOP;
			break;
		}
		case PKTSTOP:
		{
			if(ch == '#') {
				recvline[recv_used - 1] = '\0';
				finishreading = 1;
			}
			else if(ch == '!'){
				recvline[recv_used++] = ch;
			}
			else{
				recvline[recv_used++] = ch;
				readyrecv = PKTRECV;
			}
			break;
		}
		default:
			break;
		}
		if(finishreading == 1) break;
    }
	//printf("finish listening\n");
	memcpy(pkt,recvline,sizeof(sip_pkt_t));
	memset(recvline,0,sizeof(recvline));
	return 1;
}
