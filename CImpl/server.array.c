/* server.c */
#include "tcp_utils.h"
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
int done;

main()
{
	void doneinc();
	char *buffer;
	int sock,newsock,fromlen;
	struct sockaddr_in fsin;
	int i,j,index;
	sock=TCPOpenServerSock(5138);
	if(sock<0) {perror("bad socket"); exit(0); }
	signal(SIGINT,doneinc);
	for(done=0;!done;)
	{
		fromlen=sizeof(fsin);
		newsock=accept(sock,&fsin,&fromlen);
		printf("accepted %d from %d\n",newsock,sock);
		if(newsock<0)
			{ perror("accept: bad socket number");
			  continue;
			}
		if(done) {puts("interrupted"); shutdown(newsock,2); break; }
		puts("socket connection opened");
		TCPBlockingRead(newsock,&i,sizeof(int));
		TCPBlockingRead(newsock,&j,sizeof(int));
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
}

void doneinc(sig)
int sig;
{
	puts("interrupt");
	done++;
}
