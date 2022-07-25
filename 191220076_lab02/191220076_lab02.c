#include "sysinclude.h"

extern void tcp_DiscardPkt(char* pBuffer, int type);

extern void tcp_sendIpPkt(unsigned char* pData, UINT16 len, unsigned int  srcAddr, unsigned int dstAddr, UINT8	ttl);

extern int waitIpPacket(char *pBuffer, int timeout);

extern unsigned int getIpv4Address();

extern unsigned int getServerIpv4Address();
/*以上都是调用函数的接口*/
//tcb控制块中的io种类，判断此时是接受包还是发送包
#define INPUT 0
#define OUTPUT 1
//是否已经初始化完成，对应的tcb块可以使用
#define NOT_READY 0
#define READY 1
//发送出去的包是否收到ack
#define DATA_NOT_ACKED 0
#define DATA_ACKED 1
//tcb块是否被使用
#define NOT_USED 0
#define USED 1
//支持最大的tcp的连接数
#define MAX_TCP_CONNECTIONS 5
//在这个实现中并没有使用到，我猜测是用来判断是接收状态的包头还是发送状态的包头
#define INPUT_SEG 0
#define OUTPUT_SEG 1

typedef int STATE;//状态

int gLocalPort = 2007;//本地端口
int gRemotePort = 2006;//远端服务端端口
int gSeqNum = 1234;//客户端初始序列号
int gAckNum = 0;//客户端ack序列号，初始化为0

enum TCP_STATES//客户端的不同状态，一共6个状态，见实验手册
{
	CLOSED,
	SYN_SENT,
	ESTABLISHED,
	FIN_WAIT1,
	FIN_WAIT2,
	TIME_WAIT,
};

struct MyTcpSeg//tcp报文段
{
	unsigned short src_port;//源端口
	unsigned short dst_port;//目的端口
	unsigned int seq_num;//序列号
	unsigned int ack_num;//确认号
	unsigned char hdr_len;//tcp报文段首部长度
	unsigned char flags;//数据包的属性
	unsigned short window_size;//接收窗口的长度
	unsigned short checksum;//检验和
	unsigned short urg_ptr;//紧急指针
	unsigned char data[2048];//数据
	unsigned short len;//总长度
};

struct MyTCB//tcb结构处理单个tcp活动
{
	STATE current_state;//目前客户端状态
	unsigned int local_ip;//本地ip地址
	unsigned short local_port;//本地端口号
	unsigned int remote_ip;//远程ip地址
	unsigned short remote_port;//远程端口号
	unsigned int seq;//seqnum
	unsigned int ack;//acknum
	unsigned char flags;//数据包属性
	int iotype;//这个控制块里的tcp包应该为输入还是输出
	int is_used;//是否被使用
	int data_ack;//是否已经收到ack
	unsigned char data[2048];//数据包数据
	unsigned short data_len;//数据长度
};

struct MyTCB gTCB[MAX_TCP_CONNECTIONS];//tcb链表（这里是数组形式）
int initialized = NOT_READY;//未初始化

int convert_tcp_hdr_ntoh(struct MyTcpSeg* pTcpSeg)//网络字节序转变为主机字节序 参数为一个tcp首部 内容直接调用库函数转换即可
{
	if( pTcpSeg == NULL )
	{
		return -1;
	}//空返回-1

	pTcpSeg->src_port = ntohs(pTcpSeg->src_port);
	pTcpSeg->dst_port = ntohs(pTcpSeg->dst_port);
	pTcpSeg->seq_num = ntohl(pTcpSeg->seq_num);
	pTcpSeg->ack_num = ntohl(pTcpSeg->ack_num);
	pTcpSeg->window_size = ntohs(pTcpSeg->window_size);
	pTcpSeg->checksum = ntohs(pTcpSeg->checksum);
	pTcpSeg->urg_ptr = ntohs(pTcpSeg->urg_ptr);
	//字节序转换
	return 0;
}

int convert_tcp_hdr_hton(struct MyTcpSeg* pTcpSeg)//主机字节序转变为网络字节序 参数为一个tcp首部 内容直接调用库函数转换即可
{
	if( pTcpSeg == NULL )
	{
		return -1;
	}

	pTcpSeg->src_port = htons(pTcpSeg->src_port);
	pTcpSeg->dst_port = htons(pTcpSeg->dst_port);
	pTcpSeg->seq_num = htonl(pTcpSeg->seq_num);
	pTcpSeg->ack_num = htonl(pTcpSeg->ack_num);
	pTcpSeg->window_size = htons(pTcpSeg->window_size);
	pTcpSeg->checksum = htons(pTcpSeg->checksum);
	pTcpSeg->urg_ptr = htons(pTcpSeg->urg_ptr);

	return 0;
}

unsigned short tcp_calc_checksum(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//计算要发送的包的检验和，第一个参数为了得到源和目的的ip地址，第二个用来计算tcp检验和
{
	int i = 0;
	int len = 0;
	unsigned int sum = 0;
	unsigned short* p = (unsigned short*)pTcpSeg;

	if( pTcb == NULL || pTcpSeg == NULL )
	{
		return 0;
	}

	for( i=0; i<10; i++)
	{
		sum += p[i];
	}//8位一截，累加到urg_ptr，计算tcp首部

	sum = sum - p[8] - p[6] + ntohs(p[6]);//由于结构体中有两个char类型，故这里转换字节序，有网络转主机

	if( (len = pTcpSeg->len) > 20 )//判断这个tcp包中是否有数据
	{
		if( len % 2 == 1 )
		{
			pTcpSeg->data[len - 20] = 0;
			len++;
		}//总长度奇数则末尾补零

		for( i=10; i<len/2; i++ )
		{
			sum += ntohs(p[i]);
		}//转换字节序之后，计算tcp数据的sum
	}

	sum = sum + (unsigned short)(pTcb->local_ip>>16)
		+ (unsigned short)(pTcb->local_ip&0xffff)
		+ (unsigned short)(pTcb->remote_ip>>16)
		+ (unsigned short)(pTcb->remote_ip&0xffff);//tcp伪头部，32位源ip以及目的ip
	sum = sum + 6 + pTcpSeg->len;//加上传输层协议号以及tcp报文总长度
	sum = ( sum & 0xFFFF ) + ( sum >> 16 );
	sum = ( sum & 0xFFFF ) + ( sum >> 16 );//对每16bit进行反码求和，本来为32位，故这里算两次即可

	return (unsigned short)(~sum);//再取反则为校验和
}

int get_socket(unsigned short local_port, unsigned short remote_port)//通过传入本地端口以及远端端口，从tcb链表中找到对应的tcb位置，返回sockfd（socket file descriptor）
{
	int i = 1;
	int sockfd = -1;

	for( i=1; i<MAX_TCP_CONNECTIONS; i++ )
	{
		if( gTCB[i].is_used == USED
			&& gTCB[i].local_port == local_port
			&& gTCB[i].remote_port == remote_port )
		{
			sockfd = i;
			break;
		}
	}

	return sockfd;
}

int tcp_init(int sockfd)//初始化这次tcp活动
{
	if( gTCB[sockfd].is_used == USED )
	{
		return -1;
	}//这个位置已经被使用

	gTCB[sockfd].current_state = CLOSED;
	gTCB[sockfd].local_ip = getIpv4Address();
	gTCB[sockfd].local_port = gLocalPort + sockfd - 1;
	gTCB[sockfd].seq = gSeqNum;
	gTCB[sockfd].ack = gAckNum;
	gTCB[sockfd].is_used = USED;
	gTCB[sockfd].data_ack = DATA_ACKED;//以上都是初始化操作

	return 0;//初始化成功
}

int tcp_construct_segment(struct MyTcpSeg* pTcpSeg, struct MyTCB* pTcb, unsigned short datalen, unsigned char* pData)//tcp包头的构建，创建成功则返回0，后面两个参数为tcp报文中可能要发送的数据以及数据长度
{
	pTcpSeg->src_port = pTcb->local_port;
	pTcpSeg->dst_port = pTcb->remote_port;
	pTcpSeg->seq_num = pTcb->seq;
	pTcpSeg->ack_num = pTcb->ack;
	pTcpSeg->hdr_len = (unsigned char)(0x50);
	pTcpSeg->flags = pTcb->flags;
	pTcpSeg->window_size = 1024;
	pTcpSeg->urg_ptr = 0;//初始化tcp报文段，pTcb中已经初始化好一部分内容，故这里有用pTcb中的值

	if( datalen > 0 && pData != NULL )
	{
		memcpy(pTcpSeg->data, pData, datalen);
	}//如果有数据要发送，那么复制到tcp结构中的数据字段

	pTcpSeg->len = 20 + datalen;//总长度为数据长度加上头部长度

	return 0;
}

int tcp_kick(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段以及tcb控制块，ip分组发送，以及更新tcb控制块中的序列号
{
	pTcpSeg->checksum = tcp_calc_checksum(pTcb, pTcpSeg);//检验和赋值

	convert_tcp_hdr_hton(pTcpSeg);//字节序转换
	
	tcp_sendIpPkt((unsigned char*)pTcpSeg, pTcpSeg->len, pTcb->local_ip, pTcb->remote_ip, 255);//ip分组发送函数
	//以下都是对tcb控制块做得操作，因为tcp包已经发送
	if( (pTcb->flags & 0x0f) == 0x00 )//flag后四位都为0，更改该tcb块的seq（加上数据字段的长度）
	{
		pTcb->seq += pTcpSeg->len - 20;
	}
	else if( (pTcb->flags & 0x0f) == 0x02 )//SYN = 1，发送连接请求（三次握手的第一次）
	{
		pTcb->seq++;//发送的序列号
	}
	else if( (pTcb->flags & 0x0f) == 0x01 )//FIN = 1，请求释放连接
	{
		pTcb->seq++;//发送的序列号
	}
	else if( (pTcb->flags & 0x3f) == 0x10 )//ACK = 1，接受，发送ack包或者连接建立之后发送报文，当然我觉得这里逻辑有问题，这个分支应该是永远不会被覆盖的，因为走到这里flag要为010000，但它会进入第一个分支
	{
	}

	return 0;
}

int tcp_closed(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段（准备发送的报文）以及tcb控制块，由closed状态变为syn-sent状态
{
	if( pTcb == NULL || pTcpSeg == NULL )
	{
		return -1;
	}

	if( pTcb->iotype != OUTPUT )//此时应该发送，故要output
	{
		//to do: discard packet

		return -1;
	}

	pTcb->current_state = SYN_SENT;
	pTcb->seq = pTcpSeg->seq_num ;//更改tcb状态以及seqnum

	tcp_kick( pTcb, pTcpSeg );

	return 0;
}

int tcp_syn_sent(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段（接受到的tcp报文）以及tcb控制块，由syn-sent状态变为established状态
{
	struct MyTcpSeg my_seg;

	if( pTcb == NULL || pTcpSeg == NULL )
	{
		return -1;
	}

	if( pTcb->iotype != INPUT )//应该接受包
	{
		return -1;
	}

	if( (pTcpSeg->flags & 0x3f) != 0x12 )//SYN = 1，ACK = 1，不满足该条件就丢弃
	{
		//to do: discard packet

		return -1;
	}

	pTcb->ack = pTcpSeg->seq_num + 1;//更新tcb控制块中的ack（期望下一次收到的seq号）ack = seq + 1
	pTcb->flags = 0x10;//三次握手中的最后一次，ACK = 1

	tcp_construct_segment( &my_seg, pTcb, 0, NULL );
	tcp_kick( pTcb, &my_seg );//构造tcp报文段并发送

	pTcb->current_state = ESTABLISHED;//建立

	return 0;
}

int tcp_established(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段（接受到的tcp报文）以及tcb控制块，由established状态变为FIN_WAIT1状态，或者仍在和客户端进行数据交互
{
	struct MyTcpSeg my_seg;

	if( pTcb == NULL || pTcpSeg == NULL )
	{
		return -1;
	}

	if( pTcb->iotype == INPUT )//此时为接受报文的状态
	{
		if( pTcpSeg->seq_num != pTcb->ack )//如果收到的包不是期望接收到的，则丢弃
		{
			tcp_DiscardPkt((char*)pTcpSeg, TCP_TEST_SEQNO_ERROR);
			//to do: discard packet

			return -1;
		}

		if( (pTcpSeg->flags & 0x3f) == 0x10 )//ACK = 1，复制包中的data内容
		{
			memcpy(pTcb->data, pTcpSeg->data, pTcpSeg->len - 20);
			pTcb->data_len = pTcpSeg->len - 20;

			if( pTcb->data_len == 0 )
			{
			}
			else//复制的内容不为空
			{
				pTcb->ack += pTcb->data_len;//下次期望收到的ack序列号
				pTcb->flags = 0x10;//设置flag
				tcp_construct_segment(&my_seg, pTcb, 0, NULL);
				tcp_kick(pTcb, &my_seg);//构造tcp报文段并发送
			}
		}
	}
	else//为发送报文的状态
	{
		if( (pTcpSeg->flags & 0x0F) == 0x01 )//FIN = 1，将要发送的报文为挥手报文，那么更改状态
		{
			pTcb->current_state = FIN_WAIT1;
		}

		tcp_kick( pTcb, pTcpSeg );
	}

	return 0;
}

int tcp_finwait_1(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段（接受到的tcp报文）以及tcb控制块，由FIN_WAIT1状态变为FIN_WAIT2状态
{
	if( pTcb == NULL || pTcpSeg == NULL )
	{
		return -1;
	}

	if( pTcb->iotype != INPUT )//此时应该为接收状态
	{
		return -1;
	}

	if( pTcpSeg->seq_num != pTcb->ack )//不是期望应该接收的包
	{
		tcp_DiscardPkt((char*)pTcpSeg, TCP_TEST_SEQNO_ERROR);

		return -1;
	}

	if( (pTcpSeg->flags & 0x3f) == 0x10 && pTcpSeg->ack_num == pTcb->seq )//检测收到的包ACK是否为1以及是否是应该收到的包
	{
		pTcb->current_state = FIN_WAIT2;
	}

	return 0;
}

int tcp_finwait_2(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段（接受到的tcp报文）以及tcb控制块，由FIN_WAIT2状态变为CLOSE状态
{
	struct MyTcpSeg my_seg;

	if( pTcb == NULL || pTcpSeg == NULL )
	{
		return -1;
	}

	if( pTcb->iotype != INPUT )//此时应该为接收状态
	{
		return -1;
	}

	if( pTcpSeg->seq_num != pTcb->ack )//不是期望应该接收的包
	{
		tcp_DiscardPkt((char*)pTcpSeg, TCP_TEST_SEQNO_ERROR);

		return -1;
	}

	if( (pTcpSeg->flags & 0x0f) == 0x01 )//检测收到的包FIN是否为1
	{
		pTcb->ack++;
		pTcb->flags = 0x10;

		tcp_construct_segment( &my_seg, pTcb, 0, NULL );//构造ack包（四次挥手中最后一次）
		tcp_kick( pTcb, &my_seg );//发送
		pTcb->current_state = CLOSED;//tcb状态进入close，不知道为啥不进入time_wait状态
	}
	else
	{
		//to do
	}

	return 0;
}

int tcp_time_wait(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段以及tcb控制块，tcb状态变为CLOSE状态
{
	pTcb->current_state = CLOSED;
	//to do

	return 0;
}

int tcp_check(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//计算要收到的包的检验和，检查在传输过程中是否出现了错误，第一个参数tcb为了得到源和目的的ip地址，第二个用来计算tcp检验和
{
	int i = 0;
	int len = 0;
	unsigned int sum = 0;
	unsigned short* p = (unsigned short*)pTcpSeg;
	unsigned short *pIp;
	unsigned int myip1 = pTcb->local_ip;
	unsigned int myip2 = pTcb->remote_ip;

	if( pTcb == NULL || pTcpSeg == NULL )
	{
		return -1;
	}

	for( i=0; i<10; i++)
	{
		sum = sum + p[i];
	}//8位一截，累加到urg_ptr，计算tcp首部
	sum = sum - p[6] + ntohs(p[6]);//处理数据结构的char，所以需要减去原来的p[6]，加上网络字节序转主机字节序的正确的和

	if( (len = pTcpSeg->len) > 20 )//有数据部分
	{
		if( len % 2 == 1 )
		{
			pTcpSeg->data[len - 20] = 0;
			len++;
		}//总长度奇数则末尾补零

		for( i=10; i<len/2; i++ )
		{
			sum += ntohs(p[i]);
		}//计算tcp报文数据部分的检验和
	}

	sum = sum + (unsigned short)(myip1>>16)
		+ (unsigned short)(myip1&0xffff)
		+ (unsigned short)(myip2>>16)
		+ (unsigned short)(myip2&0xffff);//tcp伪头部，32位源ip以及目的ip
	sum = sum + 6 + pTcpSeg->len;//加上传输层协议号以及tcp报文总长度

	sum = ( sum & 0xFFFF ) + ( sum >> 16 );
	sum = ( sum & 0xFFFF ) + ( sum >> 16 );//对每16bit进行反码求和，本来为32位，故这里算两次即可

	if( (unsigned short)(~sum) != 0 )//如果取反不为0，那么说明传输过程出错
	{
		// TODO:
		printf("check sum error!\n");

		return -1;
		//return 0;
	}
	else
	{
		return 0;
	}
}

int tcp_switch(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg)//参数为tcp报文段以及tcb控制块，根据tcb控制块的状态来判断该调用哪一个函数，进行状态转换
{
	int ret = 0;

	switch(pTcb->current_state)
	{
	case CLOSED:
		ret = tcp_closed(pTcb, pTcpSeg);
		break;
	case SYN_SENT:
		ret = tcp_syn_sent(pTcb, pTcpSeg);
		break;
	case ESTABLISHED:
		ret = tcp_established(pTcb, pTcpSeg);
		break;
	case FIN_WAIT1:
		ret = tcp_finwait_1(pTcb, pTcpSeg);
		break;
	case FIN_WAIT2:
		ret = tcp_finwait_2(pTcb, pTcpSeg);
		break;
	case TIME_WAIT:
		ret = tcp_time_wait(pTcb, pTcpSeg);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

int tcp_input(char* pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr)//完成校验和检查、字节序转换，实现客户端角色的 TCP 段接收的有限状态机，第一个参数收到的tcp报文段所有数据包括首部字段，第二个参数总长度，第三个参数源ip地址，第四个参数，目的ip地址
{
	struct MyTcpSeg tcp_seg;
	int sockfd = -1;

	if( len < 20 )//总长度不足20，即不超过tcp包头长度，显然错误
	{
		return -1;
	}

	memcpy(&tcp_seg, pBuffer, len);//数据复制

	tcp_seg.len = len;//总长度赋值

	convert_tcp_hdr_ntoh(&tcp_seg);//字节序转换

	sockfd = get_socket(tcp_seg.dst_port, tcp_seg.src_port);//得到对应的一个tcb控制块，返回socket file desciptor

	if( sockfd == -1 || gTCB[sockfd].local_ip != ntohl(dstAddr) || gTCB[sockfd].remote_ip != ntohl(srcAddr) )//返回的sockfd，即tcb控制块有误
	{
		printf("sock error in tcp_input()\n");
		return -1;
	}

	if( tcp_check(&gTCB[sockfd], &tcp_seg) != 0 )//对收到的包进行检验和检查
	{
		return -1;
	}

	gTCB[sockfd].iotype = INPUT;//此时设置为收到状态
	memcpy(gTCB[sockfd].data,tcp_seg.data,len - 20);//复制tcp包中的数据字段
	gTCB[sockfd].data_len = len - 20;//赋值数据长度

	tcp_switch(&gTCB[sockfd], &tcp_seg);//进行下一次状态转换以及tcp包的收发

	return 0;
}

void tcp_output(char* pData, unsigned short len, unsigned char flag, unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr, unsigned int dstAddr)//完成简单的 TCP 协议的封装发送功能，第一个参数为要发出的所有数据，第二个参数为数据长度，第三个参数为数据包的flag，决定发出的什么样的包，后四个参数为源ip，源端口，目的ip，目的端口
{
	struct MyTcpSeg my_seg;

	sockfd = get_socket(srcPort, dstPort);//得到对应的一个tcb控制块，返回socket file desciptor

	if( sockfd == -1 || gTCB[sockfd].local_ip != srcAddr || gTCB[sockfd].remote_ip != dstAddr )//返回的sockfd，即tcb控制块有误
	{
		return;
	}

	gTCB[sockfd].flags = flag;//设置flag

	tcp_construct_segment(&my_seg, &gTCB[sockfd], len, (unsigned char *)pData);//构造报文段

	gTCB[sockfd].iotype = OUTPUT;//此时设置为发出状态

	tcp_switch(&gTCB[sockfd], &my_seg);//进行下一次状态转换以及tcp包的收发
}

int tcp_socket(int domain, int type, int protocol)//获得 socket 描述符
{
	int i = 1;
	int sockfd = -1;

	if( domain != AF_INET || type != SOCK_STREAM || protocol != IPPROTO_TCP )//检查域，类型，以及默认协议
	{
		return -1;
	}

	for( i=1; i<MAX_TCP_CONNECTIONS; i++ )
	{
		if( gTCB[i].is_used == NOT_USED )//找一个空的tcb块，赋值socket描述符
		{
			sockfd = i;

			if( tcp_init(sockfd) == -1 )//初始化不成功，返回-1
			{
				return -1;
			}

			break;
		}
	}

	initialized = READY;//已经初始化完成

	return sockfd;
}

int tcp_connect(int sockfd, struct sockaddr_in* addr, int addrlen)//建立 TCP 连接，第一个参数为socket描述符，第二个参数为sockaddr_in结构体，包含address_family，32位ip地址等，第三个为地址长度
{
	char buffer[2048];
	int len;

	gTCB[sockfd].remote_ip = ntohl(addr->sin_addr.s_addr);
	gTCB[sockfd].remote_port = ntohs(addr->sin_port);//设置远端的ip地址以及tcp端口号

	tcp_output( NULL, 0, 0x02, gTCB[sockfd].local_port, gTCB[sockfd].remote_port, gTCB[sockfd].local_ip, gTCB[sockfd].remote_ip );//TCP 协议的封装发送功能，发送SYN（flag为0x02）

	len = waitIpPacket(buffer, 10);//IP 数据分组主动接收函数，得到接收到的数据长度

	if( len < 20 )
	{
		return -1;
	}//长度小于20，出错

	if (tcp_input(buffer, len, htonl(gTCB[sockfd].remote_ip), htonl(gTCB[sockfd].local_ip)) != 0){
		return 1;
	}
	else
	{
		return 0;
	}//接收到的数据给tcp_input函数处理
}

int tcp_send(int sockfd, const unsigned char* pData, unsigned short datalen, int flags)//向服务器发送数据，第一个参数socket描述符，后面参数为数据，数据长度，标志位
{
	char buffer[2048];
	int len;

	if( gTCB[sockfd].current_state != ESTABLISHED )
	{
		return -1;
	}//没建立好tcp连接，不能发送

	tcp_output((char *)pData, datalen, flags, gTCB[sockfd].local_port, gTCB[sockfd].remote_port, gTCB[sockfd].local_ip, gTCB[sockfd].remote_ip);//tcp封装发送

	len = waitIpPacket(buffer, 10);//得到返回数据长度，并把数据放入缓冲区

	if( len < 20 )
	{
		return -1;
	}//长度小于20，出错

	tcp_input(buffer, len, htonl(gTCB[sockfd].remote_ip), htonl(gTCB[sockfd].local_ip));//接收到的数据给tcp_input函数处理

	return 0;
}

int tcp_recv(int sockfd, unsigned char* pData, unsigned short datalen, int flags)//从服务器接收数据，第一个参数socket描述符，后面参数为数据，数据长度，标志位
{
	char buffer[2048];
	int len;

	if( (len = waitIpPacket(buffer, 10)) < 20 )
	{
		return -1;
	}//长度小于20，出错

	tcp_input(buffer, len,  htonl(gTCB[sockfd].remote_ip),htonl(gTCB[sockfd].local_ip));//接收到的数据给tcp_input函数处理

	memcpy(pData, gTCB[sockfd].data, gTCB[sockfd].data_len);//复制到tcb控制块中的数据区

	return gTCB[sockfd].data_len;//返回数据长度
}

int tcp_close(int sockfd)//关闭 TCP 连接，第一个参数为socket描述符
{
	char buffer[2048];
	int len;

	tcp_output(NULL, 0, 0x11, gTCB[sockfd].local_port, gTCB[sockfd].remote_port, gTCB[sockfd].local_ip, gTCB[sockfd].remote_ip);//发送SYN = 1，ACK = 1的tcp包

	if( (len = waitIpPacket(buffer, 10)) < 20 )
	{
		return -1;
	}//长度小于20，出错

	tcp_input(buffer, len, htonl(gTCB[sockfd].remote_ip), htonl(gTCB[sockfd].local_ip));//接收到的数据给tcp_input函数处理

	if( (len = waitIpPacket(buffer, 10)) < 20 )
	{
		return -1;
	}//长度小于20，出错
	//两次一次收自己发出去的ack包，一次收服务器发送过来的fin包
	tcp_input(buffer, len, htonl(gTCB[sockfd].remote_ip), htonl(gTCB[sockfd].local_ip));//接收到的数据给tcp_input函数处理

	gTCB[sockfd].is_used = NOT_USED;//连接关闭，tcb控制块变为不用

	return 0;
}
