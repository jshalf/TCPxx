/* server.c */
#include "tcp_utils.h"
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

int done;

void doneinc(int sig)
{
  puts("interrupt");
  done++;
}


int main(int argc,char *argv[])
{
  /* void doneinc(); */
  char *buffer,buff[255];
  socklen_t sock,newsock,fromlen;
  struct sockaddr_in fsin;
  int i,j,index;
  int ticket=0,tcpport=5139,rev=1;
  int mainsock;
  
  
  
  mainsock=UDPOpenServerSock(5138);
  if(mainsock<0) {perror("bad socket"); exit(0); }
  
  sock=TCPOpenServerSock(5139);
  if(sock<0) {perror("bad socket"); exit(0); }
  signal(SIGINT,doneinc);
  
  signal(SIGINT,doneinc);
  for(done=0;!done;){
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen;
    
    recvfrom(mainsock,buff,sizeof(buff),0,
	     (struct sockaddr *)&clientaddr,&clientaddrlen);
    printf("client:%s\n",buff);
    sprintf(buff,"a:%u:%u:%u:",rev,ticket,tcpport);
    printf("server:%s\n",buff);
    sendto(mainsock,buff,strlen(buff)+1,0,
	   (struct sockaddr *)&clientaddr,sizeof(clientaddr));
    puts("message sent");
    /* flush(mainsock); */
    fromlen=sizeof(fsin);
    newsock=accept(sock,(struct sockaddr *)&fsin,&fromlen);
    printf("accepted %d from %d\n",newsock,sock);
    if(newsock<0)
      { perror("accept: bad socket number");
      continue;
      }
    if(done) {puts("interrupted"); shutdown(newsock,2); break; }
    puts("socket connection opened");
    TCPBlockingRead(newsock,(char *)&i,sizeof(int));
    TCPBlockingRead(newsock,(char *)&j,sizeof(int));
    if(done) {puts("interrupted"); shutdown(newsock,2); break; }
    printf("recieved i=%u j=%u\n",i,j);
    buffer=(char *)malloc(i*j);
    printf("reading %u bytes\n",i*j);
    if(done) {puts("interrupted"); shutdown(newsock,2); break; }
    TCPBlockingRead(newsock,buffer,i*j);
    if(done) {puts("interrupted"); shutdown(newsock,2); break; }
    for(index=0;index<100 && index<i*j;index++)
      printf("%u: %u\n",index,buffer[index]);
    puts("done");
    close(newsock);
  }
  close(mainsock);
    return 0;
}

