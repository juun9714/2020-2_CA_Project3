#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
/*
�� �ܰ� ���̿� ������ ������ �������� ���� �����
1. IF/ID = pc+4, instruction 32 bit(IF/ID.opcode, IF/ID.rs, IF/ID.rt) 
2. ID/EX = funct,rs,rt,rd index -> control signal
funct code�� control signal ����� �Լ� �����?
3. EX/MEM
4. MEM/WB
*/


/*
*
* 
* check sum�� id/ex.rs_val���� forward ������ ������ �ϱ�
* --> check sum bypassing ����� �׻� ���� ���� 
<hazard �ؾ��� ��>

�ϴ� hazard���ٰ� �����ϰ� ���ư��� �س���, �� �ܰ� �������� ���ǹ����� hazard ó���ϱ� 
1. data hazard
0:no forward, 1:mem forward, 2:EX forward, 3:Bypassing

	- register�� ������ ���� ���� inst�� ���� ������ ��� �ϴ� ��Ȳ

		1) EX HAZARD
		- regWrite�� ��� + �ٷ� ������ �ش� reg�� ���پ���(j�� �ش� �ȵ�): EX�� ����� EX�� ���� �־��ش� 
		(if (ex/mem.dst == id/ex.rs or id/ex.rt //�� ó���� ���� Ȯ��?) 
		--> id/ex.rs_val or id/ex.rt_val=ex/mem.alu_res)
		--> forward

		memtoReg=0
		memWrite=0
		memRead=0
		RegWrite=1

		if(ex/mem.dst==id/ex.rs, rt && ex/mem.co.regWrite=1 && ex.mem.dst!=0)
		{
		id/ex.val=ex/mem.alu_res
		
		dst==rs
		forwardA(rs)=2
		dst==rt
		forwardB(rt)=2
		}

		2) MEM HAZARD (EX hazard�� ���� ���� �ߵ� + �׳� BYPASS�� �̰ɷ�?)
		regWrite�� ��� + ���� ������ �ش� reg�� ���پ��� : Ex�� ����� mem/wb.data
		(if(mem/wb.dst==id/ex.rs or rt)
		id/ex.rs_val or rt_val=mem/wb.data

		if(mem/wb.dst==id/ex.rs or rt && mem/wb.co.regWrite==1 && mem/wb.dst !=0 
		&& !(ex/mem.dst==id/ex.rs, rt && ex/mem.co.regWrite=1 && ex.mem.dst!=0))
		{
		id/ex.val=mem/wb.data
		
		dst==rs
		forwardA(rs)=1
		dst==rt
		forwardB(rt)=1
		}

		3) Bypassing <-- mem hazard�� ������ �Ȱ����Ű�����(load���ƴϴϱ�?)
		--> WB �Լ� ������ ���� ������ �ٷ� ����..? ���ڴ� ���߿� 
		--> bypass ������ ���� checksum���� �ش�
		if(mem/wb.dst==id/ex.rs or rt)
		id/ex.val=mem/wb.data



		<MEM HAZARD�� ����>
		dst==rs
		forwardA(rs)=1
		dst==rt
		forwardB(rt)=1
		

		4) Load-Use Data Hazard
			1. 
			mem -> ��ĭ ��� -> ex (mem hazard)
			--> control option ���̷� ��ɾ� �޸��ؼ� ���� ����� memRead, memtoreg
			�̰͵� ex hazard ��������..
			if(mem/wb.dst==id/ex.rs or rt && mem/wb.regWrite=1 && mem/wb.dst!=0
			&& !(ex/mem.dst==id/ex.rs, rt && ex/mem.co.regWrite=1 && ex.mem.dst!=0))
			id/ex.val=mem/wb.data

			2.
			mem -> �ٷ� ex (bubble �ʿ�)

			if(id/ex.rt == if/id.rs or rt)

			if/ex.co.all = 0 // do nothing at ex mem wb for this time
			do not change value of IF/ID register and PC value

			IF �Լ� ������ if/id ���� �������״� IF �Լ� �� ù�� �̷� ��Ȳ���� check �ϴ� ���ǽ�
			+ �� �ٷ� ���� ��� ������ pc �ٲٴ� �� ���ǹ� �ȿ� �־ �̷� ��Ȳ�̸� pc�� ���� ���� �ʱ�
			--> PCWrite = 0, IF_IDWrite= 0 --> �������� ����
*/


typedef struct control_options {
	char RegDst;
	char MemtoReg;
	char RegWrite;
	char MemRead;
	char MemWrite;
	char Branch;
	char IF_Flush;
	char ForwardA; //RS 0~3������ ǥ���ϸ�� 0:no forward, 1:mem forward, 2:EX forward
	char ForwardB; //RT 0~3������ ǥ���ϸ�� 0:no forward, 1:mem forward, 2:EX forward
}CO;
typedef struct IF_ID {
	char opcode[7];//opcode[6]�� null�� �ʱ�ȭ
	char funct[7];//funct[6]�� null�� �ʱ�ȭ
	char shamt[6];
	int rs;
	int rt;
	int rd;
	int imm;
	int targ_addr;
	char pc_register[33];
}IFID;
typedef struct ID_EX {
	char opcode[7];//opcode[6]�� null�� �ʱ�ȭ
	char funct[7];//funct[6]�� null�� �ʱ�ȭ
	char shamt[6];
	int rs_val;
	int rt_val;
	int rd_val;
	int rs; 
	int rt;//ex stage���� rt�� rd�� ��� real dst���� RegDst�� �Ǻ�
	int rd;
	int imm;
	CO cont_op;
}IDEX;

typedef struct EX_MEM {
	char opcode[7];//opcode[6]�� null�� �ʱ�ȭ
	char funct[7];//funct[6]�� null�� �ʱ�ȭ
	char shamt[6];
	int alu_res;
	int dst_reg_id;
	int rs_val;
	int rt_val;
	int rd_val;
	int rs;
	int rt;//ex stage���� rt�� rd�� ��� real dst���� RegDst�� �Ǻ�
	int rd;
	int imm;
	CO cont_op;
}EXMEM;

typedef struct MEM_WB {
	char opcode[7];//opcode[6]�� null�� �ʱ�ȭ
	char funct[7];//funct[6]�� null�� �ʱ�ȭ
	char shamt[6];
	int data;//reg�� ����� ��� �����, mem���� load�ؿ� ���̵� �ϴ� MEM/WB register�� �����ϰ�, WB stage���� muxing
	int dst_reg_id;
	int rs_val;
	int rt_val;
	int rd_val;
	int rs;
	int rt;
	int rd;
	int imm;
	CO cont_op;
}MEMWB;

//for storing each stage's result
IFID ifid; 
IDEX idex; 
EXMEM exmem; 
MEMWB memwb;

//for using by each stage
//need to be updated when each cycle is done
IFID loc_ifid;
IDEX loc_idex;
EXMEM loc_exmem;
MEMWB loc_memwb;

int cycle = 0;
char PCWrite=1;
char IF_IDWrite=1;
int branch_bypass = 0;
int morecycle = 0;
unsigned int checksum = 0;
int if_f = 0;
int id_f = 0;
int ex_f = 0;
int mem_f = 0;
int wb_f = 0;

void Bin(int deci, char* result) {
	//int deci => char * result(binary string)
	//result�� �������κ��� ���� --> ��ȯ����,,

	result[8] = '\0'; //������ ��Ҵ� null�� �ʱ�ȭ 
	for (int i = 7; i >= 0; i--) {
		//���� ù���� LSB�̰�, LSB�� result[7]�� ������ �Ŵ�. 
		result[i] = (deci % 2) + '0';//'0'�� ���ؼ� ���ڷ� ������ش�. ���� 0�ƴϸ� 1��
		deci = deci / 2;
	}

}

//register print function
int Regi(char* regi) {
	//5bit
	if (!strncmp(regi, "00000", 5))
		return 0;
	else if (!strncmp(regi, "00001", 5))
		return 1;
	else if (!strncmp(regi, "00010", 5))
		return 2;
	else if (!strncmp(regi, "00011", 5))
		return 3;
	else if (!strncmp(regi, "00100", 5))
		return 4;
	else if (!strncmp(regi, "00101", 5))
		return 5;
	else if (!strncmp(regi, "00110", 5))
		return 6;
	else if (!strncmp(regi, "00111", 5))
		return 7;
	else if (!strncmp(regi, "01000", 5))
		return 8;
	else if (!strncmp(regi, "01001", 5))
		return 9;
	else if (!strncmp(regi, "01010", 5))
		return 10;
	else if (!strncmp(regi, "01011", 5))
		return 11;
	else if (!strncmp(regi, "01100", 5))//12
		return 12;
	else if (!strncmp(regi, "01101", 5))//13
		return 13;
	else if (!strncmp(regi, "01110", 5))//14
		return 14;
	else if (!strncmp(regi, "01111", 5))//15
		return 15;
	else if (!strncmp(regi, "10000", 5))//16
		return 16;
	else if (!strncmp(regi, "10001", 5))//17
		return 17;
	else if (!strncmp(regi, "10010", 5))//18
		return 18;
	else if (!strncmp(regi, "10011", 5))//19
		return 19;
	else if (!strncmp(regi, "10100", 5))//20
		return 20;
	else if (!strncmp(regi, "10101", 5))//21
		return 21;
	else if (!strncmp(regi, "10110", 5))//22
		return 22;
	else if (!strncmp(regi, "10111", 5))//23
		return 23;
	else if (!strncmp(regi, "11000", 5))//24
		return 24;
	else if (!strncmp(regi, "11001", 5))//25
		return 25;
	else if (!strncmp(regi, "11010", 5))//26
		return 26;
	else if (!strncmp(regi, "11011", 5))//27
		return 27;
	else if (!strncmp(regi, "11100", 5))//28
		return 28;
	else if (!strncmp(regi, "11101", 5))//29
		return 29;
	else if (!strncmp(regi, "11110", 5))//30
		return 30;
	else if (!strncmp(regi, "11111", 5))//31
		return 31;
}
//�ش��ϴ� register �ε����� ��ȯ

void Shift(char* regi) {
	//5bit

	if (!strncmp(regi, "00000", 5))
		printf("0");
	else if (!strncmp(regi, "00001", 5))
		printf("1");
	else if (!strncmp(regi, "00010", 5))
		printf("2");
	else if (!strncmp(regi, "00011", 5))
		printf("3");
	else if (!strncmp(regi, "00100", 5))
		printf("4");
	else if (!strncmp(regi, "00101", 5))
		printf("5");
	else if (!strncmp(regi, "00110", 5))
		printf("6");
	else if (!strncmp(regi, "00111", 5))
		printf("7");
	else if (!strncmp(regi, "01000", 5))
		printf("8");
	else if (!strncmp(regi, "01001", 5))
		printf("9");
	else if (!strncmp(regi, "01010", 5))
		printf("10");
	else if (!strncmp(regi, "01011", 5))
		printf("11");//11
	else if (!strncmp(regi, "01100", 5))//12
		printf("12");
	else if (!strncmp(regi, "01101", 5))//13
		printf("13");
	else if (!strncmp(regi, "01110", 5))//14
		printf("14");
	else if (!strncmp(regi, "01111", 5))//15
		printf("15");
	else if (!strncmp(regi, "10000", 5))//16
		printf("16");
	else if (!strncmp(regi, "10001", 5))//17
		printf("17");
	else if (!strncmp(regi, "10010", 5))//18
		printf("18");
	else if (!strncmp(regi, "10011", 5))//19
		printf("19");
	else if (!strncmp(regi, "10100", 5))//20
		printf("20");
	else if (!strncmp(regi, "10101", 5))//21
		printf("21");
	else if (!strncmp(regi, "10110", 5))//22
		printf("22");
	else if (!strncmp(regi, "10111", 5))//23
		printf("23");
	else if (!strncmp(regi, "11000", 5))//24
		printf("24");
	else if (!strncmp(regi, "11001", 5))//25
		printf("25");
	else if (!strncmp(regi, "11010", 5))//26
		printf("26");
	else if (!strncmp(regi, "11011", 5))//27
		printf("27");
	else if (!strncmp(regi, "11100", 5))//28
		printf("28");
	else if (!strncmp(regi, "11101", 5))//29
		printf("29");
	else if (!strncmp(regi, "11110", 5))//30
		printf("30");
	else if (!strncmp(regi, "11111", 5))//31
		printf("31");
}

int bintoDeci(char* bin, int flag) {
	//������ 16bit ��� Ȱ���� ���� ����� �Ŵ� ��?
	//flag is for check if it is signed or unsigned
	//nooooo --> we also have to convert target 
	//if imm = imm[0]"00001101"imm[7]  ==13(10)
	int sum = 0;
	if (flag == 1 || flag == 0) {
		//signed
		if (bin[0] == '0') {
			//positive number
			for (int i = 0; i < 16; i++) {

				if (bin[i] == '1')
					sum += pow(2, 15 - i);
			}
		}
		else if (bin[0] == '1') {
			//negative number
			sum = -pow(2, 16 - 1);
			for (int i = 1; i < 16; i++) {
				if (bin[i] == '1')
					sum += pow(2, 15 - i);
			}
		}
	}
	else if (flag == -1) {
		//26 bit immediate
		//printf("jal immediate: %s\n", bin);
		for (int i = 0; i < 26; i++) {
			if (bin[i] == '1')
				sum += pow(2, 25 - i);
		}
	}
	//else if (flag == 0) {
	//	//unsigned
	//	for (int i = 0; i < 16; i++) {
	//		if (bin[i] == '1')
	//			sum += pow(2, 15 - i);
	//	}
	//}
	return sum;
}

int printMid(char* middle, int* Reg, int* DMem, char* last) {
	//32bit�� ���� �ִ� ���ڿ��� ������ �� 
	//-> 32bit¥�� binary code�� �϶�� ��� �غ���
	//�ϴ� ���� inst mem = last �迭, data memory = DMem, register file = Reg

	//middle : 32bit string
	//Common//
	char forOp[6];
	char rs[5], rt[5], rd[5], shamt[5], target[26];
	int rsi, rti, rdi, shi, immi, tari;
	//what is opcode
	for (int op = 0; op < 6; op++)
		forOp[op] = middle[op];
	
	//opcode = "000000" =>R-type : funct�� Ȯ��
	//�������� �� Ȯ��: �ٸ� �Լ�(Regi)�� ����.
	//shamt �� Ȯ��: �ٸ� �Լ�(Shift)�� ����.

	char forFunct[6];//For R-type
	char Imm[16]; //For I-type
	if (!strncmp(forOp, "000000", 6))
	{//R-type ->  op rs rt rd shamt funct
		for (int func = 0; func < 6; func++)//funct Ȯ��
			forFunct[func] = middle[func + 26];

		for (rsi = 0; rsi < 5; rsi++)//rs Ȯ��
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt Ȯ��
			rt[rti] = middle[rti + 11];

		for (rdi = 0; rdi < 5; rdi++)//rd Ȯ��
			rd[rdi] = middle[rdi + 16];

		for (shi = 0; shi < 5; shi++)//shamt
			shamt[shi] = middle[shi + 21];
		//shamt�� ���� ���� 0~31
		//shamt�� $�Ⱥ��� --> Shift �Լ��� �־ ����ϱ�

		if (!strncmp(forFunct, "100000", 6)) {
			Reg[Regi(rd)] = Reg[Regi(rs)] + Reg[Regi(rt)];
			Reg[32] += 4;
			return 1;
			//add
		}
		else if (!strncmp(forFunct, "100100", 6)) {
			Reg[Regi(rd)] = Reg[Regi(rs)] & Reg[Regi(rt)];
			Reg[32] += 4;
			return 1;
			//and
		}
		else if (!strncmp(forFunct, "100101", 6)) {
			Reg[Regi(rd)] = Reg[Regi(rs)] | Reg[Regi(rt)];
			Reg[32] += 4;
			return 1;
			//or
		}
		else if (!strncmp(forFunct, "101010", 6)) {
			if (Reg[Regi(rs)] < Reg[Regi(rt)])
				Reg[Regi(rd)] = 1;
			else
				Reg[Regi(rd)] = 0;
			Reg[32] += 4;
			return 1;
			//slt
		}
		else if (!strncmp(forFunct, "100010", 6)) {
			Reg[Regi(rd)] = Reg[Regi(rs)] - Reg[Regi(rt)];
			Reg[32] += 4;
			return 1;
			//sub
		}
		else if (!strncmp(forFunct, "000000", 6)) {
			//printf("sll -> nop ");
			if (!(strncmp(rd, "00000", 5)) && !strncmp(rs, "00000", 5) && !strncmp(rt, "00000", 5) && !strncmp(shamt, "00000", 5)) {
				Reg[32] += 4;
				return 1;
			}
			//sll -> nop
		}
		else {
			printf("unknown instruction\n");
			Reg[32] += 4;
			return -1;
		}
	}
	else {
		//For I-type
		for (rsi = 0; rsi < 5; rsi++)//rs Ȯ��
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt Ȯ��
			rt[rti] = middle[rti + 11];

		for (immi = 0; immi < 16; immi++)//immediate Ȯ��
			Imm[immi] = middle[immi + 16];

		//For J and JAL
		for (tari = 0; tari < 26; tari++) //immediate == 26bit
			target[tari] = middle[tari + 6];


		if (!strncmp(forOp, "001000", 6)) {
			Reg[Regi(rt)] = Reg[Regi(rs)] + bintoDeci(Imm, 1);
			Reg[32] += 4;
			return 1;

			//addi
		}
		else if (!strncmp(forOp, "001100", 6)) {
			Reg[Regi(rt)] = Reg[Regi(rs)] & (0x0000FFFF & bintoDeci(Imm, 1));
			Reg[32] += 4;
			return 1;

			//andi
		}
		else if (!strncmp(forOp, "000100", 6)) {
			if (Reg[Regi(rs)] == Reg[Regi(rt)])
				Reg[32] = Reg[32] + 4 + (4 * bintoDeci(Imm, 1));
			else
				Reg[32] += 4;

			return 1;

			//beq, offset
		}
		else if (!strncmp(forOp, "000101", 6)) {
			if (Reg[Regi(rs)] != Reg[Regi(rt)])
				Reg[32] = Reg[32] + 4 + (4 * bintoDeci(Imm, 1));
			else
				Reg[32] += 4;

			return 1;

			//bne
		}
		else if (!strncmp(forOp, "001111", 6)) {
			Reg[Regi(rt)] = (bintoDeci(Imm, 1) << 16);
			Reg[32] += 4;
			return 1;

			//lui
		}
		else if (!strncmp(forOp, "100011", 6)) {
			/*printf("i am lw\n");
			printf("%x\n", DMem[((Reg[Regi(rs)]+bintoDeci(Imm,1))-0x10000000)/4]);
			printf("%x\n", bintoDeci(Imm, 1));*/
			Reg[Regi(rt)] = DMem[((Reg[Regi(rs)] + bintoDeci(Imm, 1)) - 0x10000000) / 4];
			Reg[32] += 4;
			return 1;

			//DMem�� ������ �迭(4 byte ����)�̱� ������ 0x4�� �ּҰ��� ���� �޸��� ������(1 byte ����)�� ��� �ʹٸ�, DMem[0x1]�� �����ؾ��Ѵ�. 
			//lw
			//lw $2, 1073741824($10)
		}
		else if (!strncmp(forOp, "001101", 6)) {
			Reg[Regi(rt)] = Reg[Regi(rs)] | (0x0000FFFF & bintoDeci(Imm, 1));
			Reg[32] += 4;
			return 1;

			//ori
		}
		else if (!strncmp(forOp, "101011", 6)) {
			/*printf("i am sw\n");
			printf("%x\n", Reg[Regi(rt)]);
			printf("%x\n",Reg[Regi(rs)]-0x10000000);
			printf("%x\n",bintoDeci(Imm,1))*/;
			DMem[(Reg[Regi(rs)] + bintoDeci(Imm, 1) - 0x10000000) / 4] = Reg[Regi(rt)];
			Reg[32] += 4;
			return 1;

			//sw
		}
		else if (!strncmp(forOp, "001010", 6)) {
			if (Reg[Regi(rs)] < bintoDeci(Imm, 1))
				Reg[Regi(rt)] = 1;
			else
				Reg[Regi(rt)] = 0;
			Reg[32] += 4;
			return 1;

			//slti
		}
		else if (!strncmp(forOp, "000010", 6)) {
			Reg[32] = (0x0FFFFFFF & (bintoDeci(target, -1) << 2)) | ((Reg[32] + 4) & 0xF0000000);

			return 1;

			//j
		}
		else {
			printf("unknown instruction\n");
			Reg[32] += 4;
			return -1;
		}
	}
}

void IF(char* middle,int * Reg) {
	//printf("if stage\n");
	/*
	32 bit string�� ���� by middle

	IF �Լ����� �� ��, pc+4 �Ǵ� target address �Ǵ� jump address �ּҿ� �ִ� instruction �о 
	32bit IF/ID register�� �ɰ��� ����
	�ϴ� ���� inst mem = last �迭, data memory = DMem, register file = Reg
	
	*/
	//printf("im in if stage and this instruction is 0x%s\n",middle);
	strncpy(ifid.pc_register, middle, 32);
	ifid.pc_register[32] = '\0';
	IF_IDWrite = 1;
	//Common//
	char forOp[6];
	char rs[5], rt[5], rd[5], shamt[5], target[26];
	int rsi, rti, rdi, shi, immi, tari;

	//what is opcode
	for (int op = 0; op < 6; op++)
		forOp[op] = middle[op];

	//opcode = "000000" =>R-type : funct�� Ȯ��
	//�������� �� Ȯ��: �ٸ� �Լ�(Regi)�� ����.
	//shamt �� Ȯ��: �ٸ� �Լ�(Shift)�� ����.

	char forFunct[6];//For R-type
	char Imm[16]; //For I-type
	if (!strncmp(forOp, "000000", 6))
	{//R-type ->(add sub and or slt)
	//opcode(6) rs(5) rt(5) rd(5) shamt(5) funct(6)
		//printf("im in if stage's rtype\n");

		for (int func = 0; func < 6; func++)//funct Ȯ��
			forFunct[func] = middle[func + 26];

		for (rsi = 0; rsi < 5; rsi++)//rs Ȯ��
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt Ȯ��
			rt[rti] = middle[rti + 11];

		for (rdi = 0; rdi < 5; rdi++)//rd Ȯ��
			rd[rdi] = middle[rdi + 16];

		for (shi = 0; shi < 5; shi++)//shamt
			shamt[shi] = middle[shi + 21];
		//shamt�� ���� ���� 0~31
		//shamt�� $�Ⱥ��� --> Shift �Լ��� �־ ����ϱ�

		//global FF�� stage���� �� ����
		//IF�� ��� �����پ� ���� middle�� ���� �־����� �Ŵϱ� local �� ���� ���� 
		strncpy(ifid.opcode, forOp, 6);
		ifid.opcode[6] = '\0';
		ifid.rd = Regi(rd);
		ifid.rs = Regi(rs);
		ifid.rt = Regi(rt);
		strncpy(ifid.funct, forFunct, 6);
		ifid.funct[6] = '\0';
		strncpy(ifid.shamt, shamt,5);
		ifid.shamt[5] = '\0';

		//ifid.shamt �� simulator�� shift �ϴ� ��ɾ� ����
		//IF/ID set
	}
	else {

		if (!strncmp(forOp, "000010", 6)) {
			//For J and JAL
			//opcode(6) target(26)
			for (tari = 0; tari < 26; tari++) //immediate == 26bit
				target[tari] = middle[tari + 6];

			strncpy(ifid.opcode, forOp, 6);
			ifid.opcode[6] = '\0';
			ifid.targ_addr = (0x0FFFFFFF & (bintoDeci(target, -1) << 2)) | ((Reg[32] + 4) & 0xF0000000);
			Reg[32] = ifid.targ_addr;
			PCWrite = 0;
		}
		else {
			///printf("im in if stage's I type\n");

			//For I-type (addi andi ori slti) (+ lw, sw, beq, bne, lui)
			//opcode(6) rs(5) rt(5) imm(16)
			for (rsi = 0; rsi < 5; rsi++)//rs Ȯ��
				rs[rsi] = middle[rsi + 6];

			for (rti = 0; rti < 5; rti++)//rt Ȯ��
				rt[rti] = middle[rti + 11];

			for (immi = 0; immi < 16; immi++)//immediate Ȯ��
				Imm[immi] = middle[immi + 16];
			//global�� ����
			strncpy(ifid.opcode, forOp, 6);
			ifid.opcode[6] = '\0';
			ifid.rs = Regi(rs);
			ifid.rt = Regi(rt);
			ifid.imm = bintoDeci(Imm, 1);
		}
	}
}
int ID(int* Reg) {
	branch_bypass = 0;
	//printf("im in id stage\n");
	IF_IDWrite = 1;
	strncpy(idex.opcode, loc_ifid.opcode, 7);
	strncpy(idex.funct, loc_ifid.funct, 7);
	strncpy(idex.shamt, loc_ifid.shamt, 6);

	idex.rd = loc_ifid.rd;
	idex.rs = loc_ifid.rs;
	idex.rt = loc_ifid.rt;
	idex.rd_val = Reg[loc_ifid.rd];
	idex.rs_val = Reg[loc_ifid.rs];
	idex.rt_val = Reg[loc_ifid.rt];
	/*
	IF/ID register���� 
	1. opcode�� control signal ���� ID/EX register�� �ֱ� 
	2. �� register index�� register ��(32bit) �о���� 
	3. rt, rd index �� �� ID/EX register�� �����ϱ� (ex���� dst mux)
	4. beq, bne�� ��� same���� �Ǻ��ϱ�
	5. target address ����ϱ�(beq, bne)
	6. Bypassing
	7. load-use data hazard (mem, id)
		-idex.rt==ifid.rs
		-idex.rt==ifid.rt
		-idex.cont_op.memRead == 1

		exmem.cont_op.regWrite==1
		exmem.reg_dst_id != 0
		�̰� �� ���� bubble ���� hazard ������ �ٽ� �߻��ϴµ�, �׶� �� �����ϱ� ������ ���⼭ ���� �����൵ �ȴ�. 
	8. checksum�� ID stage���� �Ͼ��. 
		- rs register�� value�� ������� check�ϴ� ���̱� ������ !
		- EX MEM HAZARD�� EX�� ALU�� ���� regWrite���� �̸� �������ִ� ����
		- ������ Bypassing�� 
	
	*/
	//7�� ���� �״�� ī���ϴ� ���� -> IF �Լ����� �̹� ������ ��Ҵ� null�� �ʱ�ȭ �߱� ������, �״�� �ű�� null�� ���õ�
	//7. load use data hazard
	//id/ex FF�� ���� 
	//if/id FF�� �״��
	//������ ID/EX rt id�� ���� IF/ID rs rt id�� ���ƾ� ��
	//checksum ����(load use data hazard / general case(including bypassing)) ���ֱ�
	if (loc_idex.cont_op.MemRead == 1 && ((loc_idex.rt == loc_ifid.rs) || (loc_idex.rt == loc_ifid.rt))) {
		/*
		1. ex mem wb�� �۵����� �ʰ�  
			--> idex register�� nop���� ����� 
			--> ���� cycle�� ex mem wb�� nop�� �����ϰ� 
		2. IF/ID FF �״�� ���α� 
			--> cycle�� ������ ��, ��� IF���� update�� ������ loc_ifid�� �������� �ʱ� 
			--> load use data hzd������ IF_IDWrite =0; (�����̴�)
			--> main���� FF ������ ��, if������ ����ó�� ���ٱ�?
			--> ���� cycle�� id�� �Ȱ��� �� �����ض� 

		3. IF�� ���ο� ��� ���� �ʰ� �ϱ�
			--> PC�� ������Ű�� ������ �� 
			--> PCWrite�� �׻� 1�̰�, beq bne j load use data hazard�� ���� 0

		*/
		checksum = (checksum << 1 | checksum >> 31) ^ Reg[loc_ifid.rs];//ID stage �ٷ� ����, checksum �� �ȿ��� ��� ������� 
		strncpy(idex.opcode, "000000\0", 7);
		strncpy(idex.funct, "000000\0", 7);
		strncpy(idex.shamt, "00000\0",6);
		printf("idex.opcode : %s\nidex.funct : %s\nidex.funct : %s\n", idex.opcode, idex.funct, idex.shamt);
		idex.imm = 0;
		idex.rd = 0;
		idex.rt = 0;
		idex.rs = 0;
		idex.rd_val = 0;
		idex.rs_val = 0;
		idex.rt_val = 0;
		//nop's control option
		idex.cont_op.RegDst = 0;
		idex.cont_op.MemtoReg = 0;
		idex.cont_op.RegWrite = 0;
		idex.cont_op.MemRead = 0;
		idex.cont_op.MemWrite = 0;
		idex.cont_op.Branch = 0;
		idex.cont_op.IF_Flush = 0;
		idex.cont_op.ForwardA = 0;
		idex.cont_op.ForwardB = 0;
		PCWrite = 0;
		IF_IDWrite = 0;
		printf("load use data hazard occurred in ID stage\n");
		return 10; //10 means load hzd
	}
	else if (!strncmp(loc_ifid.opcode, "000000", 6))
	{//R-type ->  op rs rt rd shamt funct
		//id/ex register�� reg index, reg value �� �� �ѱ��

		if (!strncmp(loc_ifid.funct, "100000", 6)) {
			printf("add\n");
			idex.cont_op.RegDst = 1; //rd
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush= 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//add
		}
		else if (!strncmp(loc_ifid.funct, "100100", 6)) {
			printf("and\n");

			idex.cont_op.RegDst = 1; //rd
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//and
		}
		else if (!strncmp(loc_ifid.funct, "100101", 6)) {
			printf("or\n");

			idex.cont_op.RegDst = 1; //rd
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//or
		}
		else if (!strncmp(loc_ifid.funct, "101010", 6)) {
			printf("slt\n");

			idex.cont_op.RegDst = 1; //rd
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//slt
		}
		else if (!strncmp(loc_ifid.funct, "100010", 6)) {
			printf("sub\n");

			idex.cont_op.RegDst = 1; //rd
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//sub
		}
		else if (!strncmp(loc_ifid.funct, "000000", 6)) {
			if ((loc_ifid.rd==0) && (loc_ifid.rs == 0) && (loc_ifid.rt == 0) && !strncmp(loc_ifid.shamt, "00000", 5)) {
				printf("nop\n");

				idex.cont_op.RegDst = 0;
				idex.cont_op.MemtoReg = 0;
				idex.cont_op.RegWrite = 0;
				idex.cont_op.MemRead = 0;
				idex.cont_op.MemWrite = 0;
				idex.cont_op.Branch = 0;
				idex.cont_op.IF_Flush = 0;
				idex.cont_op.ForwardA = 0;
				idex.cont_op.ForwardB = 0;
				PCWrite = 1;
				IF_IDWrite = 1;
			}
			//sll -> nop
		}
		else {
			//unknown 0xFFFFFFFF ���͵� �׳� �� ������ �ϰŵ� ����Ʈ ������ ������ ���ÿ�
			idex.cont_op.RegDst = 0; 
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
		}
	}
	else {
		//For I-type
		idex.imm = loc_ifid.imm;
		//For J and JAL
		if (!strncmp(loc_ifid.opcode, "001000", 6)) {
			printf("addi\n");
			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//addi
		}
		else if (!strncmp(loc_ifid.opcode, "001100", 6)) {
			printf("andi\n");

			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			// �� 0x0000FFFF�� &�� ����...
			//andi
		}
		else if (!strncmp(loc_ifid.opcode, "000100", 6)) {
			printf("beq\n");

			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 1;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			IF_IDWrite = 1;

			int rs = 0, rt = 0;
			rs = Reg[loc_ifid.rs];
			rt = Reg[loc_ifid.rt];

			/*
			<lw nop branch>
			exmem.reg-dst == ifid.rs or rt ,exmem.cont-op.memread == 1
			need 1 bubble 

			<lw branch>
			memread Ȱ��
			idex.memread==1,  idex.rt==ifid.rs or rt
			need 2 bubble

			--> �� ��� ��� �� if�� �ɸ��� ������  
			*/
			if ((loc_exmem.cont_op.MemRead && ((loc_exmem.dst_reg_id == loc_ifid.rs) || (loc_exmem.dst_reg_id == loc_ifid.rt))) ||
				(loc_idex.cont_op.MemRead && ((loc_idex.rt == loc_ifid.rs) || (loc_idex.rt == loc_ifid.rt)))) {
				checksum = (checksum << 1 | checksum >> 31) ^ Reg[loc_ifid.rs];//ID stage �ٷ� ����, checksum �� �ȿ��� ��� ������� 
				strncpy(idex.opcode, "000000\0", 7);
				strncpy(idex.funct, "000000\0", 7);
				strncpy(idex.shamt, "00000\0", 6);
				//printf("idex.opcode : %s\nidex.funct : %s\nidex.funct : %s\n", idex.opcode, idex.funct, idex.shamt);
				idex.imm = 0;
				idex.rd = 0;
				idex.rt = 0;
				idex.rs = 0;
				idex.rd_val = 0;
				idex.rs_val = 0;
				idex.rt_val = 0;
				//nop's control option
				idex.cont_op.RegDst = 0;
				idex.cont_op.MemtoReg = 0;
				idex.cont_op.RegWrite = 0;
				idex.cont_op.MemRead = 0;
				idex.cont_op.MemWrite = 0;
				idex.cont_op.Branch = 0;
				idex.cont_op.IF_Flush = 0;
				idex.cont_op.ForwardA = 0;
				idex.cont_op.ForwardB = 0;
				PCWrite = 0;
				IF_IDWrite = 0;
				printf("lw hzd with branch occurred in ID and mem stage -> 1 or 2 bubble\n");
				return 10; //10 means load hzd
			}


			/*
			<lw nop nop branch>
			data hazard�� ��� beq������ bypass why? ID ������ ���� bypassing�� ���� ���� ����� �ؾ��ϱ� ����
			ID stage �������� branch if������ branch_bypass=1�̸�, bypass Ȯ�� ���ϱ�
			*/
			if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rs)) {
				rs = loc_memwb.data;
				idex.rs_val = rs;
				printf("this is beq's bypass rs\nbypassed rs value of branch inst is 0x%08x\n", rs);
				checksum = (checksum << 1 | checksum >> 31) ^ rs;
				//bypass�� ���� ex�� �Ѱ����� �ȳѰ����� �𸣴� �ϴ� ���⼭ rs������ checksum�Ѵ� 
				//�׸��� �ȳѰ� �ְ� nop�� �Ѱ��ְ� �ȴٸ�, �Ʒ� if�� �ȿ��� �˾Ƽ� nop���� �ٽ� �����Ѵ�. 
				branch_bypass = 1;
			}
			else if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rt)){
				rt = loc_memwb.data;
				printf("this is beq's bypass rt\nbypassed rt value of branch inst is 0x%08x\n", rt);
				idex.rt_val = rt;
				checksum = (checksum << 1 | checksum >> 31) ^ rs;//bypass ���⼭ �Ͼ�ϱ�, checksum �� �ȿ��� ��� ������� 
				branch_bypass = 1;
			}

			//�귣ġ�� ID���� ������ Ȯ���ϰ�, PC+4 + offset ����ؼ� ���� PC ������� �� 
			//always not taken 
			if (rs == rt) { //if taken
				printf("beq's condition is taken\n");

				char bubble[33] = "000000000000000000000000000000000";
				bubble[32] = '\0';
				idex.cont_op.IF_Flush = 1; //if �о��������
				/*
				misprediction in beq --> make IF nop
				���� ���� �߿� !!!!!!!
				���� IF ���Ŀ�, ID ����, ID ������ IF 0x0���� �ʱ�ȭ �ϴ� ���� 
				*/
				IF(bubble, Reg);
				//IF(nop) -> global FF�� all 0���� ���� cycle ������ local=global �ϸ� if/id�� nop�� ID���� ����� ����
				//�ʱ�ȭ �� �Ŀ�, pc�� ����� �ٽ� �ϱ� 
				//���� Reg[32] = Reg[32] + 4 + (4 * loc_ifid.imm) <== �� ���̾��µ�, +4�� ���ϱ� ���� �¾����� 
				Reg[32] = Reg[32] + (4 * loc_ifid.imm);
				PCWrite = 0;
				//beq�� �Ѿ ���� ex mem wb���� �ƹ��͵� ���ϴµ�
			}
			//beq, offset
		}
		else if (!strncmp(loc_ifid.opcode, "000101", 6)) {
			printf("bne\n");

			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 1;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			IF_IDWrite = 1;

			/*
			<lw nop branch>
			exmem.reg-dst == ifid.rs or rt ,exmem.cont-op.memread == 1
			need 1 bubble
			*/

			if ((loc_exmem.cont_op.MemRead && ((loc_exmem.dst_reg_id == loc_ifid.rs) || (loc_exmem.dst_reg_id == loc_ifid.rt))) ||
				(loc_idex.cont_op.MemRead && ((loc_idex.rt == loc_ifid.rs) || (loc_idex.rt == loc_ifid.rt)))) {
				checksum = (checksum << 1 | checksum >> 31) ^ Reg[loc_ifid.rs];//ID stage �ٷ� ����, checksum �� �ȿ��� ��� ������� 
				strncpy(idex.opcode, "000000\0", 7);
				strncpy(idex.funct, "000000\0", 7);
				strncpy(idex.shamt, "00000\0", 6);
				//printf("idex.opcode : %s\nidex.funct : %s\nidex.funct : %s\n", idex.opcode, idex.funct, idex.shamt);
				idex.imm = 0;
				idex.rd = 0;
				idex.rt = 0;
				idex.rs = 0;
				idex.rd_val = 0;
				idex.rs_val = 0;
				idex.rt_val = 0;
				//nop's control option
				idex.cont_op.RegDst = 0;
				idex.cont_op.MemtoReg = 0;
				idex.cont_op.RegWrite = 0;
				idex.cont_op.MemRead = 0;
				idex.cont_op.MemWrite = 0;
				idex.cont_op.Branch = 0;
				idex.cont_op.IF_Flush = 0;
				idex.cont_op.ForwardA = 0;
				idex.cont_op.ForwardB = 0;
				PCWrite = 0;
				IF_IDWrite = 0;
				printf("lw hzd with branch occurred in ID and mem stage -> 1 or 2 bubble\n");
				return 10; //10 means load hzd
			}

			int rs = 0, rt = 0;
			rs = Reg[loc_ifid.rs];
			rt = Reg[loc_ifid.rt];

			/*
			<lw nop nop branch>
			data hazard�� ��� beq������ bypass why? ID ������ ���� bypassing�� ���� ���� ����� �ؾ��ϱ� ����
			ID stage �������� branch if������ branch_bypass=1�̸�, bypass Ȯ�� ���ϱ�
			*/
			if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rs)) {
				rs = loc_memwb.data;
				printf("this is bne's bypass rs\nbypassed rs value of branch inst is 0x%08x\n", rs);
				checksum = (checksum << 1 | checksum >> 31) ^ rs;//bypass ���⼭ �Ͼ�ϱ�, checksum �� �ȿ��� ��� ������� 
				branch_bypass = 1;
			}
			else if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rt)) {
				rt = loc_memwb.data;
				printf("this is bne's bypass rt\nbypassed rt value of branch inst is 0x%08x\n", rt);
				//checksum = (checksum << 1 | checksum >> 31) ^ rs;//bypass ���⼭ �Ͼ�ϱ�, checksum �� �ȿ��� ��� ������� 
				branch_bypass = 1;
			}

			//�귣ġ�� ID���� ������ Ȯ���ϰ�, PC+4 + offset ����ؼ� ���� PC ������� �� 
			if (rs != rt) {
				printf("bne's condition is taken\n");
				char bubble[33] = "000000000000000000000000000000000";
				bubble[32] = '\0';
				idex.cont_op.IF_Flush = 1; //if �о��������
				IF(bubble, Reg);
				// id ex mem wb�� nop���� �ʱ�ȭ ��, �ٵ� branch�� pc���� �ݿ��� �ȵǳ�
				//���� Reg[32] = Reg[32] + 4 + (4 * loc_ifid.imm) <== �� ���̾��µ�, +4�� ���ϱ� ���� �¾����� 
				Reg[32] = Reg[32] + (4 * loc_ifid.imm);
				printf("loc_ifid.imm is 0x%08x\n", loc_ifid.imm);
				printf("branched pc value is 0x%08x\n", Reg[32]);
				PCWrite = 0;
			}
			//always not taken(������ �׻� Ʋ�� �Ŷ�� ����)
			//bne, offset
		}
		else if (!strncmp(loc_ifid.opcode, "001111", 6)) {
			printf("lui\n");

			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//Reg[idex.rt] = (bintoDeci(Imm, 1) << 16); -> EX stage
			//lui
		}
		else if (!strncmp(loc_ifid.opcode, "100011", 6)) {
			printf("lw\n");

			//lw
			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 1;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 1;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//Reg[Regi(rt)] = DMem[((Reg[Regi(rs)] + bintoDeci(Imm, 1)) - 0x10000000) / 4];
		}
		else if (!strncmp(loc_ifid.opcode, "001101", 6)) {
			printf("ori\n");

			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			//Reg[Regi(rt)] = Reg[Regi(rs)] | (0x0000FFFF & bintoDeci(Imm, 1));
			//ori
		}
		else if (!strncmp(loc_ifid.opcode, "101011", 6)) {
			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 1;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			printf("sw\n");

			//DMem[(Reg[Regi(rs)] + bintoDeci(Imm, 1) - 0x10000000) / 4] = Reg[Regi(rt)];
			//sw
		}
		else if (!strncmp(loc_ifid.opcode, "001010", 6)) {
			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 1;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			printf("slti\n");

			/*if (Reg[Regi(rs)] < bintoDeci(Imm, 1))
				Reg[Regi(rt)] = 1;
			else
				Reg[Regi(rt)] = 0;*/
			//slti
		}
		else if (!strncmp(loc_ifid.opcode, "000010", 6)) {
			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
			printf("j\n");

			//j
		}
		else {
			//printf("unknown instruction\n");
			idex.cont_op.RegDst = 0;
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 0;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;
		}
	}


	//6. bypassing : wb���� ������ ���� ID stage���� �о ���� stage�� ����������� ��
	//load��, R-type instruction�̵� 
	//�Ϲ����� ��� ���Ŀ� �� ����ó���� if������ ����� �� 
	int bypass = 0;
	//branch���� bypass�� �Ͼ�� �ʾҾ�� �� 
	if (branch_bypass==0 && (loc_memwb.dst_reg_id == loc_ifid.rs) && loc_memwb.cont_op.RegWrite ==1 && loc_memwb.dst_reg_id != 0) {
		idex.rs_val = loc_memwb.data;
		bypass = 1;
	}
	else if (branch_bypass == 0 && (loc_memwb.dst_reg_id == loc_ifid.rt) && loc_memwb.cont_op.RegWrite == 1 && loc_memwb.dst_reg_id != 0) {
		idex.rt_val = loc_memwb.data;
		//checksum�� rs���̶��� ��� -> bypass�� rt���� �Ͼ�� bypass�� ���� checksum ����� ���� 
	}

	//�� ���Ŀ� checksum ������ֱ�
	if (bypass == 1) {
		checksum = (checksum << 1 | checksum >> 31) ^ idex.rs_val; //�������� rs��(global�� ������)�� ���ؼ� checksum ���
	}
	else if(idex.rs<0 || idex.rs>31){
		checksum = (checksum << 1 | checksum >> 31) ^ 0x00000000;
	}
	else {
		checksum = (checksum << 1 | checksum >> 31) ^ idex.rs_val;
	}
	return 1;
}
void EX(char* middle, int* Reg) {
	/*
	1. dst �����ϱ� (regDST -> MUX) o
	2. forwarding ���� ����� (forwardA, forwardB <- EX/MEM.rd, MEM/WB.rd, EX/MEM.regWrite, MEM/WB.regWrite, regWrite -> MUX) o
	3. ��� �ϱ� o
	3-0. ��� ���ϴ� ��� ���� (jump, nop, beq, bne -> branch ��ɾ�� ID �ܰ迡�� ����Ѵ�. )
	3-1. ��� ��� muxing �ϱ�  -> �׳� �� ��ɾ� ����
		- reg + reg (R type) o
		- reg + imm (I type, lw, sw) o
	4. ex/mem�� �����ؾ��� �͵� �����ϱ� 
		- control options o
		- dst reg(register indexes yes, values?) o 
		- ��� ��� (�޸� �ּҰ��̵�, ������ ��� �����) o alu_res

	5. Reg[32]�� ���� ���� �ؾ��ϴ°�,,?
		- pc value�� jump�� beq, bne�� ���� ���۵ȴ�, �� �̿��� ���� �׻� pc+4
			- WB���� �� �ϰ� ���� pc �� �������ֱ� --> common way by main 
		- jump�� IF stage���� ���� pc���� ���� -> jump�� ��� IF stage���� pc�� �����ϱ� 
		- beq bne�� ID stage���� pc���� ���� 
			-> ���� not taken�̶��, cycle�� ���������� 1���� ����
				- continue, �ٸ� ��ɾ��� ������ ��ġ���� pc+4 ���ֱ�
			-> ���� taken�̶��, IF/ID stage�� nop���� �ְ�, bubble �ϳ� ���°Ŵϱ�, 
				- pc���� target instruction's address �־��ְ�
			-> cycle 1 �� ����, ��� ���������� �� ���� (�� �ܰ� ��? or for�� �ȿ���?)
			-> �� �ܰ踦 �ϴٰ��� cycle�� ������ �߰��� �׸��־� �� -> �� �ܰ� ������ cycle ���� �ؾ��� ��
			-> generally cycle grows by 1
			-> But cycle grows by 2 when bubble is inserted (when branch is taken, 

	
	6. Load - use hazard -> MEM stage


		main�� ����
		for(cycle=0;cycle<given_cycle;){
		pc_change=0; ���� ������ �ʱ�ȭ
		IF;
		ID;
		EX;
		MEM;
		WB;
		if(pc_change==0)
			pc=pc+4;
	}
	*/
	
	/*
	* 
	* 
	1. EX HAZARD
		-EX/MEM.dst_reg_id==ID/EX.rs
		-EX/MEM.dst_reg_id==ID/EX.rt
	2. MEM HAZARD -> load use data hazard�� mem hazard�� �ѹ��� �ذ� ���� (bypassing�� ID stage���� �ذ�)
		-MEM/WB.dst_reg_id==ID/EX.rs
		-MEM/WB.dst_reg_id==ID/EX.rt
		
	3. load-use data hazard (mem, id)
		-idex.rt==ifid.rs
		-idex.rt==ifid.rt
		-idex.cont_op.memRead
	exmem.cont_op.regWrite==1
	exmem.reg_dst_id != 0
	�̰� �� ���� bubble ���� hazard ������ �ٽ� �߻��ϴµ�, �׶� �� �����ϱ� ������ ���⼭ ���� �����൵ �ȴ�. 


	*/

	//MEM hzd �����ϰ�, EX hzd ������ �ٽ� �ʱ�ȭ 

	if (loc_memwb.cont_op.RegWrite == 1 && loc_memwb.dst_reg_id != 0 && (loc_memwb.dst_reg_id == loc_idex.rs)
		&& !(loc_exmem.cont_op.RegWrite == 1 && loc_exmem.dst_reg_id != 0 && (loc_exmem.dst_reg_id == loc_idex.rs))) {
		//MEM-Hzd of rs without EX-Hzd of rs -> forwardA = 1
		idex.cont_op.ForwardA = 1;
		loc_idex.rs_val = loc_memwb.data;
		//���� ex stage���� rs_val�� ���� ����� ���� ���� �Ŵϱ� loc_idex.rs_val�� �ٲ� ���� �����������
	}
	if (loc_memwb.cont_op.RegWrite == 1 && loc_memwb.dst_reg_id != 0 && (loc_memwb.dst_reg_id == loc_idex.rt)
		&& !(loc_exmem.cont_op.RegWrite == 1 && loc_exmem.dst_reg_id != 0 && (loc_exmem.dst_reg_id == loc_idex.rt))) {
		//MEM-Hzd of rt without EX-Hzd of rt -> forwardB = 1
		idex.cont_op.ForwardB = 1;
		loc_idex.rt_val = loc_memwb.data;
	}
	if (loc_exmem.cont_op.RegWrite == 1 && loc_exmem.dst_reg_id != 0 && (loc_exmem.dst_reg_id == loc_idex.rs)){
		//EX-Hzd of rs -> forwardA = 2
		idex.cont_op.ForwardA = 2;
		loc_idex.rs_val = loc_exmem.alu_res;
	}
	if (loc_exmem.cont_op.RegWrite == 1 && loc_exmem.dst_reg_id != 0 && (loc_exmem.dst_reg_id == loc_idex.rt)) {
		//EX-Hzd of rt -> forwardB = 2
		//printf("ex hzd of rt  exmem.dst : %d  idex.rt : %d\n", loc_exmem.dst_reg_id, loc_idex.rt);
		idex.cont_op.ForwardB = 2;
		loc_idex.rt_val = loc_exmem.alu_res;
		//printf("ex hzd of rt  loc_idex.rt_val : 0x%08x   loc_exmem.alu_res : 0x%08x\n", loc_idex.rt_val, loc_exmem.alu_res);
	}
	


	if (!strncmp(loc_idex.opcode, "000000", 6))
	{//R-type ->  op rs rt rd shamt funct -> rd is dst regiter id
		exmem.dst_reg_id = loc_idex.rd;
		if (!strncmp(loc_idex.funct, "100000", 6)) {
			printf("add in ex\n");
			exmem.alu_res = loc_idex.rs_val + loc_idex.rt_val;
			//add
		}
		else if (!strncmp(loc_idex.funct, "100100", 6)) {
			printf("and in ex\n");
			exmem.alu_res = (loc_idex.rs_val & loc_idex.rt_val);
			//and
		}
		else if (!strncmp(loc_idex.funct, "100101", 6)) {
			printf("or in ex\n");
			exmem.alu_res = (loc_idex.rs_val | loc_idex.rt_val);
			//or
		}
		else if (!strncmp(loc_idex.funct, "101010", 6)) {
			printf("slt in ex\n");
			if (loc_idex.rs_val < loc_idex.rt_val)
				exmem.alu_res = 1;
			else
				exmem.alu_res = 0;
			//slt
		}
		else if (!strncmp(loc_idex.funct, "100010", 6)) {
			printf("sub in ex\n");
			exmem.alu_res = loc_idex.rs_val - loc_idex.rt_val;
			//sub
		}
		else {
			printf("nop in ex\n");
		}
	}
	else {
		//printf("im in EX stage I type\n");
		//For I-type
		exmem.dst_reg_id = loc_idex.rt;
		//For J and JAL

		if (!strncmp(loc_idex.opcode, "001000", 6)) {
			//printf("add instruction : loc_idex.rt_val : 0x%08x \n", loc_idex.rt_val);
			printf("addi in ex\n");
			exmem.alu_res = loc_idex.rs_val + loc_idex.imm;
			//printf("add result exmem.alu_res : 0x%08x\n", exmem.alu_res);

			//addi
		}
		else if (!strncmp(loc_idex.opcode, "001100", 6)) {
			printf("andi in ex\n");
			exmem.alu_res = loc_idex.rs_val & (0x0000FFFF & loc_idex.imm);
			//andi
		}
		else if (!strncmp(loc_idex.opcode, "001111", 6)) {
			printf("lui in ex\n");
			exmem.alu_res = (loc_idex.imm << 16);
			//lui
		}
		else if (!strncmp(loc_idex.opcode, "100011", 6)) {
			printf("lw in ex\n");
			exmem.alu_res = ((loc_idex.rs_val + loc_idex.imm) - 0x10000000) / 4;//�����ϰ��� �ϴ� memory �ּ�/4
			//lw
			//lw $2, 1073741824($10)
		}
		else if (!strncmp(loc_idex.opcode, "001101", 6)) {
			printf("ori in ex\n");
			exmem.alu_res = loc_idex.rs_val | (0x0000FFFF & loc_idex.imm);
			//ori
		}
		else if (!strncmp(loc_idex.opcode, "101011", 6)) {
			exmem.alu_res= ((loc_idex.rs_val + loc_idex.imm) - 0x10000000) / 4;//rt�� ����Ǿ� �ִ� ���� ������ memory�� �ּҰ� ���
			printf("sw in ex\nsw gets address to store the value --> 0x%08x\n", exmem.alu_res);

			//sw
		}
		else if (!strncmp(loc_idex.opcode, "001010", 6)) {
			printf("slti in ex\n");
			if (loc_idex.rs_val < loc_idex.imm)
				exmem.alu_res = 1;
			else
				exmem.alu_res = 0;
			//slti
		}
		else {
			printf("branch? in ex\n");
		}
	}

	strncpy(exmem.opcode, loc_idex.opcode, 7);
	strncpy(exmem.funct, loc_idex.funct, 7);
	strncpy(exmem.shamt, loc_idex.shamt, 6);

	exmem.rd = loc_idex.rd;
	exmem.rs = loc_idex.rs;
	exmem.rt = loc_idex.rt;
	exmem.imm = loc_idex.imm;


	exmem.rd_val = loc_idex.rd_val;
	exmem.rs_val = loc_idex.rs_val;
	exmem.rt_val = loc_idex.rt_val;
	//�ʿ��� -> sw ��ɾ� : id���� ���� register ���� ���� WB stage���� Ȱ���ؾ���
	exmem.cont_op = loc_idex.cont_op;
}

int MEM(char* middle, int* DMem) {

	//printf("im in mem stage\n");

	/*
	MEM���� �� ��
	1. load, store ����
	2. alu_res �Ǵ� read data MEM/WB�� �Ѱ��ֱ�
		- load�� ��� data�� read data
		- ������(regWrite==1)�� ���, alu_res �Ѱ��ֱ� 
	3. reg_dst_id MEM/WB�� �Ѱ��ֱ� 
	
	*/
	

	/*
	* memory ���� �͸� .. load store .. ��
	* 
	typedef struct EX_MEM {
	char opcode[7];//opcode[6]�� null�� �ʱ�ȭ
	char funct[7];//funct[6]�� null�� �ʱ�ȭ
	char shamt[6];
	int alu_res;
	int dst_reg_id;
	int rs_val;
	int rt_val;
	int rd_val;
	int rs;
	int rt;//ex stage���� rt�� rd�� ��� real dst���� RegDst�� �Ǻ�
	int rd;
	int imm;
	CO cont_op;
	}EXMEM;

	typedef struct MEM_WB {
		char opcode[7];//opcode[6]�� null�� �ʱ�ȭ��
		char funct[7];//funct[6]�� null�� �ʱ�ȭ��
		char shamt[6];��
		int data;//reg�� ����� ��� �����, mem���� load�ؿ� ���̵� WB stage���� muxing
		-> �� if �� �ȿ��� ���������� data�� ����
		int dst_reg_id;��
		int rs_val;��
		int rt_val;��
		int rd_val;��
		int rs;��
		int rt;��
		int rd;��
		int imm;��
		CO cont_op;��
	}MEMWB;
	*/


	//load, store�� ���, alu_res�� mem �ּҰ� ������� 
	if (!strncmp(loc_exmem.opcode, "100011", 6)) {
		printf("lw in mem stage\n");
		memwb.data = DMem[loc_exmem.alu_res];
		printf("lw is loading value 0x%08x from memory address 0x%08x\n", DMem[loc_exmem.alu_res], loc_exmem.alu_res);
		printf("loaded value 0x%08x from mem\n", memwb.data);
		/*
		WB stage���� Reg[memwb.reg_dst_id]=memwb.data; ���ָ� �� 
		mem������ reg�� �����ϸ� �ȵ�,data�� �Űܳ��ٰ�, WB���� register�� �����ؾ���
		Reg[Regi(rt)] = DMem[((Reg[Regi(rs)] + bintoDeci(Imm, 1)) - 0x10000000) / 4];
		DMem�� ������ �迭(4 byte ����)�̱� ������ 0x4�� �ּҰ��� ���� �޸��� ������(1 byte ����)�� ��� �ʹٸ�,
		DMem[0x1]�� �����ؾ��Ѵ�. 
		lw
		lw $2, 1073741824($10)
		*/
	}
	else if (!strncmp(loc_exmem.opcode, "101011", 6)) {
		printf("sw in mem stage\n");
		DMem[loc_exmem.alu_res] = loc_exmem.rt_val;
		printf("sw is storing value 0x%08x in memory address 0x%08x\n", DMem[loc_exmem.alu_res],loc_exmem.alu_res);
		//DMem[(Reg[Regi(rs)] + bintoDeci(Imm, 1) - 0x10000000) / 4] = Reg[Regi(rt)];
		//sw
	}
	else if (!strncmp(loc_exmem.opcode, "000000", 6)&& !strncmp(loc_exmem.funct, "000000", 6)&& !strncmp(loc_exmem.shamt, "00000", 5)) {
		printf("nothing to do in mem stage\n");
	}
	else {
		//�� �̿� : ex stage���� ����� ����� WB stage���� reg�� �����ϴ� ��� 
		printf("store alu result in data maybe r-type inst\n");
		memwb.data = loc_exmem.alu_res;
	}

	strncpy(memwb.opcode, loc_exmem.opcode, 7);
	strncpy(memwb.funct, loc_exmem.funct, 7);
	strncpy(memwb.shamt, loc_exmem.shamt, 6);

	memwb.rd = loc_exmem.rd;
	memwb.rs = loc_exmem.rs;
	memwb.rt = loc_exmem.rt;
	memwb.imm = loc_exmem.imm;
	memwb.dst_reg_id = loc_exmem.dst_reg_id;

	memwb.rd_val = loc_exmem.rd_val;
	memwb.rs_val = loc_exmem.rs_val;
	memwb.rt_val = loc_exmem.rt_val;

	memwb.cont_op = loc_exmem.cont_op;

	//printf("im in mem stage\nglobal memwb.rd :%d\nmemwb.rs :%d\nmemwb.rt :%d\n", memwb.rd, memwb.rs, memwb.rt);

}
int WB(char* middle, int* Reg) {

	/*
	
	WB���� ����
	1. lw ó���ϱ� ��
		- MEM stage���� Reg[reg_dst_id]�� ������ϰ� 
		memwb.data�� load�ؿ� �� �����س���
	2. Register�� �� �����ؾ��ϴ� ��ɾ�� ó�����ֱ� 
		- rd : ad��d su��b an��d o��r s��lt 
		- rt : a��ddi a��ndi o��ri s��lti lu��i
	3. sw�� �Ұ� ���� ��
		- mem stage���� DMEM�� ���������� ��

	** register ����� ��, 
	writeback stage�� target register�� update�� ������ register value�� ����Ϸ�
	-> WB stage���� Reg[reg_dst_id]�� ������� �� �ϱ�

	*/


	if (!strncmp(loc_memwb.opcode, "000000", 6))
	{//R-type ->  op rs rt rd shamt funct
		if (!strncmp(loc_memwb.funct, "100000", 6)) {
			printf("add in wb\n");
			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//add
		}
		else if (!strncmp(loc_memwb.funct, "100100", 6)) {
			printf("and in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//and
		}
		else if (!strncmp(loc_memwb.funct, "100101", 6)) {
			printf("or in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//or
		}
		else if (!strncmp(loc_memwb.funct, "101010", 6)) {
			printf("slt in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//slt
		}
		else if (!strncmp(loc_memwb.funct, "100010", 6)) {
			printf("sub in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//sub
		}
		else if(!strncmp(loc_memwb.funct, "000000", 6)){
			printf("maybe nop or sll in wb stage\n");
		}
	}
	else {
		//For I-type
		//For J and JAL
		if (!strncmp(loc_memwb.opcode, "001000", 6)) {
			printf("addi in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//addi
		}
		else if (!strncmp(loc_memwb.opcode, "001100", 6)) {
			printf("andi in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//andi
		}
		else if (!strncmp(loc_memwb.opcode, "001111", 6)) {
			printf("lui in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//lui
		}
		else if (!strncmp(loc_memwb.opcode, "100011", 6)) {
			printf("lw in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//lw
		}
		else if (!strncmp(loc_memwb.opcode, "001101", 6)) {
			printf("ori in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//ori
		}
		else if (!strncmp(loc_memwb.opcode, "001010", 6)) {
			printf("slti in wb\n");

			Reg[loc_memwb.dst_reg_id] = loc_memwb.data;
			//slti
		}
	}
}



int main(int argc, char* argv[]) {

	/*argv
	0:x ,
	1:bin ���� �̸�,
	2:������ ��ɾ� ����
	3:��� ������� (reg or mem)
	4:mem�϶� �����ּ�
	5:mem�϶� ���� �޸� ����(4 byte ����)
	*/
	int cy_number = 1;
	int memstart = 1;
	int memnum = 1;
	char mr[4];
	if (argc < 4) { //bin ���� �̸�, cycle ���������� ���� ���
		cy_number = atoi(argv[2]);
	}
	else if (argc == 4 && (!strncmp(argv[3], "reg", 3))) { //?, bin, inst num, reg
		cy_number = atoi(argv[2]);
	}
	else if (argc == 6 && (!strncmp(argv[3], "mem", 3))) {//?, bin, inst num, mem, memstart, mem num
		cy_number = atoi(argv[2]);
		memstart = strtol(argv[4], NULL, 16);
		memnum = atoi(argv[5]);
	}
	else
		return 0;


	// last ���ڿ��迭�� ũ�⸦ Ȯ���ϱ� ���ؼ� ! �����Ҵ� ���Ͽ� !


	unsigned char preBuf[1];
	FILE* pre;
	pre = fopen(argv[1], "rb");
	if (pre == NULL) {
		printf("Error opening input file.\n");
		exit(1);
	}


	int loop = 0;
	while (!feof(pre)) {
		loop++;
		fread(preBuf, sizeof(char), 1, pre); //8bit�� ���� 
		//loop*8 =�������� ����
	}
	loop = loop * 9;
	char* last = (char*)malloc(sizeof(char) * loop);
	//printf("size of last : %d\n",loop);
	memset(last, 0xFF, loop);
	//last ���ڿ� ��� 0xFF�� ���� �ʱ�ȭ �Ϸ� 
	/*for(int in=0;in<loop;in++)
		printf("num : %d : %0x\n",in, last[in]);*/
	fclose(pre);


	//���� ���� ���� �б�
	unsigned char buf[1]; //char������ �޾ƿ´�. --> fread ù �Ű������� �����ͷ� ��� �ؼ�,, �̷��� �ؾߤ�����
	FILE* ptr;
	int idx = 0;
	char res[4];
	char result[9];//result -> char������ buf�� �޾ƿ� file ������
	ptr = fopen(argv[1], "rb");  // r for read, b for binary
	if (ptr == NULL) {
		printf("Error opening input file.\n");
		exit(1);
	}

	//16����(2 digits) -> 10���� ����('0'~"255") -> 10���� ����(0~255) -> 2���� ���ڿ�(0000 0000 ~ 1111 1111)
	while (!feof(ptr)) {
		fread(buf, sizeof(char), 1, ptr); // 1 byte �� �о�� 
		sprintf(res, "%d", buf[0]);
		//char buf = 8 bit�� 10������ ������ ���ڿ��� �����Ѵ�. 2�� 8���� 256 �ִ� 3�ڸ��� -> res�� ũ�Ⱑ 4�� ���� �迭
		int deci = atoi(res);
		//10�������� ������ ���ڸ� ǥ���ϴ� ���ڿ��� ������ ���ڷ� �ٲپ int integer�� ����

		Bin(deci, result);
		/*
		���� deci(8bit ������ ���� ���̹Ƿ�, 2������ �ٲپ 8bit�� ǥ�� ����)�� 2�������� ��ȯ
		result ���ڿ��� 9���� ��Ҹ� ����, result[8]�� 9��° ��Ҵ� null�� �ʱ�ȭ
		Bin�� ����� result���� 8bit¥�� 2������ ����Ǿ� �ְ� �ȴ�.
		MSB�� result[0]�� ����Ǿ�����.
		*/
		for (int b = 0; b < 8; b++) {
			//2������ �ٲ� buf �������� ����غ���
			//�������� ���� �迭�� ����, idx ������ ���� �� main�� ����, �ʱ�ȭ 
			//2���� 8bit¥�� ���ڿ��� last ���ڿ��迭�� �ָ��� ����
			//last ���ڿ����� bin����
			last[idx] = result[b];
			idx++;
		}
	}
	last[idx] = '\0';
	fclose(ptr);
	/*
	������� bin���Ͽ��� ������ ���������� ��ȯ
	*/


	//register �迭, data memory �迭, instruction memory�� last �迭 �״�� ����ϱ� 
	int Reg[33]; //register
	Reg[32] = 0; //Reg[32] : pc register, instruction �ּ� 0���� ����
	int* DMem = (int*)malloc(sizeof(int) * 262145); //16^5 bytes = 1,048,576 -> 1,048,576/4 = 262,144 + 1(����)
	memset(Reg, 0, sizeof(Reg));//0���� �ʱ�ȭ Ȯ�� �Ϸ�
	memset(DMem, 0xFF, 1048580);//0xFF�� �ʱ�ȭ Ȯ�� �Ϸ�
	char middle[33];
	//middle�� 2���� ���ڿ� ������ 32 bit�� 0 or 1 �ϳ��� char �ϳ��� �����ϴ� ��
	middle[32] = '\0';
	int pc;//last�� ������ 




	for (int out = 0; out < cy_number/*number�� input���� ���� ���� Ƚ��*/;) {
		pc = Reg[32] * 8; //���� Reg[32]�� 4�� ����Ǿ������� �ι�° ��ɾ ������� �Ҹ� = last[32]�� �о�� ��
		for (int in = 0; in < 32; in++) { //32bit�� �о middle�� ������. (middle = ��ɾ� �� ��)
			//printf("out is %d\n", out);
			middle[in] = last[pc]; //last�� bit������ ��ü ��ɾ ����Ǿ� �ְ�, pc register�� byte ������ ����Ǿ� ���� 
			pc++;
			//middle�� instruction �ϳ��� ����Ǿ� ���� 
			//in�� middle�� �ε��� 
			//���� Reg[32]�� 4�� ����Ǿ������� �ι�° ��ɾ ������� �Ҹ� = last[32]�� �о�� ��
		}
		out++;//cycle �� 

		if_f = 1;
		
		if (if_f = 1 && id_f == 0 && ex_f == 0 && mem_f == 0 && wb_f == 0) {
			//if stage�� Ȱ��ȭ
			IF(middle, Reg);
			id_f = 1;
			if (PCWrite == 1) {
				//PCWrite�� IF�� j, ID�� beq bne load use data hazard (ID stage ���� if��) 
				//PCWrite�� 1�϶��� Reg[32] generally �ϰ� 4 �����Ѵ�.
				//pc�� �������� �Ȱ��� �о cycle�� �����ϱ� 
				//Reg[32]�� �������� �ʴ� ��� 
				/*
				1. beq, bne, j -> PCWrite=0; && �˾Ƽ� �ڱ���� target address(Reg[32]��) �ʱ�ȭ ���ش�
				2. load - use data hazard -> ������ �Ȱ��� Reg[32]�� ����, cycle�� ������Ű��  -> PCWrite =0;
				*/
				Reg[32] += 4;
			}

			if (IF_IDWrite == 1)
				loc_ifid = ifid;
			continue;
		}

		if (if_f = 1 && id_f == 1 && ex_f == 0 && mem_f == 0 && wb_f == 0) {
			IF(middle, Reg);
			ID(Reg);

			ex_f = 1; 
			if (PCWrite == 1) {
				Reg[32] += 4;
			}

			if(IF_IDWrite==1)
				loc_ifid = ifid;
			loc_idex = idex;
			continue;
		}
		if (if_f = 1 && id_f == 1 && ex_f == 1 && mem_f == 0 && wb_f == 0) {
			IF(middle, Reg);
			ID(Reg);
			EX(middle, Reg);
			mem_f = 1; 
			if (PCWrite == 1) {
				Reg[32] += 4;
			}

			if (IF_IDWrite == 1)
				loc_ifid = ifid;
			loc_idex = idex;
			loc_exmem = exmem;
			continue;
		}


		if (if_f = 1 && id_f == 1 && ex_f == 1 && mem_f == 1 && wb_f == 0) {
			IF(middle, Reg);
			ID(Reg);
			EX(middle, Reg);
			MEM(middle, DMem);
			wb_f = 1;
			if (PCWrite == 1) {
				Reg[32] += 4;
			}

			if (IF_IDWrite == 1)
				loc_ifid = ifid;
			loc_idex = idex;
			loc_exmem = exmem;
			loc_memwb = memwb;
			continue;
		}
		if (if_f = 1 && id_f == 1 && ex_f == 1 && mem_f == 1 && wb_f == 1) {
			IF(middle, Reg);
			ID(Reg);
			EX(middle, Reg);
			MEM(middle, DMem);
			WB(middle, Reg);
			printf("pcwrite value %d\n", PCWrite);
			if (PCWrite == 1) {
				printf("pc= pc+4\n");
				Reg[32] += 4;
			}
			if (IF_IDWrite == 1) 
				loc_ifid = ifid;

			loc_idex = idex;
			loc_exmem = exmem;
			loc_memwb = memwb;
			continue;
		}

	}
	free(last);


	if (!strncmp(argv[3], "mem", 3)) {
		memstart -= 0x10000000;
		for (int mem = memstart; mem < memnum; mem++) {
			printf("0x%08x\n", DMem[mem]);
		}
	}
	else if (!strncmp(argv[3], "reg", 3)) {
		printf("Checksum: 0x%08x\n", checksum);
		for (int regg = 0; regg < 33; regg++) {
			if (regg == 32)
				printf("PC: 0x%08x\n", Reg[regg]-4);
			else
				printf("$%d: 0x%08x\n", regg, Reg[regg]);
		}
	}
	return 0;
}


/*
1. �μ� ���� �ޱ�
- ��ɾ� ���� --> number������ �����ؼ� out<number �� �ݺ��� �����ϱ�
2. Reg�� 0���� �ʱ�ȭ, last�� 0xFF�� �ʱ�ȭ, DMem�� 0xFF�� �ʱ�ȭ �Ϸ�
3. Reg[31]�� pc register * 8�� last�迭�� ������
		- Reg[31]�� ����Ǿ��ִ� 1�� �޸� �ּ� => 1 = 4byte
		- last[idx]�� ����Ǿ��ִ� 1�� 1bit

		so, ���� Reg[31]�� 0x4�� ����Ǿ������� �ι�° ��ɾ ������� �Ҹ� = last[32]���� 32 bit�� �о�� ��

4. printMid �����ϱ�
	- register ����
	- memory�� store, load
	- j, beq, bne jump�� ��, pc+4�� �����ΰ� �����ϱ�

5. reg, mem ��� �Լ� �����ϱ�


*/
