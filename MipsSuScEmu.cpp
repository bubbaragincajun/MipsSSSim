#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>
#include <bitset>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;

struct CacheLine {
    bool valid;
    bool dirty;
    int tag;
    int[2] data;
};

struct CacheSet {
    bool LRU;
    CacheLine[2] line;
};

struct PostBuff {
    bool valid;
    int dest, data;
};

struct PreBuff {
    bool valid;
    int instruction;
};


//Global register/memory declarations
//Not necessarily sold on the preALU and preMEM implementations...
//Might make it harder to check if a register is being written back to.
//Maybe some functions to get the register operands might be helpful
//Or could just send a destination register, and two values?
// [alu op] [some int] [some other int] [destination reg] (between 1 and 32)
//Something like that?
int memarray[100];
int r[32];
CacheSet[4] cache;
PostBuff postAlu;
PostBuff postMem;
PreBuff[2] preAlu;
PreBuff[2] preMem;
PreBuff[4] preIssue;
int sp, pc, memstart, cycle;

string mipsReturn(const int& i);
void disassemble(const string& filename, const string& outfile);
string interpret(const int& i);
void status(const int& i, ofstream& out);
void showhelpinfo(char *s);
void writeBack();

int main(int argc, char* argv[]) {
	char tmp;
	string infile("");
	string outfile("");
	/*if the program is ran witout options ,it will show the usage and exit*/
	if(argc == 1)
	{
		showhelpinfo(argv[0]);
		return(1);
	}
	/*use function getopt to get the arguments with option."hi:o:" indicate 
	that option h is the option without arguments while i and o are the
	options with arguments*/
	while((tmp=getopt(argc,argv,"hi:o:"))!=-1)
	{
		switch(tmp)
		{
			/*option h shows the help infomation*/
			case 'h':
				showhelpinfo(argv[0]);
				break;
			/*option i set input file*/
			case 'i':
				infile = optarg;
				break;
			/*option o set output file*/ 
			case 'o':
				outfile = optarg;
				break;
			/*invail input will get the help infomation*/
			default:
			showhelpinfo(argv[0]);
			break;
		}
	}
	if (!((infile != "") && (outfile != ""))){
		cout << "Too few arguments. Please execute: '" << argv[0] << " -h' to see help info\n";
	}
	else {
		//build the virtual memory and print out the disassembled code (using interpret)
		disassemble(infile,outfile);
		//eventually putting the guts of the sim here
		outfile = outfile + "_sim.txt";
    }

    return 0;
}

void writeBack() {
    //check if post alu buffer is ready to write back
    if (postAlu.valid) {
        r[postAlu.dest] = postAlu.data;
        postAlu.valid = false;
        postAlu.dest = 0;
        postAlu.data = 0;
    }
    //check if post mem buffer is ready to write back
    if (postMem.valid) {
        r[postMem.dest] = postMem.data;
        postMem.valid = false;
        postMem.dest = 0;
        postMem.data = 0;
    }
}

void ALU() {
    //checks if there is an instruction ready in the first buffer slot
    if (preAlu[0].valid) {
        ALUIssue(preAlu[0].instruction);
    }
    preAlu[0] = preAlu[1];
    preAlu[1].valid = false;
    preAlu[1].instruction = 0;
}

void ALUIssue(const int& instruction) {
    int op, rs, rt, rd, imm, shift, aluOp;
    op = instruction << 1;
    op >>= 27;
    rs = instruction << 6;
    rs >>= 27;
    rt = instruction << 11;
    rt >>= 27;
    rd = instruction << 16;
    rd >>= 27;
    imm = instruction << 16;
    imm >>= 16;
    shift = instruction << 21;
    shift >>= 27;
    aluOp = instruction << 26;
    aluOp >>= 26;

    if (op == 0) {
        op = aluOp;
    }

    /*switch (op) {
        //ADD
        case 6'b100000:
            break;
        //ADDI
        case 5'b01000:
            break;
        //SUB
        case 6'b100010:
            break;
        //SLL
        case 6'b000000:
            break;
        //SRL
        case 6'b000010:
            break;
        //MUL
        case 5'b11100:
            break;
        //AND
        case 6'b100101:
            break;
        //OR
        case 6'b100100:
            break;
        //MOVZ?
        case 6'b001010:
            break;

    }*/

}


//should be able to use as is
void disassemble(const string& filename, const string& outfilename) {
	char buffer[4];
	int i;
	char * iPtr;
	iPtr = (char*)(void*) &i;
	int count = 0;
	pc = 96;

	int FD = open(filename.c_str(), O_RDONLY);
	string dis = outfilename + "_dis.txt";
	ofstream fout(dis.c_str());
	int amt = 4;
	while( amt != 0 )
	{
		amt = read(FD, buffer, 4);
		if( amt == 4)
		{
			iPtr[0] = buffer[3];
			iPtr[1] = buffer[2];
			iPtr[2] = buffer[1];
			iPtr[3] = buffer[0];
			memarray[count] = i;
			//passes in the integer representation of the command
			bitset<32> bits(i);
			string bitstr = bits.to_string();
			if (memstart == 0){
				fout << bitstr.substr(0,1) << " " 
				<< bitstr.substr(1,5) << " "
				<< bitstr.substr(6,5) << " " 
				<< bitstr.substr(11,5) << " " 
				<< bitstr.substr(16,5) << " "
				<< bitstr.substr(21,5) << " "
				<< bitstr.substr(26,6) << " "
				<< pc << " \t" << mipsReturn(i) << endl;
			}
			else {
				fout << setw(38) << left << bitstr << setw(0) << " " << pc << " \t" << i << endl;
			}
			count++;
			pc += 4;
			if (i == -2147483635) {
				memstart = pc;
			}
		}
	}
	fout.close();
	sp = pc;
	pc = 96;
}

//should be able to use as is.
string interpret(const int& i) {
	bitset<32> op(i);
	string bitstring = op.to_string();
	return bitstring;
}

//edit this to match the output for the current project
void status(const int& i, ofstream& out) {
	//old
	/*
	out << "====================\n";
	out << "cycle:" << cycle << " " << pc <<"\t" << mipsReturn(i) << "\n\n";
	out << "registers:\n";
	out << "r00: ";
	for (int i = 0; i < 8; i++) {
		out << "\t" << r[i];
	}
	out << "\nr08: \t";
	for (int i = 8; i < 16; i++) {
		out << r[i] << "\t";
	}
	out << "\nr16: \t";
	for (int i = 16; i < 24; i++) {
		out << r[i] << "\t";
	}
	out << "\nr24: \t";
	for (int i = 24; i < 32; i++) {
		out << r[i] << "\t";
	}
	out << "\n\ndata:\n";
	int numData = (sp - memstart)/4;
	int lines = numData/8;	
	int offset = memstart;

	for (int i = 0; i < lines-1; i++) {
		out << offset << ": ";
		for (int j = (offset-96)/4; j < (offset-96)/4 + 8; j++) {
			out << "\t" << memarray[j] ;
		}
		out << "\n";
		offset += 32;
	}
	int numLeft = (numData<8)? numData: numData - (lines-1)*8;
	out << offset << ": ";
	for (int i = (offset-96)/4; i < (offset-96)/4 + numLeft; i++) {
		out << "\t" << memarray[i];
	}
	out << "\n\n";
	*/

	//new
	out << "--------------------\n";
	out << "Cycle[" << cycle << "]:\n\n";
	
	//[re issue buffer]
	out << "Pre-Issue Buffer:\n";
	out << "\tEntry 0:\t" << "[//instruction here]\n";
	out << "\tEntry 1:\t" << "[//instruction here]\n";
	out << "\tEntry 2:\t" << "[//instruction here]\n";
	out << "\tEntry 3:\t" << "[//instruction here]\n";
	out << "Pre_ALU Queue:\n";
	out << "\tEntry 0:\t" << "[//instruction here]\n";
	out << "\tEntry 1:\t" << "[//instruction here]\n";
	out << "Post_ALU Queue:\n";
	out << "\tEntry 0:\t" << "[//instruction here]\n";
	out << "Pre_MEM Queue:\n";
	out << "\tEntry 0:\t" << "[//instruction here]\n";
	out << "\tEntry 1:\t" << "[//instruction here]\n";
	out << "Post_MEM Queue:\n";
	out << "\tEntry 0:\t" << "[//instruction here]\n\n";

	out << "Registers\n";
	out << "R00: ";
	for (int i = 0; i < 8; i++) {
		out << "\t" << r[i];
	}
	out << "\nR08: \t";
	for (int i = 8; i < 16; i++) {
		out << r[i] << "\t";
	}
	out << "\nR16: \t";
	for (int i = 16; i < 24; i++) {
		out << r[i] << "\t";
	}
	out << "\nR24: \t";
	for (int i = 24; i < 32; i++) {
		out << r[i] << "\t";
	}
	out << "\n\n";

	cout << "Cache\n";
	out << "Set 0: LRU=[//insert LRU Value here]\n";
	out << "\tEntry 0: [(valid bit, dirty bit, int(tag))<word0,word1>]\n";
	out << "\tEntry 1: [(valid bit, dirty bit, int(tag))<word0,word1>]\n";
	out << "Set 1: LRU=[//insert LRU Value here]\n";
	out << "\tEntry 0: [(valid bit, dirty bit, int(tag))<word0,word1>]\n";
	out << "\tEntry 1: [(valid bit, dirty bit, int(tag))<word0,word1>]\n";
	out << "Set 2: LRU=[//insert LRU Value here]\n";
	out << "\tEntry 0: [(valid bit, dirty bit, int(tag))<word0,word1>]\n";
	out << "\tEntry 1: [(valid bit, dirty bit, int(tag))<word0,word1>]\n";
	out << "Set 3: LRU=[//insert LRU Value here]\n";
	out << "\tEntry 0: [(valid bit, dirty bit, int(tag))<word0,word1>]\n";
	out << "\tEntry 1: [(valid bit, dirty bit, int(tag))<word0,word1>]\n\n";

	out << "Data:\n";
	int numData = (sp - memstart)/4;
	int lines = numData/8;	
	int offset = memstart;

	for (int i = 0; i < lines-1; i++) {
		out << offset << ": ";
		for (int j = (offset-96)/4; j < (offset-96)/4 + 8; j++) {
			out << "\t" << memarray[j] ;
		}
		out << "\n";
		offset += 32;
	}
	int numLeft = (numData<8)? numData: numData - (lines-1)*8;
	out << offset << ": ";
	for (int i = (offset-96)/4; i < (offset-96)/4 + numLeft; i++) {
		out << "\t" << memarray[i];
	}
	out << "\n\n";
}

string mipsReturn(const int& command) {

	bitset<32> instruction(command);
	bitset<1> valid;
	bitset<5> op;
	bitset<5> rs;
	bitset<5> rt;
	bitset<5> rd;
	bitset<5> sa;	
	bitset<16> imm;
	bitset<26> tar;
    bitset<6> func;

	//collecting reg contents
	//valid
	valid[0] = instruction[31];



	//finds opCode
	for (int i = 4; i >= 0; i--) {
		op[i] = instruction[i + 26];
	}

	//rs
	for (int i = 4; i >= 0; i--) {
		rs[i] = instruction[i + 21];
	}

	//rt
	for (int i = 4; i >= 0; i--) {
		rt[i] = instruction[i + 16];
	}

	//rd
	for (int i = 4; i >= 0; i--) {
		rd[i] = instruction[i + 11];
	}

	//sa
	for (int i = 4; i >= 0; i--) {
		sa[i] = instruction[i + 6];
	}

	//finds func
	for (int i = 5; i >= 0; i--) {
		func[i] = instruction[i];
	}

	//imm
	for (int i = 15; i >= 0; i--) {
		imm[i] = instruction[i];
	}

	//tar
	for (int i = 25; i >= 0; i--) {
		tar[i] = instruction[i];
	}

	//create string stream for retuning the string
	stringstream ss;

	//Invalid
	if (valid.to_string() == "0")
	{
		ss << "Invalid Instruction";
	}

	//NOP
	else if (op.to_string() == "00000" && tar.to_string() == "00000000000000000000000000")
	{
		ss << "NOP";
	}
	
	//J
	else if (op.to_string() == "00010")
	{
		ss << 'J' << "\t\t" << '#' << ((command << 6)>>4);
	}

	//JR
	else if (op.to_string() == "00000" && func.to_string() == "001000")

	{

		ss << "JR" << "\t\t" << 'R' << rs.to_ulong();

	}

	//BEQ
	else if (op.to_string() == "00100")

	{

		ss << "BEQ" << "\t\t" << 'R' << rs.to_ulong() << ", R" + rt.to_ulong() << ", #" << ((command << 16) >> 14);

	}

	//BLTZ
	else if (op.to_string() == "00001")
	{
		bitset<16> tempBitset = imm;
		tempBitset = tempBitset << 2;

		ss << "BLTZ" << "\t" << 'R' << rs.to_ulong() << ", #" << ((command << 16) >> 14);

	}

	//ADD
	else if (op.to_string() == "00000" && func.to_string() == "100000")

	{

		ss << "ADD" << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();

	}

	//ADDI
	else if (op.to_string() == "01000")

	{

		ss << "ADDI" << "\t" << 'R' << rt.to_ulong() << ", R" << rs.to_ulong() << ", #" << ((command << 16) >> 16);

	}

	//SUB
	else if (op.to_string() == "00000" && func.to_string() == "100010")
	{
		ss << "SUB" << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}
	
	//SW
	else if (op.to_string() == "01011")
	{
		ss << "SW" << "\t\t" << 'R' << rt.to_ulong() << ", " << ((command << 16) >> 16) << "(R" << rs.to_ulong() << ')';
	}
	
	//LW
	else if (op.to_string() == "00011")
	{
		ss << "LW" << "\t\t" << 'R' << rt.to_ulong() << ", " << ((command << 16) >> 16) << "(R" << rs.to_ulong() << ')';
	}
	
	//SLL
	else if (op.to_string() == "00000" && func.to_string() == "000000")
	{
		ss << "SLL" << "\t\t" << 'R' << rd.to_ulong() << ", R" << rt.to_ulong() << ", #" << sa.to_ulong();
	}
	
	//SRL
	else if (op.to_string() == "00000" && func.to_string() == "000010")
	{
		ss << "SRL" << "\t\t" << 'R' << rd.to_ulong() << ", R" << rt.to_ulong() << ", #" << sa.to_ulong();
	}

	//MUL
	else if (op.to_string() == "11100" && func.to_string() == "000010")
	{
		ss << "MUL" << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//AND
	else if (op.to_string() == "00000" && func.to_string() == "100101")
	{
		ss << "AND" << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//OR
	else if (op.to_string() == "00000" && func.to_string() == "100100")
	{
		ss << "OR" << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//MOVZ
	else if (op.to_string() == "00000" && func.to_string() == "001010")
	{
		ss << "MOVZ" << "\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//BREAK
	else if (op.to_string() == "00000" && func.to_string() == "001101")
	{
		ss << "BREAK";
		// ifAfterBreak = true;
		
	}
	
	return ss.str();
}