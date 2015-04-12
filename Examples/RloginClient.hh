#ifndef __RLOGINCLIENT_HH_
#define __RLOGINCLIENT_HH_
#include <RawTCP.hh>
#include <PortMux.hh>
/*
Implements basic rlogin protocol
	Does not trap ctrl-c
	Does not trap ctrl-z
	does not trap '~' (if you send it, you lose the connection)
*/

class RloginClient : private RawTCPclient {
	void login(char *name,char *localname=0,char *termtype="VT100/9600");
	void sendPasswdInClear(char *passwd);
#ifdef KRB5
	int sendPasswdKerberos5(char *passwd);
#endif
	char CEscape,CPause,CResume,CCmd[3],CWindowSize[3];
	char SFlush,SStartFlow,SStopFlow,SWindowSize;
	char paused;
	PortMux block,nonblock;
	void init(char *name,char *passwd=0);
public:
	RloginClient(char *hostname,char *name,char* passwd=0);
	~RloginClient();
	int handleCommand(char &c);
	int getChar();
	int getCharNonBlock();
	int put(char c);
	int put(char *c,int size);
	int pause();
	int resume();
};

#endif /* __RLOGINCLIENT_HH_ */

