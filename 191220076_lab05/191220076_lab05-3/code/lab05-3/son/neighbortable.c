//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API

#include "neighbortable.h"

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
int get_neightbor(){
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

int hostname_to_ip(char * hostname , char* ip)
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

nbr_entry_t* nt_create()
{
  //printf("good\n");
  
  char hostname[256];
  gethostname(hostname,256);
  printf("%s\n",hostname);
	char ip[100];
	hostname_to_ip(hostname , ip);
  strcpy(myip,ip);
	printf("%s resolved to %s\n" , hostname , ip);
  myid = topology_getNodeIDfromip(ip);
  int size = get_neightbor();
  printf("%d\n",size);
  char data1[100];
  char data2[100];
  char data3[10];
  nbr_entry_t* mytable = (nbr_entry_t*) malloc(size * sizeof(nbr_entry_t));
  int pointer = 0;
  FILE* f = fopen("../topology/topology.dat", "r");
  if(f == NULL){
    printf("error open\n");
  }
  while(!feof(f)){
    fscanf(f,"%s",data1);
    fscanf(f,"%s",data2);
    //printf("data1 = %s\n",data1);
    //printf("data2 = %s\n",data2);
    int end = fscanf(f,"%s",data3);
    //printf("%s\n",data);
    if(end == -1) break;
    if(strcmp(data1,hostname) == 0){
      mytable[pointer].conn = -1;
      hostname_to_ip(data2,mytable[pointer].nodeIP);
      //printf("%s\n",mytable[pointer].nodeIP);
      char temp[20];
      strcpy(temp,mytable[pointer].nodeIP);
      mytable[pointer].nodeID = topology_getNodeIDfromip(temp);
      //printf("%d\n",mytable[pointer].nodeID);
      pointer++;
    }
    else if(strcmp(data2,hostname) == 0){
      mytable[pointer].conn = -1;
      hostname_to_ip(data1,mytable[pointer].nodeIP);
       //printf("%s\n",mytable[pointer].nodeIP);
      char temp[20];
      strcpy(temp,mytable[pointer].nodeIP);
      mytable[pointer].nodeID = topology_getNodeIDfromip(temp);
      //printf("%d\n",mytable[pointer].nodeID);
      pointer++;
    }
  }

  fclose(f);
  return mytable;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
  return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
  return 0;
}
