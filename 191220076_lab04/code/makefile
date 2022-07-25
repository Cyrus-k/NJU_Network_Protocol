all:gameclient gameserver
.PHONY:all 

gameclient:gameclient.o
	gcc gameclient.o -o gameclient -lpthread
gameserver:gameserver.o
	gcc gameserver.o -o gameserver -lpthread

gameclient.o:gameclient.c
	gcc -c gameclient.c -o gameclient.o

gameserver.o:gameserver.c
	gcc -c gameserver.c -o gameserver.o

.PHONY:clean
clean:
	rm *.o
	rm gameclient
	rm gameserver