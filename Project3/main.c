#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
/*
각 단계 사이에 데이터 저장할 레지스터 파일 만들기
1. IF/ID = pc+4, instruction 32 bit(IF/ID.opcode, IF/ID.rs, IF/ID.rt) 
2. ID/EX = funct,rs,rt,rd index -> control signal
funct code로 control signal 만드는 함수 만들기?
3. EX/MEM
4. MEM/WB
*/


/*
*
* 
* check sum은 id/ex.rs_val으로 forward 이전의 값으로 하기
* --> check sum bypassing 빼고는 항상 제일 먼저 
<hazard 해야할 것>

일단 hazard없다고 가정하고 돌아가게 해놓고, 각 단계 마지막에 조건문으로 hazard 처리하기 
1. data hazard
0:no forward, 1:mem forward, 2:EX forward, 3:Bypassing

	- register에 저장한 값을 다음 inst가 재료로 가져다 써야 하는 상황

		1) EX HAZARD
		- regWrite인 명령 + 바로 다음에 해당 reg를 갖다쓴다(j는 해당 안됨): EX의 결과를 EX의 재료로 넣어준다 
		(if (ex/mem.dst == id/ex.rs or id/ex.rt //맨 처음에 조건 확인?) 
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

		2) MEM HAZARD (EX hazard가 없을 때만 발동 + 그냥 BYPASS도 이걸로?)
		regWrite인 명령 + 다음 다음에 해당 reg를 갖다쓴다 : Ex의 결과를 mem/wb.data
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

		3) Bypassing <-- mem hazard랑 구현이 똑같은거같은데(load가아니니까?)
		--> WB 함수 내에서 조건 맞으면 바로 저장..? 모루겠다 나중에 
		--> bypass 이후의 값은 checksum으로 해당
		if(mem/wb.dst==id/ex.rs or rt)
		id/ex.val=mem/wb.data



		<MEM HAZARD와 동일>
		dst==rs
		forwardA(rs)=1
		dst==rt
		forwardB(rt)=1
		

		4) Load-Use Data Hazard
			1. 
			mem -> 한칸 띄고 -> ex (mem hazard)
			--> control option 차이로 명령어 달리해서 구분 여기는 memRead, memtoreg
			이것도 ex hazard 없을때만..
			if(mem/wb.dst==id/ex.rs or rt && mem/wb.regWrite=1 && mem/wb.dst!=0
			&& !(ex/mem.dst==id/ex.rs, rt && ex/mem.co.regWrite=1 && ex.mem.dst!=0))
			id/ex.val=mem/wb.data

			2.
			mem -> 바로 ex (bubble 필요)

			if(id/ex.rt == if/id.rs or rt)

			if/ex.co.all = 0 // do nothing at ex mem wb for this time
			do not change value of IF/ID register and PC value

			IF 함수 내에서 if/id 값을 갱신할테니 IF 함수 맨 첫에 이런 상황인지 check 하는 조건식
			+ 그 바로 상위 블록 내에서 pc 바꾸는 거 조건문 안에 넣어서 이런 상황이면 pc값 갱신 하지 않기
			--> PCWrite = 0, IF_IDWrite= 0 --> 전역으로 쓰자
*/


typedef struct control_options {
	char RegDst;
	char MemtoReg;
	char RegWrite;
	char MemRead;
	char MemWrite;
	char Branch;
	char IF_Flush;
	char ForwardA; //RS 0~3까지만 표현하면됨 0:no forward, 1:mem forward, 2:EX forward
	char ForwardB; //RT 0~3까지만 표현하면됨 0:no forward, 1:mem forward, 2:EX forward
}CO;
typedef struct IF_ID {
	char opcode[7];//opcode[6]은 null로 초기화
	char funct[7];//funct[6]은 null로 초기화
	char shamt[6];
	int rs;
	int rt;
	int rd;
	int imm;
	int targ_addr;
}IFID;
typedef struct ID_EX {
	char opcode[7];//opcode[6]은 null로 초기화
	char funct[7];//funct[6]은 null로 초기화
	char shamt[6];
	int rs_val;
	int rt_val;
	int rd_val;
	int rs; 
	int rt;//ex stage에서 rt와 rd중 어떤게 real dst인지 RegDst로 판별
	int rd;
	int imm;
	CO cont_op;
}IDEX;

typedef struct EX_MEM {
	int alu_res;
	int dst_reg_id;
	CO cont_op;
}EXMEM;

typedef struct MEM_WB {
	int data;//reg에 저장될 계산 결과든, mem의 주소를 나타내는 값이든 by ALU
	int dst_reg_id;
	CO cont_op;
}MEMWB;

IFID ifid; 
IDEX idex; 
EXMEM exmem; 
MEMWB memwb;
int cycle = 0;
int given_cycle = 100;
char PCWrite=1;
char IF_IDWrite=1;

void Bin(int deci, char* result) {
	//int deci => char * result(binary string)
	//result를 메인으로부터 전달 --> 반환안해,,

	result[8] = '\0'; //마지막 요소는 null로 초기화 
	for (int i = 7; i >= 0; i--) {
		//가장 첫값이 LSB이고, LSB를 result[7]에 저장할 거다. 
		result[i] = (deci % 2) + '0';//'0'을 더해서 문자로 만들어준다. 차피 0아니면 1임
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
//해당하는 register 인덱스를 반환

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
	//어차피 16bit 상수 활용할 때만 사용할 거니 까?
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
	//32bit를 갖고 있는 문자열이 들어오는 겨 
	//-> 32bit짜리 binary code가 하라는 대로 해보자
	//일단 지금 inst mem = last 배열, data memory = DMem, register file = Reg

	//middle : 32bit string
	//Common//
	char forOp[6];
	char rs[5], rt[5], rd[5], shamt[5], target[26];
	int rsi, rti, rdi, shi, immi, tari;
	//what is opcode
	for (int op = 0; op < 6; op++)
		forOp[op] = middle[op];
	
	//opcode = "000000" =>R-type : funct를 확인
	//레지스터 값 확인: 다른 함수(Regi)로 뺐다.
	//shamt 값 확인: 다른 함수(Shift)로 뺐다.

	char forFunct[6];//For R-type
	char Imm[16]; //For I-type
	if (!strncmp(forOp, "000000", 6))
	{//R-type ->  op rs rt rd shamt funct
		for (int func = 0; func < 6; func++)//funct 확인
			forFunct[func] = middle[func + 26];

		for (rsi = 0; rsi < 5; rsi++)//rs 확인
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt 확인
			rt[rti] = middle[rti + 11];

		for (rdi = 0; rdi < 5; rdi++)//rd 확인
			rd[rdi] = middle[rdi + 16];

		for (shi = 0; shi < 5; shi++)//shamt
			shamt[shi] = middle[shi + 21];
		//shamt는 양의 정수 0~31
		//shamt는 $안붙음 --> Shift 함수로 넣어서 출력하기

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
		for (rsi = 0; rsi < 5; rsi++)//rs 확인
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt 확인
			rt[rti] = middle[rti + 11];

		for (immi = 0; immi < 16; immi++)//immediate 확인
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

			//DMem은 정수형 배열(4 byte 단위)이기 때문에 0x4의 주소값을 갖는 메모리의 데이터(1 byte 단위)를 얻고 싶다면, DMem[0x1]에 접근해야한다. 
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
	/*
	32 bit string이 들어옴 by middle

	IF 함수에서 할 일, pc+4 또는 target address 또는 jump address 주소에 있는 instruction 읽어서 
	32bit IF/ID register에 쪼개서 저장
	일단 지금 inst mem = last 배열, data memory = DMem, register file = Reg
	
	*/

	//Common//
	char forOp[6];
	char rs[5], rt[5], rd[5], shamt[5], target[26];
	int rsi, rti, rdi, shi, immi, tari;

	//what is opcode
	for (int op = 0; op < 6; op++)
		forOp[op] = middle[op];

	//opcode = "000000" =>R-type : funct를 확인
	//레지스터 값 확인: 다른 함수(Regi)로 뺐다.
	//shamt 값 확인: 다른 함수(Shift)로 뺐다.

	char forFunct[6];//For R-type
	char Imm[16]; //For I-type
	if (!strncmp(forOp, "000000", 6))
	{//R-type ->(add sub and or slt)
	//opcode(6) rs(5) rt(5) rd(5) shamt(5) funct(6)
		for (int func = 0; func < 6; func++)//funct 확인
			forFunct[func] = middle[func + 26];

		for (rsi = 0; rsi < 5; rsi++)//rs 확인
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt 확인
			rt[rti] = middle[rti + 11];

		for (rdi = 0; rdi < 5; rdi++)//rd 확인
			rd[rdi] = middle[rdi + 16];

		for (shi = 0; shi < 5; shi++)//shamt
			shamt[shi] = middle[shi + 21];
		//shamt는 양의 정수 0~31
		//shamt는 $안붙음 --> Shift 함수로 넣어서 출력하기
		strncpy(ifid.opcode, forOp, 6);
		ifid.opcode[6] = '/0';
		ifid.rd = Regi(rd);
		ifid.rs = Regi(rs);
		ifid.rt = Regi(rt);
		strncpy(ifid.funct, forFunct, 6);
		ifid.funct[6] = '/0';
		strncpy(ifid.shamt, shamt,5);
		ifid.shamt[5] = '/0';

		//ifid.shamt 이 simulator에 shift 하는 명령어 없음
		//IF/ID set
	}
	else {

		if (!strncmp(forOp, "000010", 6)) {
			//For J and JAL
			//opcode(6) target(26)
			for (tari = 0; tari < 26; tari++) //immediate == 26bit
				target[tari] = middle[tari + 6];

			strncpy(ifid.opcode, forOp, 6);
			ifid.opcode[6] = '/0';
			ifid.targ_addr = (0x0FFFFFFF & (bintoDeci(target, -1) << 2)) | ((Reg[32] + 4) & 0xF0000000);
		}
		else {
			//For I-type (addi andi ori slti) (+ lw, sw, beq, bne, lui)
			//opcode(6) rs(5) rt(5) imm(16)
			for (rsi = 0; rsi < 5; rsi++)//rs 확인
				rs[rsi] = middle[rsi + 6];

			for (rti = 0; rti < 5; rti++)//rt 확인
				rt[rti] = middle[rti + 11];

			for (immi = 0; immi < 16; immi++)//immediate 확인
				Imm[immi] = middle[immi + 16];

			strncpy(ifid.opcode, forOp, 6);
			ifid.opcode[6] = '/0';
			ifid.rs = Regi(rs);
			ifid.rt = Regi(rt);
			ifid.imm = bintoDeci(Imm, 1);
		}
	}
}
int ID(char* middle, int* Reg) {
	/*
	IF/ID register에서 
	1. opcode로 control signal 만들어서 ID/EX register에 넣기 
	2. 각 register index로 register 값(32bit) 읽어오기 
	3. rt, rd index 둘 다 ID/EX register에 저장하기 (ex에서 dst mux)
	4. beq, bne의 경우 same인지 판별하기
	5. target address 계산하기(beq, bne)
	
	*/
	//7개 문자 그대로 카피하는 이유 -> IF 함수에서 이미 마지막 요소는 null로 초기화 했기 때문에, 그대로 옮기면 null도 들어올듯
	strncpy(idex.opcode, ifid.opcode, 7);
	strncpy(idex.funct, ifid.funct, 7);
	strncpy(idex.shamt, ifid.shamt, 6);
	/*
	char RegDst; //rt=0(i) rd=1(r)
	char MemtoReg;
	char RegWrite;
	char MemRead;
	char MemWrite;
	char Branch;
	char PCWrite; 전역으로 뺌
	char IF_IDWrite; 전역으로 뺌
	char IF_Flush;
	char ForwardA; rs 0~3까지만 표현하면됨 0:no forward, 1:mem forward, 2:EX forward
	char ForwardB; rt
	*/

	if (!strncmp(ifid.opcode, "000000", 6))
	{//R-type ->  op rs rt rd shamt funct
		//id/ex register에 reg index, reg value 둘 다 넘기기
		idex.rd = ifid.rd;
		idex.rs = ifid.rs;
		idex.rt = ifid.rt;
		idex.rd_val = Reg[ifid.rd];
		idex.rs_val = Reg[ifid.rs];
		idex.rt_val = Reg[ifid.rt];

		if (!strncmp(ifid.funct, "100000", 6)) {
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
			
			Reg[32] += 4;
			return 1;
			//add
		}
		else if (!strncmp(ifid.funct, "100100", 6)) {
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

			Reg[32] += 4;
			return 1;
			//and
		}
		else if (!strncmp(ifid.funct, "100101", 6)) {
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
			
			Reg[32] += 4;
			return 1;
			//or
		}
		else if (!strncmp(ifid.funct, "101010", 6)) {
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
			
			/*if (Reg[Regi(rs)] < Reg[Regi(rt)])
				Reg[Regi(rd)] = 1;
			else
				Reg[Regi(rd)] = 0;*/
			Reg[32] += 4;
			return 1;
			//slt
		}
		else if (!strncmp(ifid.funct, "100010", 6)) {
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

			//Reg[Regi(rd)] = Reg[Regi(rs)] - Reg[Regi(rt)];
			Reg[32] += 4;
			return 1;
			//sub
		}
		else if (!strncmp(ifid.funct, "000000", 6)) {
			//printf("sll -> nop ");
			if ((ifid.rd==0) && (ifid.rs == 0) && (ifid.rt == 0) && !strncmp(ifid.shamt, "00000", 5)) {
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
				Reg[32] += 4;
				return 1;
			}
			//sll -> nop
		}
		else {
			//unknown 0xFFFFFFFF 나와도 그냥 쭉 가세요 암거도 프린트 마세요 멈추지 마시요
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
			Reg[32] += 4;
			return 1;
		}
	}
	else {
		//For I-type
		idex.imm = ifid.imm;

		//For J and JAL

		/*
		char opcode[7];//opcode[6]은 null로 초기화
		char funct[7];//funct[6]은 null로 초기화
		char shamt[6];
		int rs_val;
		int rt_val;
		int rd_val;
		int rs; 
		int rt;//ex stage에서 rt와 rd중 어떤게 real dst인지 RegDst로 판별
		int rd;
		int imm;
		CO cont_op;
		*/

		if (!strncmp(ifid.opcode, "001000", 6)) {
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
			//Reg[Regi(rt)] = Reg[Regi(rs)] + bintoDeci(Imm, 1);
			Reg[32] += 4;
			return 1;

			//addi
		}
		else if (!strncmp(ifid.opcode, "001100", 6)) {
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
			//Reg[Regi(rt)] = Reg[Regi(rs)] & (0x0000FFFF & bintoDeci(Imm, 1));
			// 왜 0x0000FFFF랑 &를 하지...
			Reg[32] += 4;
			return 1;
			//andi
		}
		else if (!strncmp(ifid.opcode, "000100", 6)) {
			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 1;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;

			//브랜치는 ID에서 같은지 확인하고, PC+4 + offset 계산해서 다음 PC 정해줘야 해 
			if (Reg[ifid.rs] == Reg[ifid.rt]) {
				char bubble[33] = "00000000000000000000000000000000\0";
				idex.cont_op.IF_Flush = 1; //if 밀어버리세용
				//misprediction in beq --> make IF nop
				//순서 꼭곡 중요 !!!!!!!
				//기존 IF 이후에, ID 실행, ID 내에서 IF 0x0으로 초기화 하는 것임 
				IF(bubble, Reg);//ifid가 nop을 실행한 것처럼 만들기 = id ex mem wb에서 아무 일도 일어나지 않는다.
				//초기화 한 후에, pc값 제대로 다시 하기 
				Reg[32] = Reg[32] + 4 + (4 * ifid.imm);
			}
			else
				Reg[32] += 4;
			//always not taken(조건이 항상 틀릴 거라고 가정)
			return 1;
			//beq, offset
		}
		else if (!strncmp(ifid.opcode, "000101", 6)) {
			idex.cont_op.RegDst = 0; //rt
			idex.cont_op.MemtoReg = 0;
			idex.cont_op.RegWrite = 0;
			idex.cont_op.MemRead = 0;
			idex.cont_op.MemWrite = 0;
			idex.cont_op.Branch = 1;
			idex.cont_op.IF_Flush = 0;
			idex.cont_op.ForwardA = 0;
			idex.cont_op.ForwardB = 0;
			PCWrite = 1;
			IF_IDWrite = 1;

			//브랜치는 ID에서 같은지 확인하고, PC+4 + offset 계산해서 다음 PC 정해줘야 해 
			if (Reg[ifid.rs] != Reg[ifid.rt]) {
				char bubble[33] = "00000000000000000000000000000000\0";
				idex.cont_op.IF_Flush = 1; //if 밀어버리세용
				//misprediction in beq --> make IF nop
				//순서 꼭곡 중요 !!!!!!!
				//기존 IF 이후에, ID 실행, ID 내에서 IF 0x0으로 초기화 하는 것임 
				IF(bubble, Reg);//ifid가 nop을 실행한 것처럼 만들기 = id ex mem wb에서 아무 일도 일어나지 않는다.
				//초기화 한 후에, pc값 제대로 다시 하기 
				Reg[32] = Reg[32] + 4 + (4 * ifid.imm);
			}
			else
				Reg[32] += 4;
			//always not taken(조건이 항상 틀릴 거라고 가정)
			return 1;
			//bne, offset
		}
		else if (!strncmp(ifid.opcode, "001111", 6)) {
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
			Reg[32] += 4;
			return 1;
			//lui
		}
		else if (!strncmp(ifid.opcode, "100011", 6)) {
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
			Reg[32] += 4;
			return 1;
		}
		else if (!strncmp(ifid.opcode, "001101", 6)) {
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
			Reg[32] += 4;
			return 1;
			//ori
		}
		else if (!strncmp(ifid.opcode, "101011", 6)) {
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
			//DMem[(Reg[Regi(rs)] + bintoDeci(Imm, 1) - 0x10000000) / 4] = Reg[Regi(rt)];
			Reg[32] += 4;
			return 1;
			//sw
		}
		else if (!strncmp(ifid.opcode, "001010", 6)) {
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
			/*if (Reg[Regi(rs)] < bintoDeci(Imm, 1))
				Reg[Regi(rt)] = 1;
			else
				Reg[Regi(rt)] = 0;*/
			Reg[32] += 4;
			return 1;
			//slti
		}
		else if (!strncmp(ifid.opcode, "000010", 6)) {
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
			Reg[32] = ifid.targ_addr;
			return 1;
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
			Reg[32] += 4;
			return 1;
		}
	}
}
int EX(char* middle, int* Reg) {
	
	if (!strncmp(forOp, "000000", 6))
	{//R-type ->  op rs rt rd shamt funct
		for (int func = 0; func < 6; func++)//funct 확인
			forFunct[func] = middle[func + 26];

		for (rsi = 0; rsi < 5; rsi++)//rs 확인
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt 확인
			rt[rti] = middle[rti + 11];

		for (rdi = 0; rdi < 5; rdi++)//rd 확인
			rd[rdi] = middle[rdi + 16];

		for (shi = 0; shi < 5; shi++)//shamt
			shamt[shi] = middle[shi + 21];
		//shamt는 양의 정수 0~31
		//shamt는 $안붙음 --> Shift 함수로 넣어서 출력하기

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
		for (rsi = 0; rsi < 5; rsi++)//rs 확인
			rs[rsi] = middle[rsi + 6];

		for (rti = 0; rti < 5; rti++)//rt 확인
			rt[rti] = middle[rti + 11];

		for (immi = 0; immi < 16; immi++)//immediate 확인
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

			//DMem은 정수형 배열(4 byte 단위)이기 때문에 0x4의 주소값을 갖는 메모리의 데이터(1 byte 단위)를 얻고 싶다면, DMem[0x1]에 접근해야한다. 
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



int main(int argc, char* argv[]) {

	/*argv
	0:x ,
	1:bin 파일 이름,
	2:실행할 명령어 개수
	3:어떤거 출력할지 (reg or mem)
	4:mem일때 시작주소
	5:mem일때 읽을 메모리 개수(4 byte 단위)
	*/
	int ins_number = 1;
	int memstart = 1;
	int memnum = 1;
	char mr[4];
	if (argc < 4) { //bin 파일 이름, instruction 개수까지만 들어온 경우
		ins_number = atoi(argv[2]);
	}
	else if (argc == 4 && (!strncmp(argv[3], "reg", 3))) { //?, bin, inst num, reg
		ins_number = atoi(argv[2]);
	}
	else if (argc == 6 && (!strncmp(argv[3], "mem", 3))) {//?, bin, inst num, mem, memstart, mem num
		ins_number = atoi(argv[2]);
		memstart = strtol(argv[4], NULL, 16);
		memnum = atoi(argv[5]);
	}
	else
		return 0;


	// last 문자열배열의 크기를 확인하기 위해서 ! 동적할당 위하여 !


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
		fread(preBuf, sizeof(char), 1, pre); //8bit씩 읽음 
		//loop*8 =이진수의 개수
	}
	loop = loop * 9;
	char* last = (char*)malloc(sizeof(char) * loop);
	//printf("size of last : %d\n",loop);
	memset(last, 0xFF, loop);
	//last 문자열 모두 0xFF로 세팅 초기화 완료 
	//-> 인자로 요구한 것 보다 더 적은 수의 명령어가 존재하면 unknown instruction인 것이니 프로그램 종료하기 
	/*for(int in=0;in<loop;in++)
		printf("num : %d : %0x\n",in, last[in]);*/
	fclose(pre);


	//본격 파일 내용 읽기
	unsigned char buf[1]; //char형으로 받아온다. --> fread 첫 매개변수를 포인터로 줘야 해서,, 이렇게 해야ㅏㄹ듯
	FILE* ptr;
	int idx = 0;
	char res[4];
	char result[9];//result -> char형으로 buf에 받아온 file 내용을
	ptr = fopen(argv[1], "rb");  // r for read, b for binary
	if (ptr == NULL) {
		printf("Error opening input file.\n");
		exit(1);
	}

	//16진수(2 digits) -> 10진수 문자('0'~"255") -> 10진수 숫자(0~255) -> 2진수 문자열(0000 0000 ~ 1111 1111)
	while (!feof(ptr)) {
		fread(buf, sizeof(char), 1, ptr); // 1 byte 씩 읽어보기 
		sprintf(res, "%d", buf[0]);
		//char buf = 8 bit를 10진수의 형태인 문자열로 저장한다. 2의 8승은 256 최대 3자리수 -> res도 크기가 4인 문자 배열
		int deci = atoi(res);
		//10진법으로 쓰여진 숫자를 표현하는 문자열을 정수형 숫자로 바꾸어서 int integer에 저장

		Bin(deci, result);
		/*
		정수 deci(8bit 값으로 만든 것이므로, 2진수로 바꾸어도 8bit면 표현 가능)를 2진법으로 변환
		result 문자열은 9개의 요소를 갖고, result[8]인 9번째 요소는 null로 초기화
		Bin의 결과로 result에는 8bit짜리 2진수가 저장되어 있게 된다.
		MSB가 result[0]에 저장되어있음.
		*/
		for (int b = 0; b < 8; b++) {
			//2진수로 바뀐 buf 한자한자 출력해보자
			//한자한자 최종 배열에 저장, idx 변수는 가장 밖 main에 선언, 초기화 
			//2진수 8bit짜리 문자열을 last 문자열배열에 주르륵 저장
			//last 문자열에는 bin파일
			last[idx] = result[b];
			idx++;
		}
	}
	last[idx] = '\0';
	fclose(ptr);
	/*
	여기까지 bin파일에서 이진수 문자형으로 변환
	*/


	//register 배열, data memory 배열, instruction memory는 last 배열 그대로 사용하기 
	int Reg[33]; //register
	Reg[32] = 0; //Reg[32] : pc register, instruction 주소 0부터 시작
	int* DMem = (int*)malloc(sizeof(int) * 262145); //16^5 bytes = 1,048,576 -> 1,048,576/4 = 262,144 + 1(여유)
	memset(Reg, 0, sizeof(Reg));//0으로 초기화 확인 완료
	memset(DMem, 0xFF, 1048580);//0xFF로 초기화 확인 완료
	/*for(int in=0;in<262145;in++)
		printf("num : %d : %0x\n",in, DMem[in]);*/
	char middle[33];
	//middle은 2진수 문자열 형태인 32 bit인 0 or 1 하나를 char 하나에 저장하는 것
	middle[32] = '\0';
	int pc;//last의 시작점 
	for (int out = 0; out < ins_number/*number은 input으로 받을 것임 횟수*/;) {
		pc = Reg[32] * 8;
		//만약 Reg[32]에 4가 저장되어있으면 두번째 명령어를 읽으라는 소리 = last[32]를 읽어야 함
		for (int in = 0; in < 32; in++) { //32bit씩 읽어서 middle에 저장함. (middle = 명령어 한 줄)
			//printf("out[%d] : %d\n", out,in);
			middle[in] = last[pc]; //last는 bit단위로 전체 명령어가 저장되어 있고, pc register는 byte 단위로 저장되어 있음 
			pc++;
			//middle은 instruction 하나씩 저장되어 있음 
			//in은 middle의 인덱스 
			//만약 Reg[32]에 4가 저장되어있으면 두번째 명령어를 읽으라는 소리 = last[32]를 읽어야 함
		}
		//printf("middle[%d] : %s\n", out,middle);
		//Reg[31] += 4;
		//--> middle, last가 이진 코드 잘 읽고 있는 것 확인 
		out++;//읽는 instruction 개수
		IF(middle, Reg);
		int check = printMid(middle, Reg, DMem, last); //한줄씩 각종 함수를 사용해서 출력, 명령어에 맞춰서 Reg[32]인 pc 증가시키기 
		if (check == 1)
			continue;
		else if (check == -1)
			break;
	}
	free(last);
	//printf("%d %s %x %d\n", ins_number, mr, memstart, memnum);


	if (!strncmp(argv[3], "mem", 3)) {
		memstart -= 0x10000000;
		for (int mem = memstart; mem < memnum; mem++) {

			printf("0x%08x\n", DMem[mem]);
		}
	}
	else if (!strncmp(argv[3], "reg", 3)) {
		for (int regg = 0; regg < 33; regg++) {
			if (regg == 32)
				printf("PC: 0x%08x\n", Reg[regg]);
			else
				printf("$%d: 0x%08x\n", regg, Reg[regg]);
		}
	}
	return 0;
}


/*
1. 인수 전달 받기
- 명령어 갯수 --> number변수에 저장해서 out<number 로 반복문 제어하기
2. Reg는 0으로 초기화, last는 0xFF로 초기화, DMem도 0xFF로 초기화 완료
3. Reg[31]인 pc register * 8이 last배열의 시작점
		- Reg[31]에 저장되어있는 1은 메모리 주소 => 1 = 4byte
		- last[idx]에 저장되어있는 1은 1bit

		so, 만약 Reg[31]에 0x4가 저장되어있으면 두번째 명령어를 읽으라는 소리 = last[32]부터 32 bit를 읽어야 함

4. printMid 구현하기
	- register 접근
	- memory에 store, load
	- j, beq, bne jump할 때, pc+4가 기준인거 생각하기

5. reg, mem 출력 함수 구현하기


*/
