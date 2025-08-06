# Figure out what architecture we are dealing with here
# Strip out T3E unicos wierdness
UNAME := $(shell uname | perl -pe 's/(sn\d\d\d\d|jsimpson)/UNICOS\/mk/')

CC=cc
CCP=CC
MPCXX = CC

#default flags
CC = cc
CCP = CC
CFLAGS = -I.. -O -g
LDFLAGS = -L.. -lTCP++ -lC
LIBLIST = ../libTCP++.a

ifeq ($(UNAME), OSF1)
CXX = cxx
CCP = cxx
CFLAGS += -DOSF
LDFLAGS = -L.. -lTCP++ -lmpi -lelan -lm
endif

ifeq ($(UNAME), machten)
CC = gcc
CCP = g++
MPICXX = mpicxx
CFLAGS += -DMACHTEN
endif

ifeq ($(UNAME), Darwin)
CC = cc
CCP = c++
CFLAGS += -DDARWIN
MPICXX = mpicxx
# LDFLAGS = -L.. -lTCP++
MPIFLAGS = -I/usr/local/include -DUSE_MPI
MPILIBS = -L/usr/local/lib -lmpich
endif

ifeq ($(UNAME), IRIX64)
CCP = CC
# CFLAGS = -I.. -g -n32 -mips4 -O0
CFLAGS = -I.. -O3 -64 -mips4 -LNO -IPA -LANG:ansi-for-init-scope=ON -DSGI
LDFLAGS = -L.. -lTCP++ -lm -lmpi
AR = /bin/ar
endif

ifeq ($(UNAME), IRIX)
CCP = CC
CFLAGS = -I.. -O -DSGI
LDFLAGS = -L.. -lTCP++ -lm
AR = /bin/ar
endif

ifeq ($(UNAME), UNICOS/mk)
CCP = CC
CFLAGS = -g -I.. -hinstantiate=used -DT3E
LDFLAGS = -L.. -lTCP++ -lm
AR = ar
endif

ifeq ($(UNAME), Linux)
CC = gcc 
CCP = g++ 
MPCXX = g++
CFLAGS = -g -I.. -DLINUX
LDFLAGS = -L.. -lTCP++ -lm
MPIFLAGS = -I../Examples -I/usr/local/include -DMPI
MPILIBS = -L/usr/local/lib -lmpich -lpmpich
AR = ar
endif

ifeq ($(UNAME), AIX)
CC = xlcc
CCP = xlC
MPCXX = mpCC
CFLAGS = -g -I.. -DAIX
LDFLAGS = -L.. -lTCP++ -lm
MPIFLAGS = -DUSE_MPI
MPILIBS = -lmpi
AR = ar
endif

all: tests

distclean: clean
	touch  tmp
	rm -f  tmp VPtester VPdatasink VPreader \
		UDPpace UDPpaceServer UDPblaster PerfMonV3
	touch ii_files
	rm -rf ii_files
	touch core
	rm core
	rm -f *~

clean:
	touch t.o
	rm -f *.o

tests: VPtester VPdatasink VPreader pace UDPblaster CactusStubV3 VisapultStubV3 PerfMonV3 VisapultStubV3-check

examples: tests

VPreader: VPreader.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o VPreader VPreader.cc ${LDFLAGS}

VPtester: VPtester.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o VPtester VPtester.cc ${LDFLAGS}

VPdatasink: VPdatasink.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o VPdatasink VPdatasink.cc ${LDFLAGS} -lpthread

CactusStubV3.o: CactusStubV3.cc ProtocolV3.hh $(LIBLIST)
	$(CCP) $(CFLAGS) -I/usr/local/include -c CactusStubV3.cc

CactusStubV3: CactusStubV3.o ProtocolV3.o $(LIBLIST)
	$(CCP) $(CFLAGS) -I/usr/local/include -o CactusStubV3 \
		CactusStubV3.o ProtocolV3.o ${LDFLAGS}

VisapultStubV3.o: VisapultStubV3.cc ProtocolV3.hh $(LIBLIST)
	$(CCP) $(CFLAGS) -I/usr/local/include -c VisapultStubV3.cc

VisapultStubV3: VisapultStubV3.o ProtocolV3.o $(LIBLIST)
	$(CCP) $(CFLAGS) -I/usr/local/include -o VisapultStubV3 \
		VisapultStubV3.o ProtocolV3.o ${LDFLAGS}

VisapultStubV3-check.o: VisapultStubV3-check.cc ProtocolV3.hh $(LIBLIST)
	$(CCP) $(CFLAGS) -I/usr/local/include -c VisapultStubV3-check.cc

VisapultStubV3-check: VisapultStubV3-check.o ProtocolV3.o $(LIBLIST)
	$(CCP) $(CFLAGS) -I/usr/local/include -o VisapultStubV3-check \
		VisapultStubV3-check.o ProtocolV3.o ${LDFLAGS}

ProtocolV3mpi.o: ProtocolV3.cc ProtocolV3.hh $(LIBLIST)
	$(CCP) -c $(CFLAGS) ${MPIFLAGS} -DUSE_MPI -c ProtocolV3.cc -o ProtocolV3mpi.o

MPIutils.o: MPIutils.cc MPIutils.hh $(LIBLIST)
	$(CCP) $(CFLAGS) ${MPIFLAGS} -c MPIutils.cc

PerfMonV3.o: PerfMonV3.cc ProtocolV3.hh MPIutils.hh $(LIBLIST)
	$(CCP) $(CFLAGS) ${MPIFLAGS} -c PerfMonV3.cc

PerfMonV3: PerfMonV3.o ProtocolV3mpi.o MPIutils.o $(LIBLIST)
	$(CCP) $(CFLAGS) ${MPIFLAGS} -o PerfMonV3 \
		PerfMonV3.o ProtocolV3mpi.o MPIutils.o ${MPILIBS} ${LDFLAGS}

UDPblaster: UDPblaster.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPblaster.cc -o UDPblaster $(LDFLAGS)

ParallelUDPblaster: ParallelUDPblaster.cc $(LIBLIST)
	$(MPCXX) $(CFLAGS) ${MPIFLAGS} \
	 ParallelUDPblaster.cc MPIutils.cc -o ParallelUDPblaster \
	 $(LDFLAGS) ${MPILIBS}

pace: UDPpaceServer UDPpace

UDPpaceServer: UDPpaceServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPpaceServer.cc -o UDPpaceServer $(LDFLAGS)

UDPpace: UDPpace.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPpace.cc -o UDPpace $(LDFLAGS) -lm


ProtocolV3.o: ProtocolV3.cc ProtocolV3.hh $(LIBLIST)
	$(CCP) -c $(CFLAGS) ProtocolV3.cc 

