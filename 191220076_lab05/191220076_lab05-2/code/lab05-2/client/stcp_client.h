//
// �ļ���: stcp_client.h
//
// ����: ����ļ������ͻ���״̬����, һЩ��Ҫ�����ݽṹ�Ϳͻ���STCP�׽��ֽӿڶ���. ����Ҫʵ��������Щ�ӿ�.

#ifndef STCPCLIENT_H
#define STCPCLIENT_H

#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include "../common/seg.h"


#define NOT_USED 5
#define USED 6
//FSM��ʹ�õĿͻ���״̬
#define	CLOSED 1
#define	SYNSENT 2
#define	CONNECTED 3
#define	FINWAIT 4

//�ڷ��ͻ����������д洢�εĵ�Ԫ.
typedef struct segBuf {
        seg_t seg;
        unsigned int sentTime;
        struct segBuf* next;
} segBuf_t;

//�ͻ��˴�����ƿ�. һ��STCP���ӵĿͻ���ʹ��������ݽṹ��¼������Ϣ.   
typedef struct client_tcb {
	unsigned int server_nodeID;        //�������ڵ�ID, ����IP��ַ, ��ǰδʹ��
	unsigned int server_portNum;       //�������˿ں�
	unsigned int client_nodeID;     //�ͻ��˽ڵ�ID, ����IP��ַ, ��ǰδʹ��
	unsigned int client_portNum;    //�ͻ��˶˿ں�
	unsigned int state;     	//�ͻ���״̬
	unsigned int is_used;
	unsigned int next_seqNum;       //�¶�׼��ʹ�õ���һ����� 
	pthread_mutex_t* bufMutex;      //���ͻ�����������
	segBuf_t* sendBufHead;          //���ͻ�����ͷ
	segBuf_t* sendBufunSent;        //���ͻ������еĵ�һ��δ���Ͷ�
	segBuf_t* sendBufTail;          //���ͻ�����β
	unsigned int unAck_segNum;      //�ѷ��͵�δ�յ�ȷ�϶ε�����
} client_tcb_t;

client_tcb_t ClientTCB[MAX_TRANSPORT_CONNECTIONS];
int Sockfd;
pthread_mutex_t mutex_syn;
extern bool isSYNACK;

pthread_mutex_t mutex_fin;
extern bool isFINACK;


//
//  ���ڿͻ���Ӧ�ó����STCP�׽���API. 
//  ===================================
//
//  �����������ṩ��ÿ���������õ�ԭ�Ͷ����ϸ��˵��, ����Щֻ��ָ���Ե�, ����ȫ���Ը����Լ����뷨����ƴ���.
//
//  ע��: ��ʵ����Щ����ʱ, ����Ҫ����FSM�����п��ܵ�״̬, �����ʹ��switch�����ʵ��. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn);

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL.  
// ��������ص�����TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, �ñ�����Ϊsip_sendseg��sip_recvseg���������.
// ���, �����������seghandler�߳������������STCP��. �ͻ���ֻ��һ��seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port);

// ����������ҿͻ���TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��. ����, TCB state������ΪCLOSED���ͻ��˶˿ڱ�����Ϊ�������ò���client_port. 
// TCB������Ŀ��������Ӧ��Ϊ�ͻ��˵����׽���ID�������������, �����ڱ�ʶ�ͻ��˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int socked, unsigned int server_port);

// ��������������ӷ�����. �����׽���ID�ͷ������Ķ˿ں���Ϊ�������. �׽���ID�����ҵ�TCB��Ŀ.  
// �����������TCB�ķ������˿ں�,  Ȼ��ʹ��sip_sendseg()����һ��SYN�θ�������.  
// �ڷ�����SYN��֮��, һ����ʱ��������. �����SYNSEG_TIMEOUTʱ��֮��û���յ�SYNACK, SYN �ν����ش�. 
// ����յ���, �ͷ���1. ����, ����ش�SYN�Ĵ�������SYN_MAX_RETRY, �ͽ�stateת����CLOSED, ������-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length);

// �������ݸ�STCP������. �������ʹ���׽���ID�ҵ�TCB���е���Ŀ.
// Ȼ����ʹ���ṩ�����ݴ���segBuf, �������ӵ����ͻ�����������.
// ������ͻ������ڲ�������֮ǰΪ��, һ����Ϊsendbuf_timer���߳̾ͻ�����.
// ÿ��SENDBUF_ROLLING_INTERVALʱ���ѯ���ͻ������Լ���Ƿ��г�ʱ�¼�����. 
// ��������ڳɹ�ʱ����1�����򷵻�-1. 
// stcp_client_send��һ����������������.
// ��Ϊ�û����ݱ���ƬΪ�̶���С��STCP��, ����һ��stcp_client_send���ÿ��ܻ�������segBuf
// �����ӵ����ͻ�����������. ������óɹ�, ���ݾͱ�����TCB���ͻ�����������, ���ݻ������ڵ����,
// ���ݿ��ܱ����䵽������, ���ڶ����еȴ�����.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd);

// ����������ڶϿ���������������. �����׽���ID��Ϊ�������. �׽���ID�����ҵ�TCB���е���Ŀ.  
// �����������FIN�θ�������. �ڷ���FIN֮��, state��ת����FINWAIT, ������һ����ʱ��.
// ��������ճ�ʱ֮ǰstateת����CLOSED, �����FINACK�ѱ��ɹ�����. ����, ����ھ���FIN_MAX_RETRY�γ���֮��,
// state��ȻΪFINWAIT, state��ת����CLOSED, ������-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd);

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg);

// ������stcp_client_init()�������߳�. �������������Է������Ľ����. 
// seghandler�����Ϊһ������sip_recvseg()������ѭ��. ���sip_recvseg()ʧ��, ��˵���ص����������ѹر�,
// �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���. ��鿴�ͻ���FSM���˽����ϸ��.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//


void* sendBuf_timer(void* clienttcb);

// ����̳߳�����ѯ���ͻ������Դ�����ʱ�¼�. ������ͻ������ǿ�, ��Ӧһֱ����.
// ���(��ǰʱ�� - ��һ���ѷ��͵�δ��ȷ�϶εķ���ʱ��) > DATA_TIMEOUT, �ͷ���һ�γ�ʱ�¼�.
// ����ʱ�¼�����ʱ, ���·��������ѷ��͵�δ��ȷ�϶�. �����ͻ�����Ϊ��ʱ, ����߳̽���ֹ.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

#endif
