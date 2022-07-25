//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 

#include "topology.h"

int Hostname_to_ip(char * hostname , char* ip)
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


//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
  char ip[100];
	Hostname_to_ip(hostname , ip);
  return topology_getNodeIDfromip(ip);
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(char* addr)
{
  char* ip = addr;
  char *p;
  for(int i = 0;i<4;i++){
    if(i == 0)
	    p = strtok(ip,".");
    else
      p = strtok(NULL,".");
    if (p)
    {
		  //printf("%s\n",p);
    }
    else return -1;
  }

  return atoi(p);
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
  int fd;
  struct ifreq ifr;

  fd = socket(AF_INET, SOCK_DGRAM, 0);

 /* I want to get an IPv4 IP address */
  ifr.ifr_addr.sa_family = AF_INET;

 /* I want IP address attached to "eth0" */
  strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

  ioctl(fd, SIOCGIFADDR, &ifr);

  close(fd);

 /* display result */
  //printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
  return topology_getNodeIDfromip(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
  char hostname[256];
  gethostname(hostname,256);
  int size = 0;
  FILE* f = fopen("../topology/topology.dat", "r");
  if(f == NULL){
    printf("error open\n");
  }
  char data1[20];
  char data2[20];
  char data3[20];
  while(!feof(f)){
    fscanf(f,"%s",data1);
    fscanf(f,"%s",data2);
    int end = fscanf(f,"%s",data3);
    //printf("%s\n",data);
    if(end == -1) break;
    if(strcmp(data1,hostname) == 0)
      size ++;
    else if(strcmp(data2,hostname) == 0)
      size++;
  }
  fclose(f);
  return size;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
  return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
  return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.
int* topology_getNbrArray()
{
  return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
  return 0;
}
