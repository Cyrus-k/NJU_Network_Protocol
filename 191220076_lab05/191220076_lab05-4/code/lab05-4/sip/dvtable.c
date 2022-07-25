
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"
#include "sip.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
  dv_t* mydvtable = (dv_t*)malloc(sizeof(dv_t)*(1 + sip_neighbor_num));
  for(int i = 0;i < sip_neighbor_num + 1;i++){
    if(i == 0){
      mydvtable[i].nodeID = sip_myid;
      printf("%d\n",mydvtable[i].nodeID);
      dv_entry_t* destnode_group = (dv_entry_t*)malloc(sizeof(dv_entry_t)*all_num);
      allnode* cur = mynodetable;
      cur = cur->next;
      for(int j = 0;j < all_num;j++){
        destnode_group[j].nodeID = cur->nodeID;
        if(destnode_group[j].nodeID == mydvtable[i].nodeID)
          destnode_group[j].cost = 0;
        else
          destnode_group[j].cost = INFINITE_COST;
        cur = cur->next;
      }
      for(int j = 0;j < all_num;j++){
        for(int k = 0;k < sip_neighbor_num;k++){
          if(destnode_group[j].nodeID == sip_nt[k].nodeID){
            destnode_group[j].cost = sip_nt[k].dis;
            break;
          }
        }
      }
      mydvtable[i].dvEntry = destnode_group;
    }
    else{
      mydvtable[i].nodeID = sip_nt[i-1].nodeID;
      dv_entry_t* destnode_group = (dv_entry_t*)malloc(sizeof(dv_entry_t)*all_num);
      allnode* cur = mynodetable;
      cur = cur->next;
      for(int j = 0;j < all_num;j++){
        destnode_group[j].nodeID = cur->nodeID;
        destnode_group[j].cost = INFINITE_COST;
        cur = cur->next;
      }
      mydvtable[i].dvEntry = destnode_group;
    }
  }
  return mydvtable;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
  for(int i = 0;i < sip_neighbor_num + 1; i++){
    free(dvtable[i].dvEntry);
  }
  free(dvtable);
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
  return 0;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
  return 0;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
  for(int i = 0;i<sip_neighbor_num+1;i++){
    printf("nodeID %d: ",dvtable[i].nodeID);
    for(int j = 0;j<all_num;j++){
      printf("nodeID %d distance: %d    ",dvtable[i].dvEntry[j].nodeID,dvtable[i].dvEntry[j].cost);
    }
    printf("\n");
  }
}
