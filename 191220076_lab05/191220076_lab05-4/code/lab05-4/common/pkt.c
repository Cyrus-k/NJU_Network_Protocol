// �ļ��� pkt.c

#include "pkt.h"
enum RecvFsm{PKTSTART1,PKTSTART2,PKTRECV,PKTSTOP};
// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
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

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
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

// ���������SON���̵���, �������ǽ������ݽṹsendpkt_arg_t.
// ���ĺ���һ���Ľڵ�ID����װ��sendpkt_arg_t�ṹ.
// ����sip_conn����SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// sendpkt_arg_t�ṹͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ɹ�����sendpkt_arg_t�ṹ, ����1, ���򷵻�-1.
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

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
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

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
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

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
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
