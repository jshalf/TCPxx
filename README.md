# TCPxx
TCP++ : Library for simplified C++ access to key BSD sockets capabilities
John Shalf (1996-present) and Werner Benger (2001-present)
Many additional contributions from CCT at LSU and the Cactus (CCTK) team.
sed for Cactus Computational toolkit (http://www.cactuscode.org/) and SciNET Bandwidth challenge.

Based on C-language-based TCPutils library, developed by John Shalf at VPI&SU in 1991.   TCP++ is a ground-up rewrite for the C++ language.  The original TCPutils is still in the "CImpl" subdirectory, but development ceased in 1995.

SC2002 subdirectory contains the protocol specifications and Cactus + Visapult stubs for the penultimate SciNET bandwidth challenge implemetnation.  Cactus (a black hold simulation code) fed volumetric data in real-time to Visapult (developed by Wes Bethal of LBL) to perform real-time 3D parallel volume rendering of the ongoing simulation.  TCP++ was used to win the Bandwidth challenge for the first 3 years of existence (winning 2000, 2001, and 2002).  LBL retired from the bandwidth the bandwidth challenge after the 2002 win.

Copyright: 
    Max Planck Institute for Gravitationphysics / Albert Einstein Institut (Potsdam, Germany)
    National Center for Supercomputing Applications (Champaign, Illinois)
    Konrad Zuse Institut (Berlin, Germany)
    Lawrence Berkeley National Laboratory (Berkeley, California)
    Center for Computational Technology at Louisianna State University (Baton Rouge, Louisiana)
    Virginia Polytechnic Institute & State University (Blacksburg, Virginia)

To build, please use GNU Make.
It should recognize the architecture and use appropriate flags, but if not, modify variables within the makefile.
Build the main directory first to build the library, and then move to "examples" subdirectory to see use cases.

