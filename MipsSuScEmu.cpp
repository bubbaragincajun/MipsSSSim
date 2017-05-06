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
    int data[2];
};

struct CacheSet {
    bool LRU;
    CacheLine line[2];
};

struct PostBuff {
    bool valid;
    int dest, data, instruction;
};

struct PreBuff {
    bool valid;
    int instruction, dest;
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
CacheSet cache[4];
CacheSet nextCache[4];
PostBuff postAlu;
PostBuff postMem;
PreBuff preAlu[2];
PreBuff preMem[2];
PreBuff preIssue[4];
int sp, pc, memstart, cycle;
bool breakHit = false;




string mipsReturn(const int& i);
void disassemble(const string& filename, const string& outfile);
string interpret(const int& i);
void status(ofstream& out);
void showhelpinfo(char* s);
void writeBack();
void ALU();
void ALUIssue(const int& instruction);
void IF();
void Issue();
int pibIndex();
bool buffStall(const int& addr);
bool buffFull ();
bool cacheRead(const int& addr, int& data);
bool cacheWrite(const int& addr, const int& data);
void MEM();
bool cacheRead(const int& addr, int& data);
bool cacheWrite(const int& addr, const int& data);
void preIssueShift(const int& index);
bool checkHazards(const int& index, const int& numReg, int* reg);
void Issue();

string getIsValid(const int& command);
string getOP(const int& command);
string getRS(const int& command);
string getRT(const int& command);
string getRD(const int& command);
string getSA(const int& command);
string getIMM(const int& command);
string getTAR(const int& command);
string getFUNC(const int& command);
 

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
	if ((infile == "") && (outfile == "")){
		cout << "Too few arguments. Please execute: '" << argv[0] << " -h' to see help info\n";
	}
	else {
		//build the virtual memory and print out the disassembled code (using interpret)
		disassemble(infile,outfile);

		//eventually putting the guts of the sim here
		outfile = outfile + "_pipeline.txt";
		ofstream fout(outfile.c_str());
		cycle = 1;

		preIssue[0].valid = false;
		preIssue[1].valid = false;
		preIssue[2].valid = false;
		preIssue[3].valid = false;
		preAlu[0].valid = preAlu[1].valid = false;
		preMem[0].valid = preMem[1].valid = false;
		postAlu.valid = postMem.valid = false;
		while (!breakHit || buffFull()) {
			writeBack();
			MEM();
			ALU();
			Issue();

			if( !breakHit ) {
				IF();
			}
			if(!breakHit || buffFull()) {
				status(fout);
			}
			for (int i = 0; i < 4; i++) {
				cache[i].LRU = nextCache[i].LRU;
				for (int j = 0; j < 2; j++) {
					cache[i].line[j].valid = nextCache[i].line[j].valid;
					cache[i].line[j].dirty = nextCache[i].line[j].dirty;
					cache[i].line[j].tag = nextCache[i].line[j].tag;
					for (int k = 0; k < 2; k++) {
						cache[i].line[j].data[k] = nextCache[i].line[j].data[k];
					}
				}
			}
			if(!breakHit || buffFull()) {
				cycle++;
			}
		}
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 2; j++) {
				if (cache[i].line[j].valid && cache[i].line[j].dirty) {
					bitset<27> tag(cache[i].line[j].tag);
					bitset<2>  index(i);
					string address = tag.to_string() + index.to_string() + "000";
					bitset<32> bitAddr(address);
					int addr = (int)bitAddr.to_ulong();
					memarray[(addr-96)/4] = cache[i].line[j].data[0];
					memarray[(addr-92)/4] = cache[i].line[j].data[1];
					cache[i].line[j].dirty = false;
				}
			}
		}
		status(fout);
		fout.close();
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
		postAlu.instruction = 0;
    }
    //check if post mem buffer is ready to write back
    if (postMem.valid) {
        r[postMem.dest] = postMem.data;
        postMem.valid = false;
        postMem.dest = 0;
        postMem.data = 0;
		postMem.instruction = 0;
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
    unsigned int op, rs, rt, rd, shift, aluOp, result, dest;
	int imm;
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


    switch (op) {
        //ADD
        case 32:
			result = r[rs] + r[rt];
			dest = rd;
            break;
        //ADDI
        case 8:
			result = r[rs] + imm;
			dest = rt;
            break;
        //SUB
        case 34:
			result = r[rs] - r[rt];
			dest = rd;
            break;
        //SLL
        case 0:
			result = r[rt] << shift;
			dest = rd;
            break;
        //SRL
        case 2:
			result = r[rt] >> shift;
			dest = rd;
            break;
        //MUL
        case 28:
			result = r[rs] * r[rt];
			dest = rd;
			break;
        //AND
        case 37:
			result = r[rs] & r[rt];
			dest = rd;
            break;
        //OR
        case 36:
			result = r[rs] | r[rt];
			dest = rd;
            break;
        //MOVZ?
        case 10:
			result = r[rd];
			dest = rd;
			if (r[rt]==0) {
				result = r[rs];
			}
            break;
    }

	postAlu.valid = true;
	postAlu.instruction = instruction;
	postAlu.dest = dest;
	postAlu.data = result;

}

void MEM() {
	if (preMem[0].valid) {
		unsigned int command, op, rs, rt, imm, addr;
		int data(0);
		command = preMem[0].instruction;
		op = command << 1;
		op >>= 27;
		rs = command << 6;
    	rs >>= 27;
    	rt = command << 11;
    	rt >>= 27; 
		imm = command << 16;
   		imm >>= 16;
		addr = imm + r[rs];
		if (op == 3) { //LW
			if(cacheRead(addr, data)) {
				postMem.valid = true;
				postMem.data = data;
				postMem.instruction = command;
				postMem.dest = rt;
				preMem[0].valid = preMem[1].valid;
				preMem[0].instruction = preMem[1].instruction;				
				preMem[0].dest = preMem[1].dest;
				preMem[1].valid = false;
				preMem[1].instruction = 0;				
				preMem[1].dest = 0;
			}
		}
		else { //SW
			if (cacheWrite(addr, r[rt])) {
				preMem[0].valid = preMem[1].valid;
				preMem[0].instruction = preMem[1].instruction;				
				preMem[0].dest = preMem[1].dest;
				preMem[1].valid = false;
				preMem[1].instruction = 0;				
				preMem[1].dest = 0;
			}
		}		
	}
	else { //nothing in the first slot(i.e nothing in the second slot)
		preMem[0] = preMem[1];
		preMem[1].valid = false;
		preMem[1].instruction = 0;
		preMem[1].dest = 0;
	}
}

void Issue() {
	bool ALUready(false), MEMready(false);
	int ALUslot(0), MEMslot(0);
	if (!preAlu[0].valid) {
		ALUready = true;
	}
	else if (!preAlu[1].valid) {
		ALUready = true;
		ALUslot = 1;
	}

	//checks if preMem buffer is able to accept another instruction
	if (!preMem[0].valid) {
		MEMready = true;
	}
	else if (!preMem[1].valid) {
		MEMready = true;
		MEMslot = 1;
	}

	int top = pibIndex();
	int i = 0;

	if (top != 0) {
		while ((ALUready || MEMready) && i < 4 && preIssue[i].valid ) {
			int command = preIssue[i].instruction;
			string op = getOP(command);
			string rs = getRS(command);
			string rt = getRT(command);
			string rd = getRD(command);
			string shift = getSA(command);
			string func = getFUNC(command);
			string imm = getIMM(command);

			bitset<5> s(rs), t(rt), d(rd);
			
			//Mem instruction
			if (op == "01011") {//sw
				int reg[2] = {(int)s.to_ulong(), (int)t.to_ulong()};
				if (!checkHazards(i,2,reg) && MEMready) {
					preMem[MEMslot] = preIssue[i];
					preIssueShift(i);
					MEMready = false;
				} else {
					i++;
				}
			}
			else if (op == "00011") { //lw
				int reg[2] = {(int)s.to_ulong(), (int)t.to_ulong()};
				if (!checkHazards(i,2,reg) && MEMready) { //no hazards
					preMem[MEMslot] = preIssue[i];
					preIssueShift(i);
					MEMready = false;
				} else {
					i++;
				}				
			}
			else if (op == "00000" && func == "100000"){//add
				int reg[3] = {(int)s.to_ulong(), (int)t.to_ulong(), (int)d.to_ulong()};
				if (!checkHazards(i,3,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "01000"){//addi
				int reg[2] = {(int)s.to_ulong(), (int)t.to_ulong()};
				if (!checkHazards(i,2,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "00000" && func == "100010") {//sub
				int reg[3] = {(int)s.to_ulong(), (int)t.to_ulong(), (int)d.to_ulong()};
				if (!checkHazards(i,3,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "00000" && func == "000000") {//sll
				int reg[2] = {(int)d.to_ulong(), (int)t.to_ulong()};
				if (!checkHazards(i,2,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "00000" && func == "000010") {//srl 
				int reg[2] = {(int)d.to_ulong(), (int)t.to_ulong()};
				if (!checkHazards(i,2,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "11000" && func == "000010") {//MUL
				int reg[3] = {(int)s.to_ulong(), (int)t.to_ulong(), (int)d.to_ulong()};
				if (!checkHazards(i,3,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "00000" && func == "100101") {//and
				int reg[3] = {(int)s.to_ulong(), (int)t.to_ulong(), (int)d.to_ulong()};
				if (!checkHazards(i,3,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "00000" && func == "100100") {//or
				int reg[3] = {(int)s.to_ulong(), (int)t.to_ulong(), (int)d.to_ulong()};
				if (!checkHazards(i,3,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
			else if (op == "00000" && func == "001010") {//movz
				int reg[3] = {(int)s.to_ulong(), (int)t.to_ulong(), (int)d.to_ulong()};
				if (!checkHazards(i,3,reg) && ALUready) {
					preAlu[ALUslot] = preIssue[i];
					preIssueShift(i);
					ALUready = false;
				}
				else {
					i++;
				}
			}
		}
	}
}

void preIssueShift(const int& index) {
	for (int i = index; i < 3; i++) {
		preIssue[i].valid = preIssue[i+1].valid;
		preIssue[i].instruction = preIssue[i+1].instruction;
		preIssue[i].dest = preIssue[i+1].dest;
	}
	preIssue[3].valid = false;
	preIssue[3].instruction = 0;
	preIssue[3].dest = 0;
}

bool checkHazards(const int& index, const int& numReg, int* reg) {
	for (int i = 0; i < index; i++) {
		if (preIssue[i].valid) {
			for (int j = 0; j < numReg; j++) {
				if (reg[j] == preIssue[i].dest) {
					return true;
				}
			}
		}
	}

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < numReg; j++) {
			if ((preMem[i].dest == reg[j] && preMem[i].valid)|| (preAlu[i].valid && preAlu[i].dest == reg[j])) {
				return true;
			}
		}
	}

	for (int i = 0; i < numReg; i++) {
		if ((postMem.dest == reg[i] && postMem.valid)|| (postAlu.dest == reg[i] && postAlu.valid)) {
			return true;
		}
	}

	return false;
}

bool cacheRead(const int& addr, int& data) {
	//get offsets for the cache

	unsigned int tag, word, line;

	tag = addr >> 5;
	word = addr << 29;
	word = word >> 31;
	line = addr << 27;
	line = line >> 30;

	bool success = false;
	CacheSet* cachePtr = &cache[line];

	if (cachePtr->line[0].valid) { //checking if in set 0
		CacheLine* linePtr = &cachePtr->line[0];
		if (linePtr->tag == tag) {
			data = linePtr->data[word];
			success = true;
		}
		linePtr = nullptr;
	}
	if(cachePtr->line[1].valid && !success) { //checking if in set 1
		CacheLine* linePtr = &cachePtr->line[1];
		if (linePtr->tag == tag) {
			data = linePtr->data[word];
			success = true;
		}
		linePtr = nullptr;
	}
	if (!success){
		cachePtr = &nextCache[line];
		int set = cachePtr->LRU? 1: 0;
		CacheLine* linePtr = &cachePtr->line[set];
		linePtr->valid = true;
		linePtr->dirty = false;
		linePtr->tag = tag;
		int index1, index2;
		if (word == 0) {
			index1 = (addr-96)/4;
			index2 = (addr-92)/4;
		}
		else {
			index1 = (addr-100)/4;
			index2 = (addr-96)/4;
		}
		linePtr->data[0] = memarray[index1];
		linePtr->data[1] = memarray[index2];	
		cachePtr->LRU = cachePtr->LRU? false: true;

		
		linePtr = nullptr;
		success = false;
	}
	cachePtr = nullptr;
	return success;
}

bool cacheWrite(const int& addr, const int& data) {
	unsigned int tag, word, line;

	tag = addr >> 5;
	word = addr << 29;
	word = word >> 31;
	line = addr << 27;
	line = line >> 30;


	bool success = false;
	CacheSet* cachePtr = &cache[line];

	if (cachePtr->line[0].valid) { //checking if in set 0
		CacheLine* linePtr = &cachePtr->line[0];
		if (linePtr->tag == tag) {
			linePtr->data[word] = data;
			linePtr->dirty = true;
			CacheLine* nextCacheLine = &nextCache[line].line[0];	
			nextCacheLine->data[word] = data;
			nextCacheLine->dirty = true;
			success = true;
			nextCacheLine = nullptr;
		}
		linePtr = nullptr;
	}
	if(cachePtr->line[1].valid && !success) { //checking if in set 1
		CacheLine* linePtr = &cachePtr->line[1];
		if (linePtr->tag == tag) {
			linePtr->data[word] = data;
			linePtr->dirty = true;
			CacheLine* nextCacheLine = &nextCache[line].line[1];	
			nextCacheLine->data[word] = data;
			nextCacheLine->dirty = true;
			success = true;
			nextCacheLine = nullptr;
		}
		linePtr = nullptr;
	}
	if(!success){
		cachePtr = &nextCache[line];
		int set = cachePtr->LRU? 1: 0;
		CacheLine* linePtr = &cachePtr->line[set];
		linePtr->valid = true;
		linePtr->dirty = false;
		linePtr->tag = tag;
		int index1, index2;
		if (word == 0) {
			index1 = (addr-96)/4;
			index2 = (addr-92)/4;
		}
		else {
			index1 = (addr-100)/4;
			index2 = (addr-96)/4;
		}
		linePtr->data[0] = memarray[index1];
		linePtr->data[1] = memarray[index2];
		cachePtr->LRU = !cachePtr->LRU;

		linePtr = nullptr;
		success = false;
	}
	cachePtr = nullptr;
	return success;
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
void status(ofstream& out) {
	out << "--------------------\n";
	out << "Cycle:" << cycle << "\n\n";

	//[re issue buffer]
	out << "Pre-Issue Buffer:\n";
	out << "\tEntry 0:\t" << ((preIssue[0].valid)? ("[" + mipsReturn(preIssue[0].instruction) + "]") : "") << "\n";
	out << "\tEntry 1:\t" << ((preIssue[1].valid)? ("[" + mipsReturn(preIssue[1].instruction) + "]") : "") << "\n";
	out << "\tEntry 2:\t" << ((preIssue[2].valid)? ("[" + mipsReturn(preIssue[2].instruction) + "]") : "") << "\n";
	out << "\tEntry 3:\t" << ((preIssue[3].valid)? ("[" + mipsReturn(preIssue[3].instruction) + "]") : "") << "\n";
	out << "Pre_ALU Queue:\n";
	out << "\tEntry 0:\t" << ((preAlu[0].valid)? ("[" + mipsReturn(preAlu[0].instruction) + "]") : "") << "\n";
	out << "\tEntry 1:\t" << ((preAlu[1].valid)? ("[" + mipsReturn(preAlu[1].instruction) + "]") : "") << "\n";
	out << "Post_ALU Queue:\n";
	out << "\tEntry 0:\t" << ((postAlu.valid)? ("[" + mipsReturn(postAlu.instruction) + "]") : "") << "\n";
	out << "Pre_MEM Queue:\n";
	out << "\tEntry 0:\t" << ((preMem[0].valid)? ("[" + mipsReturn(preMem[0].instruction) + "]") : "") << "\n";
	out << "\tEntry 1:\t" << ((preMem[1].valid)? ("[" + mipsReturn(preMem[1].instruction) + "]") : "") << "\n";
	out << "Post_MEM Queue:\n";
	out << "\tEntry 0:\t" << ((postMem.valid)? ("[" + mipsReturn(postMem.instruction) + "]") : "") << "\n";

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

	out << "Cache\n";
	out << "Set 0: LRU=" << cache[0].LRU << "\n";
	out << "\tEntry 0: [(" << cache[0].line[0].valid << ", " << cache[0].line[0].dirty << ", "<< cache[0].line[0].tag << ")<" << interpret(cache[0].line[0].data[0]) << ", " << interpret(cache[0].line[0].data[1]) << ">]\n";
	out << "\tEntry 1: [(" << cache[0].line[1].valid << ", " << cache[0].line[1].dirty << ", "<< cache[0].line[1].tag << ")<" << interpret(cache[0].line[1].data[0]) << ", " << interpret(cache[0].line[1].data[1]) << ">]\n";

	out << "Set 1: LRU=" << cache[1].LRU << "\n";
	out << "\tEntry 0: [(" << cache[1].line[0].valid << ", " << cache[1].line[0].dirty << ", "<< cache[1].line[0].tag << ")<" << interpret(cache[1].line[0].data[0]) << ", " << interpret(cache[1].line[0].data[1]) << ">]\n";
	out << "\tEntry 1: [(" << cache[1].line[1].valid << ", " << cache[1].line[1].dirty << ", "<< cache[1].line[1].tag << ")<" << interpret(cache[1].line[1].data[0]) << ", " << interpret(cache[1].line[1].data[1]) << ">]\n";

	out << "Set 2: LRU=" << cache[2].LRU << "\n";
	out << "\tEntry 0: [(" << cache[2].line[0].valid << ", " << cache[2].line[0].dirty << ", "<< cache[2].line[0].tag << ")<" << interpret(cache[2].line[0].data[0]) << ", " << interpret(cache[2].line[0].data[1]) << ">]\n";
	out << "\tEntry 1: [(" << cache[2].line[1].valid << ", " << cache[2].line[1].dirty << ", "<< cache[2].line[1].tag << ")<" << interpret(cache[2].line[1].data[0]) << ", " << interpret(cache[2].line[1].data[1]) << ">]\n";

	out << "Set 3: LRU=" << cache[3].LRU << "\n";
	out << "\tEntry 0: [(" << cache[3].line[0].valid << ", " << cache[3].line[0].dirty << ", "<< cache[3].line[0].tag << ")<" << interpret(cache[3].line[0].data[0]) << ", " << interpret(cache[3].line[0].data[1]) << ">]\n";
	out << "\tEntry 1: [(" << cache[3].line[1].valid << ", " << cache[3].line[1].dirty << ", "<< cache[3].line[1].tag << ")<" << interpret(cache[3].line[1].data[0]) << ", " << interpret(cache[3].line[1].data[1]) << ">]\n";

	out << "Data\n";
	int numData = (sp - memstart)/4;
	int lines = numData/8;	
	int offset = memstart;

	for (int i = 0; i < lines; i++) {
		out << offset << ": ";
		for (int j = (offset-96)/4; j < (offset-96)/4 + 8; j++) {
			out << "\t" << memarray[j] ;
		}
		out << "\n";
		offset += 32;
	}
	int numLeft;
	if (numData <=8 ) {
		numLeft = numData;
	} else {
		numLeft = numData - (lines)*8;
	}
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
	    ss << 'J' << "\t\t" << '#' << ((command << 6) >> 4);
	}

	//JR
	else if (op.to_string() == "00000" && func.to_string() == "001000")

	{

	    ss << "JR"
	       << "\t\t" << 'R' << rs.to_ulong();
	}

	//BEQ
	else if (op.to_string() == "00100")

	{

	    ss << "BEQ"
	       << "\t\t" << 'R' << rs.to_ulong() << ", R" + rt.to_ulong() << ", #" << ((command << 16) >> 14);
	}

	//BLTZ
	else if (op.to_string() == "00001")
	{
	    bitset<16> tempBitset = imm;
	    tempBitset = tempBitset << 2;

	    ss << "BLTZ"
	       << "\t" << 'R' << rs.to_ulong() << ", #" << ((command << 16) >> 14);
	}

	//ADD
	else if (op.to_string() == "00000" && func.to_string() == "100000")

	{

	    ss << "ADD"
	       << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//ADDI
	else if (op.to_string() == "01000")

	{

	    ss << "ADDI"
	       << "\t" << 'R' << rt.to_ulong() << ", R" << rs.to_ulong() << ", #" << ((command << 16) >> 16);
	}

	//SUB
	else if (op.to_string() == "00000" && func.to_string() == "100010")
	{
	    ss << "SUB"
	       << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//SW
	else if (op.to_string() == "01011")
	{
	    ss << "SW"
	       << "\t\t" << 'R' << rt.to_ulong() << ", " << ((command << 16) >> 16) << "(R" << rs.to_ulong() << ')';
	}

	//LW
	else if (op.to_string() == "00011")
	{
	    ss << "LW"
	       << "\t\t" << 'R' << rt.to_ulong() << ", " << ((command << 16) >> 16) << "(R" << rs.to_ulong() << ')';
	}

	//SLL
	else if (op.to_string() == "00000" && func.to_string() == "000000")
	{
	    ss << "SLL"
	       << "\t\t" << 'R' << rd.to_ulong() << ", R" << rt.to_ulong() << ", #" << sa.to_ulong();
	}

	//SRL
	else if (op.to_string() == "00000" && func.to_string() == "000010")
	{
	    ss << "SRL"
	       << "\t\t" << 'R' << rd.to_ulong() << ", R" << rt.to_ulong() << ", #" << sa.to_ulong();
	}

	//MUL
	else if (op.to_string() == "11100" && func.to_string() == "000010")
	{
	    ss << "MUL"
	       << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//AND
	else if (op.to_string() == "00000" && func.to_string() == "100101")
	{
	    ss << "AND"
	       << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//OR
	else if (op.to_string() == "00000" && func.to_string() == "100100")
	{
	    ss << "OR"
	       << "\t\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//MOVZ
	else if (op.to_string() == "00000" && func.to_string() == "001010")
	{
	    ss << "MOVZ"
	       << "\t" << 'R' << rd.to_ulong() << ", R" << rs.to_ulong() << ", R" << rt.to_ulong();
	}

	//BREAK
	else if (op.to_string() == "00000" && func.to_string() == "001101")
	{
	    ss << "BREAK";
	    // ifAfterBreak = true;
	}

	return ss.str();
}

string getIsValid(const int& command) {
	bitset<32> instruction(command);
	bitset<1> valid;

	valid[0] = instruction[31];
	return valid.to_string();
}
string getOP(const int& command) {
	bitset<32> instruction(command);
	bitset<5> op;

	for (int i = 4; i >= 0; i--) {
		op[i] = instruction[i + 26];
	}

	return op.to_string();
}
string getRS(const int& command) {
	bitset<32> instruction(command);
	bitset<5> rs;

	for (int i = 4; i >= 0; i--) {
		rs[i] = instruction[i + 21];
	}

	return rs.to_string();
}
string getRT(const int& command) {
	bitset<32> instruction(command);
	bitset<5> rt;

	for (int i = 4; i >= 0; i--) {
		rt[i] = instruction[i + 16];
	}

	return rt.to_string();
}
string getRD(const int& command) {
	bitset<32> instruction(command);
	bitset<5> rd;

	for (int i = 4; i >= 0; i--) {
		rd[i] = instruction[i + 11];
	}

	return rd.to_string();
}
string getSA(const int& command) {
	bitset<32> instruction(command);
	bitset<5> sa;

	for (int i = 4; i >= 0; i--) {
		sa[i] = instruction[i + 6];
	}

	return sa.to_string();
}
string getIMM(const int& command) {
	bitset<32> instruction(command);
	bitset<16> imm;

	for (int i = 15; i >= 0; i--) {
		imm[i] = instruction[i];
	}

	return imm.to_string();
}
string getTAR(const int& command) {
	bitset<32> instruction(command);
	bitset<26> tar;

	for (int i = 25; i >= 0; i--) {
		tar[i] = instruction[i];
	}

	return tar.to_string();
}
string getFUNC(const int& command) {
	bitset<32> instruction(command);
	bitset<6> func;

	for (int i = 5; i >= 0; i--) {
		func[i] = instruction[i];
	}

	return func.to_string();
}

int pibIndex() /* returns highest open slot */ {
	for(int i = 0; i < 4; i++) {
		if ( preIssue[i].valid == false ) 
			return i;
	}
	
	return -1;
}

bool buffStall(const int& addr) {
	bool shouldStall = false;

	//pre-Issue
	for(int i = 0; i < 4; i++) {
		if ( preIssue[i].dest == addr )
			shouldStall = true;
	}

	//pre-MEM
	for(int i = 0; i < 2; i++) {
		if ( preMem[i].dest == addr )
			shouldStall = true;	
	}

	//post-MEM
	for(int i = 0; i < 1; i++) {
		if ( postMem.dest == addr )
			shouldStall = true;	
	}

	//pre-ALU
	for(int i = 0; i < 2; i++) {
		if ( preAlu[i].dest == addr )
			shouldStall = true;	
	}

	//post-ALU
	for(int i = 0; i < 1; i++) {
		if ( postAlu.dest == addr )
			shouldStall = true;	
	}

	return shouldStall;
}

bool buffFull () {
	return (preIssue[0].valid || preMem[0].valid || preAlu[0].valid || postMem.valid || postAlu.valid);
}

int getDest(const int& command) {
	string rd(getRD(command)), rt(getRT(command)), op(getOP(command)), func(getFUNC(command));
	bitset<5> RD(rd), RT(rt);
	int d((int)RD.to_ulong()), t((int)RT.to_ulong());

	if (op == "00000" && func == "100000") {//add
		return d;
	}
	else if(op == "01000") { //addi
		return t;
	}
	else if(op == "00000" && func == "100010") {//sub
		return d;
	}
	else if(op == "01011") {//sw
		return t;
	}
	else if(op == "00011") {//lw
		return t;
	}
	else if(op == "00000" && func == "000000") {//sll
		return d;
	}
	else if(op == "00000" && func == "000010") {//srl
		return d;
	}
	else if(op == "11000" && func == "000010") {//mul
		return d;
	}
	else if(op == "00000" && func == "100101") {//and
		return d;
	}
	else if(op == "00000" && func == "100100") {//or
		return d;
	}
	else if(op == "00000" && func == "001010") {//movz
		return d;
	}

	return 0;
}
void showhelpinfo(char *s) {
  cout<<"Usage:   "<<s<<" [-option] [argument]"<<endl;
  cout<<"option:  "<<"-h  show help information"<<endl;
  cout<<"         "<<"-i  input file name"<<endl;
  cout<<"         "<<"-o  output file name"<<endl;
  cout<<"example: "<<s<<" -i <input file name> -o <output file prefix>"<<endl;
}

void IF() { 
	int switchp;
	int data;
	bool hit = cacheRead(pc, data);

	


	if ( /* next cache misses */ !hit || /* PIB is full */ pibIndex() == -1) {
		switchp = 1;
	}
	else if ( /* next i is NOP or invalid*/ (getOP(data) == "00000" && getTAR(data) == "00000000000000000000000000") || getIsValid(data)!= "1") {
		switchp = 2;
	}
	else if ( /* i0 is a branch J / JR / BEQ / BLTZ*/ getOP(data) == "00010" || (getOP(data) == "00000" && getFUNC(data) == "001000") || getOP(data) == "00100" || getOP(data) == "00001") {
		switchp = 3;
	}
	else if (/*i is a break*/(getOP(data) == "00000") && (getFUNC(data) == "001101")) {
		switchp = 4;
	}
	else {  /* i0 is not a branch, break, NOP, or miss */
		switchp = 5;
	}

		

	switch (switchp) {
		case 1: /* cache miss or full PIB.  Has to wait  */
			return;
		break;

		case 2:/* instruction is a NOP */
			//fetch, but no PIB push.  Handle completely here.
			pc += 4;
		break;

		case 3:/* instuction is a Branch */ /* prebuff.dest???? */
			//J
			if ( getOP(data) == "00010" ) {
				int addr = data << 6;
				addr >>= 4;
				pc = addr;
			}

			//JR
			if ( getOP(data) == "00000" && getFUNC(data) == "001000" ) {
				string jump = getRS(data);
				bitset<5> reg(jump);
				int addr = (int)(reg.to_ulong());

				//r[addr] reg check fails, stall
				if ( !buffStall(r[addr]) ) {
					pc = r[addr];
					break;
				}

				
			}

			//BEQ
			if ( getOP(data) == "00100" ) {
				string opstring = interpret(data);
				bitset<5> rs(opstring.substr(6,5));
				bitset<5> rt(opstring.substr(11,5));
				int reg1 = (int)(rs.to_ulong());
				int reg2 = (int)(rt.to_ulong());
				int next = pc+4;
				if (r[reg1]==r[reg2]) {	
					next = data << 16;
					next =  next >> 14;
					next = pc + next + 4;
				}

				//r[reg1] & r[reg2] reg check fails, stall
				if ( buffStall(reg1) || buffStall(reg2) ){

					next = pc;
					break;
				}

				pc = next;
			}

			//BLTZ
			if ( getOP(data) == "00001" ) {
				string opstring = interpret(data);
				bitset<5> rs(opstring.substr(6,5));
				int s = data << 6;
				s = s >> 27;
				int next = pc+4;
				if (r[s] < 0 ) {		
					next = data << 16;
					next = next >> 14;
					next = pc + next + 4;
				}

				//r[s] reg check fails, stall
				if ( buffStall(s) ) {
					next = pc;
					break;
				}

				pc = next;
			}
		break; 

		case 4:

			breakHit = true;
			break;

		case 5:/* instruction is normal instruction */
			//push i0 into highest PIB slot. set values
			preIssue[pibIndex()].instruction = data;
			preIssue[pibIndex()].dest = getDest(data);
			preIssue[pibIndex()].valid = true;
			pc += 4;

			if ( /* PIB has space for another instruction */ pibIndex() != -1 ) {
				//push i1 into highest PIB slot.  handle if it's a branch instruction
				hit = cacheRead(pc, data);
				if ( /* next cache misses */ !hit || /* PIB is full */ pibIndex() == -1) {
					switchp = 1;
				}
				else if ( /* next i is NOP or invalid*/ (getOP(data) == "00000" && getTAR(data) == "00000000000000000000000000") || getIsValid(data)!= "1") {
					switchp = 2;
				}
				else if ( /* i0 is a branch J / JR / BEQ / BLTZ*/ getOP(data) == "00010" || (getOP(data) == "00000" && getFUNC(data) == "001000") || getOP(data) == "00100" || getOP(data) == "00001") {
					switchp = 3;
				}
				else if (/*i is a break*/getOP(data) == "00000" && getFUNC(data) == "001101") {
					switchp = 4;
				}
				else  /* i0 is not a branch, NOP, or miss */ {
					switchp = 5;
				}

					

				switch (switchp) {
					case 1: /* cache miss or full PIB.  Has to wait  */
						return;
					break;

					case 2:/* instruction is a NOP */
						//fetch, but no PIB push.  Handle completely here.
						pc += 4;
					break;

					case 3:/* instuction is a Branch */ /* prebuff.dest???? */
						//J
						if ( getOP(data) == "00010" ) {
							int addr = data << 6;
							addr >>= 4;
							pc = addr;
						}

						//JR
						if ( getOP(data) == "00000" && getFUNC(data) == "001000" ) {
							string jump = getRS(data);
							bitset<5> reg(jump);
							int addr = (int)(reg.to_ulong());

							//r[addr] reg check fails, stall
							if ( buffStall(r[addr]) ) {
								pc = r[addr];
								break;
							}
						}

						//BEQ
						if ( getOP(data) == "00100" ) {
							string opstring = interpret(data);
							bitset<5> rs(opstring.substr(6,5));
							bitset<5> rt(opstring.substr(11,5));
							int reg1 = (int)(rs.to_ulong());
							int reg2 = (int)(rt.to_ulong());
							int next = pc+4;
							if (r[reg1]==r[reg2]) {	
								next = data << 16;
								next =  next >> 14;
								next = pc + next + 4;
							}

							//r[reg1] & r[reg2] reg check fails, stall
							if ( buffStall(r[reg1]) || buffStall(r[reg2]) ){
								next = pc;
							}

							pc = next;
						}

						//BLTZ
						if ( getOP(data) == "00001" ) {
							string opstring = interpret(data);
							bitset<5> rs(opstring.substr(6,5));
							int s = data << 6;
							s = s >> 27;
							int next = pc+4;
							if (r[s] < 0 ) {		
								next = data << 16;
								next = next >> 14;
								next = pc + next + 4;
							}

							//r[s] reg check fails, stall
							if ( buffStall(r[s]) ) {
								next = pc;
								break;
							}

							pc = next;
						}
					break; 

					case 4:

						breakHit = true;
						break;

					case 5:/* instruction is normal instruction */
						//push i0 into highest PIB slot. set values
						preIssue[pibIndex()].instruction = data;
						preIssue[pibIndex()].dest = getDest(data);
						preIssue[pibIndex()].valid = true;
						pc += 4;
					break;
		
				}
			break;
			}
	}
}