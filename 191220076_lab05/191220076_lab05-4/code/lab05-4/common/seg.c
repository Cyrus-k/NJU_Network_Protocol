
#include "seg.h"

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
  int ret = 0;
	char sendline[MAXLINE];
	char tempbuf[MAXLINE];
  sendseg_arg_t sendpkt;
	seg_t temp = *segPtr;
	unsigned short checknum = checksum(&temp);
	temp.header.checksum = checknum;
  sendpkt.seg = temp;
  sendpkt.nodeID = dest_nodeID;
	//printf("get %d\n",temp.header.dest_port);
  memcpy(tempbuf,&sendpkt,sizeof(sendseg_arg_t));
	//printf("%d\n",tempbuf[4]);	
  char head[3] = "!&";
	char tail[3] = "!#";
	memcpy(sendline,head,sizeof(head));
	memcpy(sendline+2,tempbuf,sizeof(sendseg_arg_t));
	memcpy(sendline+2+sizeof(sendseg_arg_t),tail,sizeof(tail));
	//printf("%s\n",sendline);
	//printf("good\n");
	if(send(sip_conn,sendline,sizeof(sendline),0))
    {
        //printf("send information\n");
		ret = 1;
    }
	memset(sendline,0,sizeof(sendline));
	memset(tempbuf,0,sizeof(tempbuf));
	//printf("**********************DESTNODEID = %d\n",sendpkt.nodeID);
	return ret;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
enum RecvFsm{SEGSTART1,SEGSTART2,SEGRECV,SEGSTOP};

int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
  sendseg_arg_t recvpkt;
  char recvline[MAXLINE];
	memset(recvline,0,sizeof(recvline));
	int recv_used = 0;
	int result;
	enum RecvFsm readyrecv = SEGSTART1;
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
		case SEGSTART1:
		{
			if(ch == '!') readyrecv = SEGSTART2;
			break;
		}
		case SEGSTART2:
		{
			if(ch == '&') readyrecv = SEGRECV;
			else if(ch == '!'){

			}
			else	readyrecv = SEGSTART1;
			break;
		}
		case SEGRECV:
		{
			recvline[recv_used++] = ch;
			if(ch == '!') readyrecv = SEGSTOP;
			break;
		}
		case SEGSTOP:
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
				readyrecv = SEGRECV;
			}
			break;
		}
		default:
			break;
		}
		if(finishreading == 1) break;
  }
  	memcpy(&recvpkt,recvline,sizeof(sendseg_arg_t));
  	*src_nodeID = recvpkt.nodeID;
	seg_t temp = recvpkt.seg;
	if(seglost(&temp) == 0)
	{
		//printf("not lost\n");
		if(checkchecksum(&temp) == 1){
			//printf("not checksum\n");
			memcpy(segPtr,&temp,sizeof(seg_t));
			//printf("receive acknum %d\n",segPtr->header.ack_num);
		}
		else printf("wrong checksum\n");
	}
	else
		printf("lose packet\n");
	//memcpy(segPtr,recvline,sizeof(seg_t));	
	//printf("%d\n",segPtr->header.dest_port);
	memset(recvline,0,sizeof(recvline));
	return 1;
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
  sendseg_arg_t recvpacket;
  char recvline[MAXLINE];
	memset(recvline,0,sizeof(recvline));
	int recv_used = 0;
	int result;
	enum RecvFsm readyrecv = SEGSTART1;
	int finishreading = 0;
	while (1) {
        char ch;
		//printf("waiting\n");
        result = recv(stcp_conn, &ch, 1, 0);
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
		case SEGSTART1:
		{
			if(ch == '!') readyrecv = SEGSTART2;
			break;
		}
		case SEGSTART2:
		{
			if(ch == '&') readyrecv = SEGRECV;
			else if(ch == '!'){

			}
			else	readyrecv = SEGSTART1;
			break;
		}
		case SEGRECV:
		{
			recvline[recv_used++] = ch;
			if(ch == '!') readyrecv = SEGSTOP;
			break;
		}
		case SEGSTOP:
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
				readyrecv = SEGRECV;
			}
			break;
		}
		default:
			break;
		}
		if(finishreading == 1) break;
    }
	//printf("finish listening\n");
	memcpy(&recvpacket,recvline,sizeof(sendseg_arg_t));
  memcpy(segPtr,&(recvpacket.seg),sizeof(seg_t));
  *dest_nodeID = recvpacket.nodeID;
  //printf("recvpkt destnode %d...............\n",recvpacket.nodeID);
	memset(recvline,0,sizeof(recvline));
  //free(recvpacket);
	return 1;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
  int ret = 0;
	char sendline[MAXLINE];
	char tempbuf[MAXLINE];
  sendseg_arg_t sendpkt;
	seg_t temp = *segPtr;
	//unsigned short checknum = checksum(&temp);
	//temp.header.checksum = checknum;
	//printf("get %d\n",temp.header.dest_port);
  sendpkt.seg = temp;
  sendpkt.nodeID = src_nodeID;
  memcpy(tempbuf,&sendpkt,sizeof(sendseg_arg_t));
	//printf("%d\n",tempbuf[4]);	
  char head[3] = "!&";
	char tail[3] = "!#";
	memcpy(sendline,head,sizeof(head));
	memcpy(sendline+2,tempbuf,sizeof(sendseg_arg_t));
	memcpy(sendline+2+sizeof(sendseg_arg_t),tail,sizeof(tail));
	//printf("%s\n",sendline);
	//printf("good\n");
	if(send(stcp_conn,sendline,sizeof(sendline),0))
  {
        //printf("send information\n");
		ret = 1;
  }
	memset(sendline,0,sizeof(sendline));
	memset(tempbuf,0,sizeof(tempbuf));
	return ret;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr)
{
  int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
      return 1;
		}
		//50%可能性是错误的校验和
		else {
			//printf("checksum wrong\n");
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
  unsigned int sum = 0;
	unsigned short* p = (unsigned short*)segment;
	int length = 12 + segment->header.length / 2;
	if(segment->header.length%2 != 0)
		length = length + 1;
	for(int i=0;i<length;i++)
		sum = sum + p[i];
	while(sum >> 16)
		sum = (unsigned short)(sum >> 16) + (sum & 0xffff);
	//printf("checksum is %x\n",(unsigned short)~sum);
	return (unsigned short)~sum;
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{
  unsigned int sum = 0;
	unsigned short* p = (unsigned short*)segment;
	int length = 12 + segment->header.length / 2;
	if(segment->header.length%2 != 0)
		length = length + 1;
	for(int i=0;i<length;i++)
		sum = sum + p[i];
	while(sum >> 16)
		sum = (unsigned short)(sum >> 16) + (sum & 0xffff);
	//printf("receive checksum is %x\n",(unsigned short)segment->header.checksum);
	//printf("cal checksum is%x\n",(unsigned short)sum);
	if((unsigned short)sum != 0xffff)
		return -1;
	else
		return 1;
}
