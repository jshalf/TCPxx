// TestDC.cc
// Use the -s option to make it the data sender
// otherwise it will sit passively waiting to receive the data.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DataConnection.hh"

void main(int argc,char *argv[]){
	int length;
	DataType::Type type;
	char hostname[32];
	char name[64];
	char mode='r';
	if(argc>1) // first arg is sender vs. receiver
		mode=*(argv[1]);
	if(argc>2) // a second arg means specify a hostname
		strcpy(hostname,argv[2]);
	else
		strcpy(hostname,"localhost");
	if(mode=='s'){
		double databuffer[2*1024*1024];
		DataSender *s=new DataSender(hostname,7008);
		s->setBlockSize(16*1024);
		strcpy(name,"gxx");
		length=512*1024;
		for(int i=0;i<1024;i++) databuffer[i]=(double)i;
		type=DataType::Float64;
		printf("Sending\n");
		printf("Data sent %u\n",
			s->sendData(name,length,type,databuffer));
	}
	else { // I'm a reciever
		DataReceiver *r=new DataReceiver(hostname,7008);
		double *data;
		int i=0;
		while(!(data=(double*)(r->receiveData(name,length,type)))){sginap(10);}
			// fprintf(stderr,"Still Trying %u\r",i++);
		printf("\nReceived %u elements of type %u with message [%s]\n",
			length,(int)type,name);
		for(i=0;i<3;i++) printf("data[%u]=%lf\n",i,data[i]);
		for(i=1020;i<1023;i++) printf("data[%u]=%lf\n",i,data[i]);
	}
	puts("Done");
}
