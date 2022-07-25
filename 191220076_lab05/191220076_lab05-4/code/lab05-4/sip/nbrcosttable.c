
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
  nbr_cost_entry_t* mynbrcosttable = (nbr_cost_entry_t*)malloc(sizeof(nbr_cost_entry_t)*sip_neighbor_num);
  for(int i=0;i<sip_neighbor_num;i++){
    mynbrcosttable[i].cost = sip_nt[i].dis;
    mynbrcosttable[i].nodeID = sip_nt[i].nodeID;
  }
  return mynbrcosttable;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
  free(nct);
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
  for(int i = 0;i<sip_neighbor_num;i++){
    if(nodeID == nct[i].nodeID)
      return nct[i].cost;
  }
  return INFINITE_COST;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
  for(int i = 0;i<sip_neighbor_num;i++){
    printf("neighbor: %d  cost: %d\n",nct[i].nodeID,nct[i].cost);
  }
}
