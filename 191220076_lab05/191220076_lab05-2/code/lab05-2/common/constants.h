//�ļ���: common/constants.h

//����: ����ļ�����STCPЭ��ʹ�õĳ���

#ifndef CONSTANTS_H
#define CONSTANTS_H

//�������򿪵��ص������˿ں�. �ͻ��˽����ӵ�����˿�. 
#define SON_PORT 9009
//����STCP����֧�ֵ����������. ���TCB��Ӧ����MAX_TRANSPORT_CONNECTIONS����Ŀ.
#define MAX_TRANSPORT_CONNECTIONS 10
//���γ���
//MAX_SEG_LEN = 1500 - sizeof(stcp header) - sizeof(sip header)
#define MAX_SEG_LEN  1464
//���ݰ���ʧ��Ϊ10%
#define PKT_LOSS_RATE 0.1
//SYN_TIMEOUTֵ, ��λΪ����
#define SYN_TIMEOUT 1000000000
//FIN_TIMEOUTֵ, ��λΪ����
#define FIN_TIMEOUT 1000000000
//stcp_client_connect()�е����SYN�ش�����
#define SYN_MAX_RETRY 5
//stcp_client_disconnect()�е����FIN�ش�����
#define FIN_MAX_RETRY 5
//������CLOSEWAIT��ʱֵ, ��λΪ��
#define CLOSEWAIT_TIMEOUT 1
//sendBuf_timer�̵߳���ѯ���, ��λΪ����
#define SENDBUF_POLLING_INTERVAL 100000000
//STCP�ͻ�����stcp_server_recv()������ʹ�����ʱ��������ѯ���ջ�����, �Լ���Ƿ������������ȫ������, ��λΪ��. 
#define RECVBUF_POLLING_INTERVAL 1
//stcp_server_accept()����ʹ�����ʱ������æ�ȴ�TCB״̬ת��, ��λΪ����
#define ACCEPT_POLLING_INTERVAL 100000000
//���ջ�������С
#define RECEIVE_BUF_SIZE 1000000
//���ݶγ�ʱֵ, ��λΪ����
#define DATA_TIMEOUT 100000000
//GBN���ڴ�С
#define GBN_WINDOW 10

#define MAXLINE 1600
#endif
