/* client.c */
#include "tcp_utils.h"
#include <stdio.h>
#include <stdlib.h>

main()
{
	char *buffer;
	int sock;
	int i,j,index;
	sock=TCPOpenClientSock("bach",5138);
	if(sock>=0) puts("socket successfully opened");
	else{
	  perror("socket open failed. Exiting");
	  exit(0);
	}
	i=25; j=8;
	printf("writing i=%u,j=%u\n",i,j);
	TCPBlockingWrite(sock,&i,sizeof(int));
	TCPBlockingWrite(sock,&j,sizeof(int));
	buffer=(char *)malloc(i*j);
	for(index=0;index<i*j;index++)
		buffer[index]=index;
	puts("sending i,j data");
	TCPBlockingWrite(sock,buffer,i*j);
	puts("send complete");
	close(sock);
}
