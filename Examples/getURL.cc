#include "RawTCP.hh"
#include <stdio.h> 

typedef char *charstring;
typedef char *charP;
#define ERROR -1
#define BUFSZ 4096

// destructive breakup of url
int ParseURL(char *url,charP &hostname,int &port,charP &file);
int main(int argc,char *argv[]){
  if(argc<2) {
    fprintf(stderr,"Usage: %s [-l] <url>\n",argv[0]);
    fprintf(stderr,"\t-l : is an optional parameter which will cause the webserver to print its full response header in addition to the contents of the url.  Otherwise, just the contents of the requested URL will be printed");
    fprintf(stderr,"\t<urL> : This must be a well-formed URL address such as\n\t\thttp://www.somehost.com/something.html\n\tor\n\t\thttp://www.somehost.com:8080/image.gif\n");
    return 0;
  }
  char *url,*urlarg;
  int long_query;
  char *hostname,*filename;
  int port;
  char buffer[BUFSZ];
  int nread;
  FILE *outfile=stdout;
  if(argc>2){
    // scan for -l
    urlarg=argv[2];
    long_query=1;
  }
  else {
    urlarg = argv[1];
    long_query=0;
  }
  url = new char[strlen(urlarg)+1];
  strcpy(url,urlarg);
  // parse the URL connection commandline argument
  if(ParseURL(url,hostname,port,filename)==ERROR){
    fprintf(stderr,"Malformed URL [%s]\n",argv[1]);
    delete url;
    return 0;
  }
  // construct a proper query
  char *query = 0;
  if(long_query){ // use fancy web-browser-like request
    query = new char[strlen(filename)+strlen(hostname)+256];
    sprintf(query,"GET /%s HTTP/1.0\nUser-Agent: TCP++ (Text)\nHost: %s:%u\nAccept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, */*\n\n",filename,hostname,port);
  }
  else { // use simple request (and get a simpler response)
    query = new char[strlen(filename)+strlen(hostname)+256];
    sprintf(query,"GET /%s\n\n",filename);
  }
  RawTCPclient *urlConnection = new RawTCPclient(hostname,port);
  // write out the query
  urlConnection->write(query,strlen(query));
  // now read the response
  while((nread = urlConnection->read(buffer,BUFSZ))>0)
    fwrite(buffer,1,nread,outfile);
  if(nread>0) // write the rest
    fwrite(buffer,1,nread,outfile);
  // done... now clean up 
  delete urlConnection;
  delete url;
  delete query;
  return 1;
}

// destructive breakup of url
int ParseURL(char *url,charP &hostname,int &port,charP &file){
  char *s,*sp;
  if (!url)
    return ERROR;
  if (strncmp("http://", url, 7))
    return 0;
  port = 80; // default http port
  hostname = url+7;
  //printf("hostname=[%s]\n",hostname);
  url[6]='\0';
  s = strchr(hostname,'/');
  if(!s) 
    s = hostname + strlen(hostname);
  *s = '\0';
  if((sp=strrchr(s-1,':'))){
    port = atoi(sp+1);
    *sp='\0';
    // printf("port=%u [%s]\n",port,sp+1);
  }
  //printf("hostname=[%s]\n",hostname);
  file = s+1;
  //printf("file=[%s]\n",file);
  return 1;
}
