TCP++ : Library for simplified C++ access to key BSD sockets capabilities
John Shalf (1996-present) and Werner Benger (2001-present)
Many additional contributions from CCT at LSU and the Cactus (CCTK) team.
sed for Cactus Computational toolkit (http://www.cactuscode.org/) and SciNET Bandwidth challenge.

Based on TCPutils library, developed by John Shalf at VPI&SU in 1991.  TCPutils used the C language.  TCP++ is a ground-up rewrite for the C++ language.  The original TCPutils is still in the "CImpl" subdirectory, but development ceased in 1995.

Copyright: 
	Max Planck Institute for Gravitationphysics / Albert Einstein Institut (Potsdam, Germany)
	National Center for Supercomputing Applications (Champaign, Illinois)
	Konrad Zuse Institut (Berlin, Germany)
	Lawrence Berkeley National Laboratory (Berkeley, California)
	Center for Computational Technology at Louisianna State University (Baton Rouge, Louisiana)

To build, please use GNU Make.
It should recognize the architecture and use appropriate flags, but if not, modify variables within the makefile.
Build the main directory first to build the library, and then move to "examples" subdirectory to see use cases.

