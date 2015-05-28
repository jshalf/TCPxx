#include "RawTCP.hh"
#include <stdio.h>

int main(int argc,char *argv[]) {
  char buffer[1024*128];
  int port = 7052;
  long nbytes = 1024*128;

  RawTCPport *ServerPort = new RawTCPserver(port);
  puts("Wait for connection");
  RawPort *Server = ServerPort->accept();
  long count,recvd;
  printf("Await receive of %ld bytes\n",nbytes);
  for(count=0,recvd=0;recvd<nbytes;count++){
    long nrec=0;
    nrec=Server->read(buffer,nbytes-recvd);

    fprintf(stderr,"nrec=%ld\n",nrec);
    if(nrec<0){
      perror("Error:  Short Read: ");
      break;
    }
    else if(nrec==0){
      fprintf(stderr,"Error:  EOF.  The other end dropped the connection");
      break;
    }
    recvd+=nrec;
    fprintf(stderr,"Received %ld of %ld.  %ld remain\n",recvd,nbytes,nbytes-recvd);
  }
  puts("Go to Second Recv Loop");
  for(count=0,recvd=0;recvd<nbytes;count++){
    long nrec=0;
    nrec=Server->read(buffer,nbytes-recvd);
    fprintf(stderr,"nrec=%ld\n",nrec);
    if(nrec<0){
      perror("Error:  Short Read: ");
      break;
    }
    else if(nrec==0){
      fprintf(stderr,"Error:  EOF.  The other end dropped the connection");
      break;
    }
    recvd+=nrec;
    fprintf(stderr,"Received %ld of %ld.  %ld remain\n",recvd,nbytes,nbytes-recvd);
  }
  delete Server;
  delete ServerPort;
  return 1;
}

