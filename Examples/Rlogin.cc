#include "RloginClient.hh"
#include <RawPort.hh>
#include <PortMux.hh>

int main(int argc,char *argv[]){
  RloginClient *rlogin;
	const char *host="localhost";
	const char *login="jshalf";
	const char *passwd="fakepassword";
  if(argc<3) {
    puts("demo login to bach");
    rlogin = new RloginClient(host,login,passwd);
  }
  for(int i=0;i<argc;i++) puts(argv[i]);
  if(argc==4){
    puts("4");
    rlogin = new RloginClient(argv[1],argv[2],argv[3]);
  }
  else{
    puts("3");
    rlogin = new RloginClient(argv[1],argv[2]);
  }
  int ic;
  while((ic=rlogin->getChar())>=0){
    char c=ic;
    cerr << c;
  }
  cerr << "\n" << endl;
}
