******************************************************************************
******************************************************************************
**                             Super Scalar Mips Sim                        **
**                              Tanner Burge-Beckley                        **
**                                  James McCoy                             **
**                                                                          **
**                                   May 3, 2017                            **
**                                                                          **
******************************************************************************
******************************************************************************


Our Super Scalar Simulator pulls in instructions from a file to simulate a
memory unit.  It pulls those values into a cache unit.  From there, 
the instructions are fetched and decoded. Depending on the instruction, it is
either handled by this Instruction Fetch unit or pushed into a Pre-Issue buffer.  
The Issue unit pulls from this Pre-Issue buffer and determines if the operation 
is a Memory operation or ALU operation, and pushes it into the appropriate 
Pre-Mem or Pre-Alu buffer. The Mem unit and ALU decode the instruction, carry 
out the operation, and pushes the result destination and data into a post 
buffer, which the Write Back Unit pulls from, and writes to the appropriate 
register.


******************************************************************************
******************************************************************************
**                               Files Included                             **
******************************************************************************
******************************************************************************
MipsSuScEmu.cpp
makefile
README.txt(this thing you're looking at now)