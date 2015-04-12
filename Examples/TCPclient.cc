#include "RawTCP.hh"
#include <stdio.h>
#include <unistd.h>

int main(int argc,char *argv[]) {
  int nbytes=1024*128;
  char buffer[1024*128];
  char *hostname="localhost";
  int port = 7052;
  
  for(int i=0;i<nbytes;i++) buffer[i]=0;
  RawPort *Client = new RawTCPclient(hostname,port);
  puts("begin");
  sleep(1);  // bogus sleep (make the server wait)
  printf("Wrote %d bytes\n",Client->write(buffer,nbytes-28));
  sleep(1);  // bogus sleep (make the server wait)
  printf("2nd time: Wrote %d bytes\n",Client->write(buffer,nbytes));
  delete Client;
  return 1;
}
