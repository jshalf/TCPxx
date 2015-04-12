#include <stdio.h>
#include "SockIOwriter.hh"
#include "IEEEIO.hh"
#include "CommandProcessor.hh"
#include "ObjectMux.hh"

struct	ServedFile
{
	IEEEIO		file;
	SockIOwriter	sockio;
	IObase::DataType numbertype;
	int		rank;	
	int		dims[10];
	void		*data;
	int		sent_timeslices;

	ServedFile(const char*filename, const char*hostname, int port);
	~ServedFile();

	bool send();
};

ServedFile::ServedFile(const char*filename, const char*hostname, int port)
: file(filename, IObase::Read), sockio(hostname, port)
{
	data = 0;
	sent_timeslices = 0;

        if (!file.isValid())
        {
		printf("ServedFile: File %s is invalid!\n", filename);
		return;
	}

	file.readInfo(numbertype,rank,dims,10);
	data = new char[IObase::nBytes(numbertype,rank,dims)];
}

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

bool	ServedFile::send()
{
	sent_timeslices++;
	file.read(data);
	sockio.write(numbertype,rank,dims,data);

int     nattr = file.nAttributes();

	printf("Attributes : %d\n",nattr);

        for(int a=0;a<nattr;a++)
        {
        char    buf[1024];
	Long	len;
	IObase::DataType AttrNumbertype;

                file.readAttributeInfo(a, buf, AttrNumbertype, len, sizeof(buf));

                printf(" %s (%s, %d elements): \n", buf, Type(AttrNumbertype), len);

        int     mem = IObase::sizeOf(AttrNumbertype)*len;
        void    *data = new char[mem];

		file.readAttribute(a,data);

		sockio.writeAttribute(buf, AttrNumbertype, len, data);
		delete data;
	}

	return true;
}

ServedFile::~ServedFile()
{}


#define CLOSE_PORT (reinterpret_cast<PortAction*>(-1))

class	PortAction
{
protected:
	RawTCPport*TCPport;

public:	PortAction( RawTCPport*TCP )
	: TCPport(TCP)
	{}

	virtual ~PortAction()
	{}

	int Port() const
	{
		if (!TCPport) return -1;
		return TCPport->getPortNum();
	}

	RawPort*RawPort()
	{
		return TCPport;
	}

	  /// Remote put string
	void	rputs(const char*s)
	{
		if (TCPport)
			TCPport->write((char*)s, strlen(s) );
	}

	virtual PortAction*Action() = 0;
};

class	FileServer : public ServedFile, public PortAction
{
public:	FileServer( const char*filename, const char*hostname, int port)
        : ServedFile(filename, hostname, port),
	  PortAction( sockio.TCPclient()  )
	{
		printf("FileServer established at port %d.\n", TCPport->getPortNum() );
		send();
	}

	PortAction*Action()
	{
	char	inbuf[1024];
	int	R = TCPport->read(inbuf, sizeof(inbuf) );

		if (R<0)
		{
			puts("**FileServer::PortAction> Error while reading.");
			return CLOSE_PORT;
		}

		rputs("FileServer: Waiting for command >");

		inbuf[R--] = '\0';
		while(R>0 && (inbuf[R]=='\n' || inbuf[R]=='\r'))
			inbuf[R--] = '\0';

		printf("**FileServer::PortAction> Got %d chars (%s).\n",R+1,inbuf);

	char	*args = strchr(inbuf,' ');
		if (args) { *args='\0'; args++; }

	int	seeknum = atoi(inbuf);	
		if (seeknum>0)
		{
			while(sent_timeslices < seeknum && sent_timeslices < file.nDatasets() )
			{
				printf("FileServer> Seek: Sending %d of %d\n", sent_timeslices, file.nDatasets() );
				send();
			}
		}

		return 0;
	}
};


class	DirectoryServer : public PortAction
{
public:	DirectoryServer( RawTCPport*rtp)
	: PortAction( rtp )
	{
		printf("DirectoryServer established at port %d.\n", TCPport->getPortNum() );

		rputs("DirectoryServer: Hello > ");
	}

	virtual PortAction*Action();

	~DirectoryServer()
	{
		delete TCPport;
	}
};


PortAction*DirectoryServer::Action()
{
char	inbuf[1024];
int	R = TCPport->read(inbuf, sizeof(inbuf) );

	 if (R<0)
	 {
		 puts("**DirectoryServer::PortAction> Nothing received.");
		 return CLOSE_PORT;
	 }

	 inbuf[R--] = '\0';
	 while(R>0 && (inbuf[R]=='\n' || inbuf[R]=='\r'))
		 inbuf[R--] = '\0';

	 printf("**DirectoryServer::PortAction> Got %d chars (%s).\n",R+1,inbuf);

char	*args = strchr(inbuf,' ');
	 if (args) { *args='\0'; args++; }

	 if (strcmp(inbuf,"quit")==0)
	 {
		 puts("**DirectoryServer> QUIT.");
		 return CLOSE_PORT;
	 }

	 if (strcmp(inbuf,"connect")==0)
	 {
		if (!args || !args[0]) return 0;
	 char	filename[1024];
	 char	remotehostname[1024];
	 int	port;
		strcpy(filename, args);
	char	*s = strchr(filename,' ');
		if (s)
		{
			*s=0; s++;
			strcpy(remotehostname, s);
		char	*t = strchr(remotehostname,':');
			if (t)
			{
				*t=0; t++;
				port = atoi(t);
				if (port>0)
				{
					printf("DirectoryServer> connecting %s --> %s:%d\n", filename, remotehostname, port);
					rputs("connecting");
					return new FileServer( filename, remotehostname, port);
				}
			}
		}
		puts("DirectoryServer> Connection error.");
		rputs("error");
	 }
	 else
		 rputs("DirectoryServer: Waiting for command >");
	 
	 return 0;
}


class	ConnectionServer : public PortAction
{
	int	PortNo;

public:	ConnectionServer( int port )
	: PortAction( new RawTCPserver( port ) )
	{
		while( !TCPport->isAlive() )
		{
			printf("ConnectionServer not available on port %d.\n", port );
			delete TCPport;
			port++;
			TCPport =  new RawTCPserver( port );
		}
		printf("ConnectionServer established at port %d.\n", port );

		PortNo = port + 2;
	}

	PortAction*Action()
	{
		puts("**DirectoryServer> New client.");

	RawTCPserver*server = static_cast<RawTCPserver*>(TCPport);

	RawTCPport*clientport = server->accept();

		return new DirectoryServer(clientport);
	}

	~ConnectionServer()
	{
		delete TCPport;
	}
};


int	main(int argc, char*argv[])
{
int	ConnectionPort = 7030;
	if (argc>1) ConnectionPort=atoi(argv[1]);
	if (!ConnectionPort) ConnectionPort = 7030;

ConnectionServer CnS(ConnectionPort);

ObjectMux OM;

	OM.addInport( CnS.RawPort(), &CnS);

	OM.enableTimeout(false);

	for(;;)
	{
	PortAction*action = static_cast<PortAction*>(OM.selectObject());
		if (!action)
		{
			puts("FileServer> timeout.");
			return 0;
		}
		puts("FileServer> Somewhat happened.");

	PortAction*newAction = action->Action();

		if (newAction == CLOSE_PORT)
		{
			puts("FileServer> Closing connection.");
			OM.del( action );
			delete action;
			continue;
		}
		sleep(1);

		if (newAction)
			OM.addInport( newAction->RawPort(), newAction );
	}
}
