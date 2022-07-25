//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 

#include "topology.h"


//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
  return 0;
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
  return 0;
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
