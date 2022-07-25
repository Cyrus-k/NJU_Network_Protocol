
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"
#include "sip.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
  return node%KEY;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{
  routingtable_t* myroutingtable = (routingtable_t*)malloc(sizeof(routingtable_t));
  for(int i = 0;i < MAX_ROUTINGTABLE_SLOTS;i++){
    myroutingtable->hash[i] = NULL;
  }
  for(int i = 0;i < sip_neighbor_num;i++){
    int key = makehash(sip_nt[i].nodeID);
    routingtable_entry_t* newnode = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
    newnode->destNodeID = sip_nt[i].nodeID;
    newnode->nextNodeID = sip_nt[i].nodeID;
    newnode->next = myroutingtable->hash[i];
    myroutingtable->hash[key] = newnode;
  }
  return myroutingtable;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
  for(int i = 0;i < MAX_ROUTINGTABLE_SLOTS;i++){
    if(routingtable->hash[i] != NULL){
      routingtable_entry_t* temp = routingtable->hash[i];
      routingtable_entry_t* head = routingtable->hash[i];
      while(head){
        temp = head;
        head = head->next;
        free(temp);
      }
    }
  }
  free(routingtable);
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
  int key = makehash(destNodeID);
  bool isexist = false;
  routingtable_entry_t* temp = routingtable->hash[key];
  while(temp){
    if(temp->destNodeID == destNodeID){
      isexist = true;
      break;
    }
    temp = temp->next;
  }
  if(isexist == true){
    temp->nextNodeID = nextNodeID;
  }
  else{
    routingtable_entry_t* newnode = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
    newnode->destNodeID = destNodeID;
    newnode->nextNodeID = nextNodeID;
    newnode->next = routingtable->hash[key];
    routingtable->hash[key] = newnode;
  }
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
  int key = makehash(destNodeID);
  routingtable_entry_t* temp = routingtable->hash[key];
  while(temp){
    if(temp->destNodeID == destNodeID)
      return temp->nextNodeID;
    temp = temp->next;
  }
  return -1;
}

void routingtable_deletenode(routingtable_t* routingtable, int destNodeID){
  int key = makehash(destNodeID);
  routingtable_entry_t* temp = routingtable->hash[key];
  while(temp){
    if(temp->destNodeID == destNodeID)
      break;
    temp = temp->next;
  }
  routingtable_entry_t* pre = routingtable->hash[key];
  while(pre->next){
    if(pre->next->destNodeID == destNodeID)
      break;
    pre = pre->next;
  }
  if(pre->destNodeID == destNodeID){
    free(temp);
    routingtable->hash[key] = NULL;
  }
  else{
    pre->next = temp->next;
    free(temp);
  }
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
  for(int i = 0;i < MAX_ROUTINGTABLE_SLOTS;i++){
    if(routingtable->hash[i] != NULL){
      routingtable_entry_t* head = routingtable->hash[i];
      while(head){
        printf("destnode: %d, next hop: %d\n",head->destNodeID,head->nextNodeID);
        head = head->next;
      }
    }
  }
}
