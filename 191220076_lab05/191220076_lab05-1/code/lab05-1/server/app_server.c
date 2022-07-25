//�ļ���: server/app_server.c

//����: ���Ƿ�����Ӧ�ó������. ����������ͨ���ڿͻ��˺ͷ�����֮�䴴��TCP����,�����ص������. Ȼ��������stcp_server_init()��ʼ��STCP������. 
//��ͨ�����ε���stcp_server_sock()��stcp_server_accept()����2���׽��ֲ��ȴ����Կͻ��˵�����. ���, ������ͨ������stcp_server_close()�ر��׽���.
//�ص������ͨ������son_stop()ֹͣ.

//����: ��

//���: STCP������״̬

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "../common/constants.h"
#include "stcp_server.h"

#define LISTENQ 8

//������������, һ��ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. ��һ��ʹ�ÿͻ��˶˿ں�89�ͷ������˿ں�90
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//�����Ӵ�����, �ȴ�10��, Ȼ��ر�����
#define WAITTIME 10

//这个函数通过在客户和服务器之间创建TCP连接来启动重叠网络层. 它返回TCP套接字描述符, STCP将使用该描述符发送段. 如果TCP连接失败, 返回-1.
int son_start() {
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
		return -1;
    }

    int rt2 = listen(listenfd, LISTENQ);
    if (rt2 < 0) {
        printf("listen failed ");
		return -1;
    }
	struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listenfd, (struct sockaddr *) &ss, &slen);
    if (fd < 0) {
        printf("error connect");
		return -1;
    }
    return fd;
}

//这个函数通过关闭客户和服务器之间的TCP连接来停止重叠网络层
void son_stop(int son_conn) {
	close(son_conn);
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//启动重叠网络层并获取重叠网络层TCP套接字描述符
	int son_conn = son_start();
	if(son_conn<0) {
		printf("can not start overlay network\n");
	}

	//初始化STCP服务器
	stcp_server_init(son_conn);
	
	//在端口SERVERPORT1上创建STCP服务器套接字 
	printf("init\n");
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd);

	//在端口SERVERPORT2上创建另一个STCP服务器套接字
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd2);

	sleep(WAITTIME);

	//关闭STCP服务器 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//停止重叠网络层
	son_stop(son_conn);
	printf("finish\n");
}
