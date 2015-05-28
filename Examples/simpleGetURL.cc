#include "RawTCP.hh"
#include <stdio.h> 

typedef char *charstring;
typedef char *charP;
#define ERROR -1
#define BUFSZ 4096

int main(int argc,char *argv[]){
  if(argc<3) {
    fprintf(stderr,"Usage: %s <webservername> </fileaddr> [<optionalport>]\n",argv[0]);
    fprintf(stderr,"\tWill print to stdout, so redirect to file with > if you need to\n");
    return 0;
  }
  // keep things unabiguous by giving these args names
  char *hostname=argv[1],*filename=argv[2];
  int port=(argc==4)?atoi(argv[3]):80; // default port 80.  otherwise parse 3rd arg
  FILE *outfile=stdout; // output file (for now, it is stdout)
  char buffer[BUFSZ];
  int nread;
  // construct the query in a single buffer
  char *query = new char[strlen(filename)+16];
  sprintf(query,"GET %s\n",filename);
  RawTCPclient urlConnection(hostname,port);
  if(!urlConnection.isValid()) { 
    fprintf(stderr,"Error: Can't open URL http://%s:%u%s because: %s\n",
	    hostname,port,filename,urlConnection.errorString());
    delete query;
    exit(0);
  }
  // send query to server
  urlConnection.writeString(query);
  urlConnection.writeString("\n"); // generates warning that I will not fix
  // write out the server response.
  while((nread = urlConnection.read(buffer,BUFSZ))>0)
    fwrite(buffer,1,nread,outfile);
  // clean up
  delete query;
  return 1; // success
}

