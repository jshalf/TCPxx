/*
Derived from Perl example at 
  http://www.cs.orst.edu/~pancake1/anim/test/spush_perl.txt
*/
#include <RawTCP.hh>
#include <stdio.h>
#include <string.h>

void main(int argc,char *argv[]) {
  int port,nimages; 
  char *mimetype;
  //char buffer[4096]; 
  int bufsize=1024;
  char *buffer = new char[bufsize];
  if(argc<4){
	printf("Server Push Usage: %s <portnum> <mime-type> <file1> <file2> ... <filen>\n",argv[0]);
	printf("\tportnum must be supplied as the second argument\n");
	printf("\tMime-type: This should be the mime-type for the files you are sending\n");
	printf("\tFor example, for a GIF image you should use\n");
	printf("\t\timage/gif\n");
	printf("\tAnd JPEG is\n");
	printf("\t\timage/jpeg\n");
	printf("\tThe remaining args should be a list of images to push\n");
	exit(0);
  }
  port = atoi(argv[1]);
  mimetype=argv[2];
  nimages=argc-3;
  printf("Opening server port %u to push %u images of type [%s]\n",
	port,nimages,mimetype);
  RawTCPport *ServerPort = new RawTCPserver(port);
  puts("Wait for connection");
  RawPort *Server = ServerPort->accept();
  Server->writeString("HTTP/1.0 200\n");  // write Webserver response header
  Server->writeString("Content-type: multipart/x-mixed-replace;boundary=ThisRandomString\n"); 
  Server->writeString("\n--ThisRandomString\n"); // write the boundary delimiter
  for(int i=0;i<nimages;i++){
    char *filename=argv[i+3];
    FILE *file = fopen(filename,"r");
    int nbytes;
    if(!file){
	printf("Failed to open file %s\n",filename);
	continue;
    }
    fseek(file,0,SEEK_END); // get file length
    nbytes = ftell(file); // get file length
    printf("Push file %s len=%u bytes\n",filename,nbytes);
    sprintf(buffer,"Content-type: %s\n",mimetype);
    // wait until write completes
    Server->write(buffer,strlen(buffer)); // first push the header (per-image)
    sprintf(buffer,"Content-length: %u\n",nbytes);
    // wait until write completes (watch for ENOBUF)
    Server->write(buffer,strlen(buffer));
    Server->writeString("\n");
    fseek(file,0,SEEK_SET); // back to beginning of file
    // then the contents of the file
    if(bufsize<nbytes) {
      delete buffer;
      buffer = new char[nbytes];
      bufsize=nbytes;
    }
    //while((nbytes = fread(buffer,1,sizeof(buffer),file))>0) {
    //  printf("Write %u bytes\n",nbytes);
    //	Server->write(buffer,nbytes);
    //}
    fread(buffer,1,nbytes,file);
    //Server->write(buffer,nbytes);
    Server->writeString("\n--ThisRandomString\n");
    sleep(5);
    fclose(file);
  }
  sleep(5);
  delete Server;
  delete ServerPort;
}

