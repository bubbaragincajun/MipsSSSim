#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>
#include <bitset>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;

int memarray[100];
int r[32];
int sp, pc, memstart, cycle;

string mipsReturn(const int& i);
void disassemble(const string& filename, const string& outfile);
string interpret(const int& i);
void status(const int& i, ofstream& out);
void J(const int& op, ofstream& fout);
void JR(const int& op, ofstream& fout);
void BEQ(const int& op, ofstream& fout);
void BLTZ(const int& op, ofstream& fout);
void ADD(const int& op, ofstream& fout);
void ADDI(const int& op, ofstream& fout);
void SUB(const int& op, ofstream& fout);
void SW(const int& op, ofstream& fout);
void LW(const int& op, ofstream& fout);
void SLL(const int& op, ofstream& fout);
void SRL(const int& op, ofstream& fout);
void MUL(const int& op, ofstream& fout);
void AND(const int& op, ofstream& fout);
void OR(const int& op, ofstream& fout);
void MOVZ(const int& op, ofstream& fout);
void NOP(const int& op, ofstream& fout);
void showhelpinfo(char *s);

int main(int argc, char* argv[]) {
	char tmp;
	string infile("");
	string outfile("");
	/*if the program is ran witout options ,it will show the usgage and exit*/
	if(argc == 1)
	{
		showhelpinfo(argv[0]);
		return(1);
	}
	/*use function getopt to get the arguments with option."hu:p:s:v" indicate 
	that option h,v are the options without arguments while u,p,s are the
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
		ofstream fout(outfile.c_str());
		cycle = 1;
		bool notDone = true;
		while (notDone) {
			int location = (pc-96)/4;
			int command = memarray[location];
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
			if (valid.to_string() == "0") {
				pc += 4;
			}

			//NOP
			else if (op.to_string() == "00000" && tar.to_string() == "00000000000000000000000000") {NOP(command,fout);}
			
			//J
			else if (op.to_string() == "00010")	{J(command,fout);}

			//JR
			else if (op.to_string() == "00000" && func.to_string() == "001000")	{JR(command,fout);}

			//BEQ
			else if (op.to_string() == "00100")	{BEQ(command,fout);}

			//BLTZ
			else if (op.to_string() == "00001")	{BLTZ(command,fout);}

			//ADD
			else if (op.to_string() == "00000" && func.to_string() == "100000")	{ADD(command,fout);}

			//ADDI
			else if (op.to_string() == "01000")	{ADDI(command,fout);	}

			//SUB
			else if (op.to_string() == "00000" && func.to_string() == "100010")	{SUB(command,fout);}
			
			//SW
			else if (op.to_string() == "01011") {SW(command,fout);}
			
			//LW
			else if (op.to_string() == "00011") {LW(command,fout);}
			
			//SLL
			else if (op.to_string() == "00000" && func.to_string() == "000000") {SLL(command,fout);}
			
			//SRL
			else if (op.to_string() == "00000" && func.to_string() == "000010") {SRL(command,fout);}

			//MUL
			else if (op.to_string() == "11100" && func.to_string() == "000010") {MUL(command,fout);}

			//AND
			else if (op.to_string() == "00000" && func.to_string() == "100101") {AND(command,fout);}

			//OR
			else if (op.to_string() == "00000" && func.to_string() == "100100") {OR(command,fout);}

			//MOVZ
			else if (op.to_string() == "00000" && func.to_string() == "001010") {MOVZ(command,fout);}

			//BREAK
			else if (op.to_string() == "00000" && func.to_string() == "001101")
			{
				status(command, fout);
				notDone = false;						
			}
		}
		fout.close();
	}
	return 0;
}

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

//returns bitstring representation of the command
string interpret(const int& i) {
	bitset<32> op(i);
	string bitstring = op.to_string();
	return bitstring;
}

void status(const int& i, ofstream& out) {
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
}


void J(const int& op, ofstream& fout) {
	string opstring = interpret(op);
	int addr = op << 6;
	addr >>= 4;
	status(op, fout);
	pc = addr;
	cycle ++;
}

void JR(const int& op, ofstream& fout) {
	string opstring = interpret(op);
	string jump = opstring.substr(6,5);
	bitset<5> reg(jump);
	int addr = (int)(reg.to_ulong());
	status(op, fout);
	pc = r[addr];
	cycle++;
}

void BEQ(const int& op, ofstream& fout) {
	string opstring = interpret(op);
	bitset<5> rs(opstring.substr(6,5));
	bitset<5> rt(opstring.substr(11,5));
	int reg1 = (int)(rs.to_ulong());
	int reg2 = (int)(rt.to_ulong());
	int next = pc+4;
	if (r[reg1]==r[reg2]) {	
		next = op << 16;
		next =  next >> 14;
		next = pc + next + 4;
	}
	status(op, fout);
	pc = next;
	cycle++;
}

void BLTZ(const int& op, ofstream& fout) {
	string opstring = interpret(op);
	bitset<5> rs(opstring.substr(6,5));
	int s = op << 6;
	s = s >> 27;
	int next = pc+4;
	if (r[s] < 0 ) {		
		next = op << 16;
		next = next >> 14;
		next = pc + next + 4;
	}
	status(op, fout);
	pc = next;
	cycle++;
}

void ADD(const int& op, ofstream& fout) {
	string opstring = interpret(op);
	bitset<5> rs(opstring.substr(6,5)); 
	bitset<5> rt(opstring.substr(11,5));	
	bitset<5> rd(opstring.substr(16,5));
	int d, s, t;
	d = (int)(rd.to_ulong());
	s = (int)(rs.to_ulong());
	t = (int)(rt.to_ulong());
	r[d] = r[s] + r[t];
	
	status(op, fout);
	pc += 4;
	cycle++;
}

void ADDI(const int& op, ofstream& fout) {
	string opstring = interpret(op);
	bitset<5> rs(opstring.substr(6,5)); 
	bitset<5> rt(opstring.substr(11,5));	
	int imm = op << 16;
	imm = imm >> 16;
	int s, t;
	s = (int)(rs.to_ulong());
	t = (int)(rt.to_ulong());
	r[t] = r[s] + imm;
	status(op, fout);
	pc += 4;
	cycle++;
}

void SUB(const int& op, ofstream& fout) {
	string opstring = interpret(op);
	bitset<5> rs(opstring.substr(6,5)); 
	bitset<5> rt(opstring.substr(11,5));	
	bitset<5> rd(opstring.substr(16,5));
	int d, s, t;
	d = (int)(rd.to_ulong());
	s = (int)(rs.to_ulong());
	t = (int)(rt.to_ulong());
	r[d] = r[s] - r[t];
	status(op, fout);
	pc += 4;
	cycle++;
}

void SW(const int& op, ofstream& fout) {
	string opcode = interpret(op);
	bitset<5> rs(opcode.substr(6,5));
	bitset<5> rt(opcode.substr(11,5));
	int imm = op << 16;
	imm >>= 16;
	int s, t;
	s = (int)(rs.to_ulong());
	t = (int)(rt.to_ulong());
	int offset = imm + r[s];
	offset -= 96;
	offset /= 4;
	memarray[offset] = r[t];
	status(op, fout);
	pc += 4;
	cycle++;
}

void LW(const int& op, ofstream& fout) {
	string opcode = interpret(op);
	bitset<5> rs(opcode.substr(6,5));
	bitset<5> rt(opcode.substr(11,5));
	int imm = op << 16;
	imm >>= 16;
	int s, t;
	s = (int)(rs.to_ulong());
	t = (int)(rt.to_ulong());
	int offset = imm + r[s];
	offset -= 96;
	offset /= 4;
	r[t] = memarray[offset];
	status(op, fout);
	pc += 4;
	cycle++;
}

void SLL(const int& op, ofstream& fout) {
	string opcode = interpret(op);
	bitset<5> rd(opcode.substr(16,5));
	bitset<5> rt(opcode.substr(11,5));
	bitset<5> shift(opcode.substr(21,5));
	int d, t, s;
	d = (int)(rd.to_ulong());
	t = (int)(rt.to_ulong());
	s = (int)(shift.to_ulong());
	r[d] = r[t] << s;
	status(op, fout);
	pc += 4;
	cycle++;
}

void SRL(const int& op, ofstream& fout) {	
	string opcode = interpret(op);
	bitset<5> rd(opcode.substr(16,5));
	bitset<5> rt(opcode.substr(11,5));
	bitset<5> shift(opcode.substr(21,5));
	int d, t, s;
	d = (int)(rd.to_ulong());
	t = (int)(rt.to_ulong());
	s = (int)(shift.to_ulong());
	r[d] = r[t] >> s;
	status(op, fout);
	pc += 4;
	cycle++;
}

void MUL(const int& op, ofstream& fout) {	
	string opcode = interpret(op);
	bitset<5> rs(opcode.substr(6,5));
	bitset<5> rd(opcode.substr(16,5));
	bitset<5> rt(opcode.substr(11,5));
	int s, d, t;
	s = (int)(rs.to_ulong());
	d = (int)(rd.to_ulong());
	t = (int)(rt.to_ulong());
	r[d] = r[s] * r[t];
	status(op, fout);
	pc+=4;
	cycle++;
}

void AND(const int& op, ofstream& fout) {	
	string opcode = interpret(op);
	bitset<5> rs(opcode.substr(6,5));
	bitset<5> rd(opcode.substr(16,5));
	bitset<5> rt(opcode.substr(11,5));
	int s, d, t;
	s = (int)(rs.to_ulong());
	d = (int)(rd.to_ulong());
	t = (int)(rt.to_ulong());
	r[d] = r[s] & r[t];
	status(op, fout);
	pc+=4;
	cycle++;
}

void OR(const int& op, ofstream& fout) {	
	string opcode = interpret(op);
	bitset<5> rs(opcode.substr(6,5));
	bitset<5> rd(opcode.substr(16,5));
	bitset<5> rt(opcode.substr(11,5));
	int s, d, t;
	s = (int)(rs.to_ulong());
	d = (int)(rd.to_ulong());
	t = (int)(rt.to_ulong());
	r[d] = r[t] | r[d];
	status(op, fout);
	pc+=4;
	cycle++;
}

void MOVZ(const int& op, ofstream& fout) {	
	string opcode = interpret(op);
	bitset<5> rs(opcode.substr(6,5));
	bitset<5> rd(opcode.substr(16,5));
	bitset<5> rt(opcode.substr(11,5));
	int s, d, t;
	s = (int)(rs.to_ulong());
	d = (int)(rd.to_ulong());
	t = (int)(rt.to_ulong());
	r[d] = (r[t] == 0)? r[s] : r[d];
	status(op, fout);
	pc+=4;
	cycle++;
}

void NOP(const int& op, ofstream& fout) {
	status(op, fout);
	pc += 4;
	cycle ++;
}

string mipsReturn(const int& command)

{

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

void showhelpinfo(char *s) {
  cout<<"Usage:   "<<s<<" [-option] [argument]"<<endl;
  cout<<"option:  "<<"-h  show help information"<<endl;
  cout<<"         "<<"-i  input file name"<<endl;
  cout<<"         "<<"-o  output file name"<<endl;
  cout<<"example: "<<s<<" -i <input file name> -o <output file prefix>"<<endl;
}
