#include <stdio.h>
#include <RawTCP.hh>
#include <SockIOreader.hh>
#include <unistd.h>

const char*Type(IObase::DataType numbertype)
{
const char*stype = "unknown";
	 switch( numbertype )
	 {
	 case IObase::Float32: stype = "float"; break;
	 case IObase::Float64: stype = "double"; break; 
	 case IObase::Int8:    stype = "char"; break;
	 case IObase::Int16:   stype = "short"; break; 
	 case IObase::Int32:   stype = "integer"; break;
	 case IObase::Int64:   stype = "long"; break;
	 case IObase::uInt8:   stype = "unsigned char ( 8 bit)"; break; 
	 case IObase::uInt16:  stype = "unsigned short integer (16 bit)"; break;
	 case IObase::uInt32:  stype = "unsigned integer (32-bit)"; break; 
	 case IObase::uInt64:  stype = "unsigned long integer (64-bit)"; break; 		 

	 case IObase::String:  stype = "string"; break;
	 case IObase::Unicode: stype = "Unicode string"; break;
	 }
	 return stype;
}

int main(int argc, char*argv[])
{
char	remotefilename[999]="grr.ieee";
char	hostname[128]="localhost";
int	port = 7030;
	if (argc>1)
	{
	char	*A = strchr(argv[1], '@');
		if (A) 
		{
			*A=0; A++; 
			strcpy(remotefilename,argv[1]);
		char	*d = strchr(A, ':');
			if (d) { *d=0; d++; port=atoi(d); }
			strcpy(hostname, A);
		}
		else
			strcpy(remotefilename,argv[1]);

		if (port<1) port = 7030;
	}
	else
	{
		puts("Usage: FileReceive <filename>@<hostname>[:<portnumber>]");
		return 1;
	}

	printf("Trying to receive file `%s' from host [%s] at port %d.\n",remotefilename,hostname,port);

char	ThisHost[1024] = "localhost";

	{
	char	ThisHostname[1024] = "localhost";
		gethostname (ThisHostname, sizeof(ThisHostname) );

	struct hostent *he = gethostbyname(ThisHostname);

		if (!he)
		{
			puts("Could not determine this host's address.\n");
			return 1;
		}
		strcpy(ThisHost,he->h_name);
	}
	printf("This is %s.\n",ThisHost);



int	eport=port+9;
	  // Connect to the directory server
RawTCPclient*Client = new RawTCPclient(hostname,port);

	while (!Client->isAlive())
	{
		delete Client;
		if (++port>=eport)
		{
			puts("*FileReceive> No Fileserver found - exiting.");			
			return 1;
		}
		Client = new RawTCPclient(hostname,port);
	}


int	dataport = 7040;

char	cmd[1024] = "T:";

	strcat(cmd, remotefilename);
	  // Create a file reader and determine available port number
SockIOreader*reader = new SockIOreader(cmd, dataport);

	printf("Transferring to local file %s.\n",cmd);

	sprintf(cmd,"connect %s %s:%d", remotefilename, ThisHost, dataport);

	printf("*FileReceive> Wrote %d bytes (%s)\n",Client->write(cmd, strlen(cmd)),cmd);

char	buf[1024];

//	for(;false;)
	for(;;)
	{
	int	got = Client->read(buf, sizeof(buf));
		if (got>0) buf[got] = '\0'; else buf[0]=0;

		printf("*FileReceive> Received `%s' (%d Bytes)\n",buf, got);

		if (strcmp(buf, "connecting")==0) break;

		if (strcmp(buf, "not connecting")==0) 
		{
			puts("*FileReceive> Cannot receive file (wrong connection parameters?).");
			return 0;
		}

		if (strcmp(buf, "error")==0) 
		{
			puts("*FileReceive> Transmission error.");
			return 0;
		}
		sleep(1);
	}

	delete Client;

int	N;

	do
	{
		N = reader->nDatasets();
		printf("Socket connection: %d datasets available.\n", N);

		sleep(2);
	}
	while( N<1 );

IObase::DataType numbertype;
int rank;
int dims[5];

	reader->readInfo(numbertype,rank,dims,5);

	printf("Number type: %s\n",Type(numbertype) );
	printf("Rank       : %d\n",rank);
	printf("Dimensions : "); for(int i=0;i<rank;i++) printf("%d%s",dims[i],i==rank-1?"":"x"); puts("");

int	nattr = reader->nAttributes();

	printf("Attributes : %d\n",nattr);

	for(int a=0;a<nattr;a++)
	{
	char	buf[1024];
	Long	len;
		reader->readAttributeInfo(a, buf, numbertype, len, sizeof(buf));

		printf(" %s (%s, %d elements): ", buf, Type(numbertype), len);

        int     mem = IObase::sizeOf(numbertype)*len;
	void	*data = new char[mem];

		reader->readAttribute(a, data); 
		for(int i=0; i<len; i++)
		{
			switch( numbertype )
			{
			case IObase::Float32: printf("%g ",(static_cast<float*>(data))[i]); break;
			case IObase::Float64: printf("%g ",(static_cast<double*>(data))[i]); break;
			case IObase::Int8:    printf("%c ",(static_cast<char*>(data))[i]); break;
			case IObase::Int16:   printf("%d ",(static_cast<short*>(data))[i]); break;
			case IObase::Int32:   printf("%ld ",(static_cast<long*>(data))[i]); break;
		//	case IObase::Int64:   printf("%d ",int((static_cast<long long*>(data))[i])); break;
			case IObase::Int64:   printf("%d ",int((static_cast<long*>(data))[i])); break;
			case IObase::uInt8:   printf("%c ",(static_cast<char*>(data))[i]); break;
			case IObase::uInt16:  printf("%d ",(static_cast<short*>(data))[i]); break;
			case IObase::uInt32:  printf("%ld ",(static_cast<long*>(data))[i]); break;
		//	case IObase::uInt64:  printf("%d ",int((static_cast<long long*>(data))[i])); break;
			case IObase::uInt64:  printf("%d ",int((static_cast<long*>(data))[i])); break;
				
			case IObase::String:  printf("%c",(static_cast<char*>(data))[i]); break;
			case IObase::Unicode: printf("%X ",(static_cast<short*>(data))[i]); break;
			}
		}
		printf("\n");
	}

int	 nAnn = reader->nAnnotations(); 

	 printf("Annotations : %d\n",nAnn);

	 for(int b=0;b<nAnn;b++)
	 {
	 char	buf[1024];
	 int	len;	 
		reader->readAnnotationInfo(b,len);
		printf(" %d (%d chars)", b, len);

		reader->readAnnotation(b,buf, sizeof(buf));
		printf(" %s\n",buf);

	 }

	 delete reader;

	return 0;
}
