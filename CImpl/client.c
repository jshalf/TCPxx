/* client.c */
#include "tcp_utils.h"
#include <stdio.h>
#include <stdlib.h>

main()
{
	char *buffer,resptype;
	char buff[255],request[64];
	int sock;
	int i,j,index;
	int revnum,ticketnum,portnum;
       
	sock=UDPOpenClientSock("bach",5138);
	if(sock<0) {perror("bad socket"); exit(0); }

	strcpy(request,"r:0:image:1:gif:image:1:tiff:");
	printf("client:%s\n",request);
	write(sock,request,strlen(request));
	puts("message sent");
	/* while(read(sock,buff,1)>0)
	      printf("char: %c\n",buff[0]);*/
	read(sock,buff,5);
	puts("message recieved");
	printf("server:%s\n",buff);
	sscanf(buffer,"%c:%u:%u:%u:",
	       &resptype,&revnum,&ticketnum,&portnum);
	printf("rev=%u, ticket=%u, port=%u\n",revnum,ticketnum,portnum);
	close(sock);
	printf("closed socket, reopen tcp to %u\n");
	sock=TCPOpenClientSock("bach",portnum);
	/* 
	   CvtRequest(sock,sourcetype,desttype,nimages);
	   again=1;
	   while(again) { again=0; switch(reply=CvtParseReply(sock))
		case QUEUED: PrintMSG(stdout,reply)
			again=1;
			break;
		case ACCEPTED: break;
	  }
	  close(sock);
	  sock=TCPOpenClientSock("bach",CvtReturnAddress(reply));
	*/
	if(sock>=0) puts("socket successfully opened");
	else{
	  perror("socket open failed. Exiting");
	  exit(0);
	}
	i=25; j=8;
	printf("writing i=%u,j=%u\n",i,j);
	TCPBlockingWrite(sock,(char *)&i,sizeof(int));
	TCPBlockingWrite(sock,(char *)&j,sizeof(int));
	buffer=(char *)malloc(i*j);
	for(index=0;index<i*j;index++)
		buffer[index]=index;
	puts("sending i,j data");
	TCPBlockingWrite(sock,buffer,i*j);
	puts("send complete");
	close(sock);
}
