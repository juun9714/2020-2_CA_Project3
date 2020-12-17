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
	char pc_register[33];
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
	char opcode[7];//opcode[6]은 null로 초기화
	char funct[7];//funct[6]은 null로 초기화
	char shamt[6];
	int alu_res;
	int dst_reg_id;
	int rs_val;
	int rt_val;
	int rd_val;
	int rs;
	int rt;//ex stage에서 rt와 rd중 어떤게 real dst인지 RegDst로 판별
	int rd;
	int imm;
	CO cont_op;
}EXMEM;

typedef struct MEM_WB {
	char opcode[7];//opcode[6]은 null로 초기화
	char funct[7];//funct[6]은 null로 초기화
	char shamt[6];
	int data;//reg에 저장될 계산 결과든, mem에서 load해온 값이든 일단 MEM/WB register는 저장하고, WB stage에서 muxing
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
	//printf("if stage\n");
	/*
	32 bit string이 들어옴 by middle

	IF 함수에서 할 일, pc+4 또는 target address 또는 jump address 주소에 있는 instruction 읽어서 
	32bit IF/ID register에 쪼개서 저장
	일단 지금 inst mem = last 배열, data memory = DMem, register file = Reg
	
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

	//opcode = "000000" =>R-type : funct를 확인
	//레지스터 값 확인: 다른 함수(Regi)로 뺐다.
	//shamt 값 확인: 다른 함수(Shift)로 뺐다.

	char forFunct[6];//For R-type
	char Imm[16]; //For I-type
	if (!strncmp(forOp, "000000", 6))
	{//R-type ->(add sub and or slt)
	//opcode(6) rs(5) rt(5) rd(5) shamt(5) funct(6)
		//printf("im in if stage's rtype\n");

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

		//global FF에 stage내의 값 저장
		//IF의 경우 가져다쓸 값이 middle로 부터 주어지는 거니까 local 값 쓸게 없음 
		strncpy(ifid.opcode, forOp, 6);
		ifid.opcode[6] = '\0';
		ifid.rd = Regi(rd);
		ifid.rs = Regi(rs);
		ifid.rt = Regi(rt);
		strncpy(ifid.funct, forFunct, 6);
		ifid.funct[6] = '\0';
		strncpy(ifid.shamt, shamt,5);
		ifid.shamt[5] = '\0';

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
			ifid.opcode[6] = '\0';
			ifid.targ_addr = (0x0FFFFFFF & (bintoDeci(target, -1) << 2)) | ((Reg[32] + 4) & 0xF0000000);
			Reg[32] = ifid.targ_addr;
			PCWrite = 0;
		}
		else {
			///printf("im in if stage's I type\n");

			//For I-type (addi andi ori slti) (+ lw, sw, beq, bne, lui)
			//opcode(6) rs(5) rt(5) imm(16)
			for (rsi = 0; rsi < 5; rsi++)//rs 확인
				rs[rsi] = middle[rsi + 6];

			for (rti = 0; rti < 5; rti++)//rt 확인
				rt[rti] = middle[rti + 11];

			for (immi = 0; immi < 16; immi++)//immediate 확인
				Imm[immi] = middle[immi + 16];
			//global로 전달
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
	IF/ID register에서 
	1. opcode로 control signal 만들어서 ID/EX register에 넣기 
	2. 각 register index로 register 값(32bit) 읽어오기 
	3. rt, rd index 둘 다 ID/EX register에 저장하기 (ex에서 dst mux)
	4. beq, bne의 경우 same인지 판별하기
	5. target address 계산하기(beq, bne)
	6. Bypassing
	7. load-use data hazard (mem, id)
		-idex.rt==ifid.rs
		-idex.rt==ifid.rt
		-idex.cont_op.memRead == 1

		exmem.cont_op.regWrite==1
		exmem.reg_dst_id != 0
		이거 두 개는 bubble 이후 hazard 어차피 다시 발생하는데, 그때 또 점검하기 때문에 여기서 굳이 안해줘도 된다. 
	8. checksum은 ID stage에서 일어난다. 
		- rs register의 value를 대상으로 check하는 것이기 때문에 !
		- EX MEM HAZARD는 EX의 ALU의 재료로 regWrite값을 미리 전달해주는 거임
		- 하지만 Bypassing은 
	
	*/
	//7개 문자 그대로 카피하는 이유 -> IF 함수에서 이미 마지막 요소는 null로 초기화 했기 때문에, 그대로 옮기면 null도 들어올듯
	//7. load use data hazard
	//id/ex FF를 조작 
	//if/id FF는 그대로
	//기존의 ID/EX rt id와 지금 IF/ID rs rt id가 같아야 함
	//checksum 각각(load use data hazard / general case(including bypassing)) 해주기
	if (loc_idex.cont_op.MemRead == 1 && ((loc_idex.rt == loc_ifid.rs) || (loc_idex.rt == loc_ifid.rt))) {
		/*
		1. ex mem wb가 작동하지 않게  
			--> idex register를 nop으로 만들기 
			--> 다음 cycle에 ex mem wb는 nop을 실행하고 
		2. IF/ID FF 그대로 냅두기 
			--> cycle이 끝났을 때, 방금 IF에서 update한 값으로 loc_ifid를 갱신하지 않기 
			--> load use data hzd에서만 IF_IDWrite =0; (전역이다)
			--> main에서 FF 갱신할 때, if문으로 조건처리 해줄까?
			--> 다음 cycle에 id는 똑같은 걸 실행해라 

		3. IF가 새로운 명령 읽지 않게 하기
			--> PC를 증가시키지 않으면 됨 
			--> PCWrite는 항상 1이고, beq bne j load use data hazard일 때만 0

		*/
		checksum = (checksum << 1 | checksum >> 31) ^ Reg[loc_ifid.rs];//ID stage 바로 종료, checksum 이 안에서 계산 해줘야함 
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
		//id/ex register에 reg index, reg value 둘 다 넘기기

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
			//unknown 0xFFFFFFFF 나와도 그냥 쭉 가세요 암거도 프린트 마세요 멈추지 마시요
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
			// 왜 0x0000FFFF랑 &를 하지...
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
			memread 활용
			idex.memread==1,  idex.rt==ifid.rs or rt
			need 2 bubble

			--> 두 경우 모두 이 if에 걸리게 만들자  
			*/
			if ((loc_exmem.cont_op.MemRead && ((loc_exmem.dst_reg_id == loc_ifid.rs) || (loc_exmem.dst_reg_id == loc_ifid.rt))) ||
				(loc_idex.cont_op.MemRead && ((loc_idex.rt == loc_ifid.rs) || (loc_idex.rt == loc_ifid.rt)))) {
				checksum = (checksum << 1 | checksum >> 31) ^ Reg[loc_ifid.rs];//ID stage 바로 종료, checksum 이 안에서 계산 해줘야함 
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
			data hazard인 경우 beq내에서 bypass why? ID 내에서 직접 bypassing한 값을 재료로 계산을 해야하기 때문
			ID stage 마지막의 branch if문에서 branch_bypass=1이면, bypass 확인 안하기
			*/
			if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rs)) {
				rs = loc_memwb.data;
				idex.rs_val = rs;
				printf("this is beq's bypass rs\nbypassed rs value of branch inst is 0x%08x\n", rs);
				checksum = (checksum << 1 | checksum >> 31) ^ rs;
				//bypass한 값을 ex로 넘겨줄지 안넘겨줄지 모르니 일단 여기서 rs값으로 checksum한다 
				//그리고 안넘겨 주고 nop을 넘겨주게 된다면, 아래 if문 안에서 알아서 nop으로 다시 세팅한다. 
				branch_bypass = 1;
			}
			else if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rt)){
				rt = loc_memwb.data;
				printf("this is beq's bypass rt\nbypassed rt value of branch inst is 0x%08x\n", rt);
				idex.rt_val = rt;
				checksum = (checksum << 1 | checksum >> 31) ^ rs;//bypass 여기서 일어나니까, checksum 이 안에서 계산 해줘야함 
				branch_bypass = 1;
			}

			//브랜치는 ID에서 같은지 확인하고, PC+4 + offset 계산해서 다음 PC 정해줘야 해 
			//always not taken 
			if (rs == rt) { //if taken
				printf("beq's condition is taken\n");

				char bubble[33] = "000000000000000000000000000000000";
				bubble[32] = '\0';
				idex.cont_op.IF_Flush = 1; //if 밀어버리세용
				/*
				misprediction in beq --> make IF nop
				순서 꼭곡 중요 !!!!!!!
				기존 IF 이후에, ID 실행, ID 내에서 IF 0x0으로 초기화 하는 것임 
				*/
				IF(bubble, Reg);
				//IF(nop) -> global FF를 all 0으로 저장 cycle 끝나고 local=global 하면 if/id의 nop이 ID에서 실행될 것임
				//초기화 한 후에, pc값 제대로 다시 하기 
				//원래 Reg[32] = Reg[32] + 4 + (4 * loc_ifid.imm) <== 이 식이었는데, +4를 빼니까 답이 맞아졌음 
				Reg[32] = Reg[32] + (4 * loc_ifid.imm);
				PCWrite = 0;
				//beq는 넘어가 봤자 ex mem wb에서 아무것도 안하는딩
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
				checksum = (checksum << 1 | checksum >> 31) ^ Reg[loc_ifid.rs];//ID stage 바로 종료, checksum 이 안에서 계산 해줘야함 
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
			data hazard인 경우 beq내에서 bypass why? ID 내에서 직접 bypassing한 값을 재료로 계산을 해야하기 때문
			ID stage 마지막의 branch if문에서 branch_bypass=1이면, bypass 확인 안하기
			*/
			if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rs)) {
				rs = loc_memwb.data;
				printf("this is bne's bypass rs\nbypassed rs value of branch inst is 0x%08x\n", rs);
				checksum = (checksum << 1 | checksum >> 31) ^ rs;//bypass 여기서 일어나니까, checksum 이 안에서 계산 해줘야함 
				branch_bypass = 1;
			}
			else if (loc_memwb.cont_op.MemRead == 1 && (loc_memwb.dst_reg_id == loc_ifid.rt)) {
				rt = loc_memwb.data;
				printf("this is bne's bypass rt\nbypassed rt value of branch inst is 0x%08x\n", rt);
				//checksum = (checksum << 1 | checksum >> 31) ^ rs;//bypass 여기서 일어나니까, checksum 이 안에서 계산 해줘야함 
				branch_bypass = 1;
			}

			//브랜치는 ID에서 같은지 확인하고, PC+4 + offset 계산해서 다음 PC 정해줘야 해 
			if (rs != rt) {
				printf("bne's condition is taken\n");
				char bubble[33] = "000000000000000000000000000000000";
				bubble[32] = '\0';
				idex.cont_op.IF_Flush = 1; //if 밀어버리세용
				IF(bubble, Reg);
				// id ex mem wb가 nop으로 초기화 됨, 근데 branch된 pc값이 반영이 안되네
				//원래 Reg[32] = Reg[32] + 4 + (4 * loc_ifid.imm) <== 이 식이었는데, +4를 빼니까 답이 맞아졌음 
				Reg[32] = Reg[32] + (4 * loc_ifid.imm);
				printf("loc_ifid.imm is 0x%08x\n", loc_ifid.imm);
				printf("branched pc value is 0x%08x\n", Reg[32]);
				PCWrite = 0;
			}
			//always not taken(조건이 항상 틀릴 거라고 가정)
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


	//6. bypassing : wb에서 저장할 값을 ID stage에서 읽어서 다음 stage로 전달해줘야할 때
	//load든, R-type instruction이든 
	//일반적인 경우 이후에 이 예외처리를 if문으로 해줘야 함 
	int bypass = 0;
	//branch에서 bypass가 일어나지 않았어야 함 
	if (branch_bypass==0 && (loc_memwb.dst_reg_id == loc_ifid.rs) && loc_memwb.cont_op.RegWrite ==1 && loc_memwb.dst_reg_id != 0) {
		idex.rs_val = loc_memwb.data;
		bypass = 1;
	}
	else if (branch_bypass == 0 && (loc_memwb.dst_reg_id == loc_ifid.rt) && loc_memwb.cont_op.RegWrite == 1 && loc_memwb.dst_reg_id != 0) {
		idex.rt_val = loc_memwb.data;
		//checksum은 rs값이랑만 계산 -> bypass가 rt에서 일어나도 bypass로 인한 checksum 계산은 안함 
	}

	//이 이후에 checksum 계산해주기
	if (bypass == 1) {
		checksum = (checksum << 1 | checksum >> 31) ^ idex.rs_val; //새로읽은 rs값(global에 저장한)에 대해서 checksum 계산
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
	1. dst 결정하기 (regDST -> MUX) o
	2. forwarding 조건 만들기 (forwardA, forwardB <- EX/MEM.rd, MEM/WB.rd, EX/MEM.regWrite, MEM/WB.regWrite, regWrite -> MUX) o
	3. 계산 하기 o
	3-0. 계산 안하는 명령 구분 (jump, nop, beq, bne -> branch 명령어는 ID 단계에서 계산한다. )
	3-1. 계산 재료 muxing 하기  -> 그냥 각 명령어 실행
		- reg + reg (R type) o
		- reg + imm (I type, lw, sw) o
	4. ex/mem에 저장해야할 것들 생각하기 
		- control options o
		- dst reg(register indexes yes, values?) o 
		- 계산 결과 (메모리 주소값이든, 순수한 계산 결과든) o alu_res

	5. Reg[32]는 누가 언제 해야하는가,,?
		- pc value는 jump와 beq, bne에 의해 조작된다, 그 이외의 경우는 항상 pc+4
			- WB까지 다 하고 나서 pc 값 변경해주기 --> common way by main 
		- jump는 IF stage에서 다음 pc값이 결정 -> jump의 경우 IF stage에서 pc값 지정하기 
		- beq bne는 ID stage에서 pc값이 결정 
			-> 만약 not taken이라면, cycle은 정상적으로 1개씩 증가
				- continue, 다른 명령어들과 동일한 위치에서 pc+4 해주기
			-> 만약 taken이라면, IF/ID stage를 nop으로 넣고, bubble 하나 들어가는거니까, 
				- pc에는 target instruction's address 넣어주고
			-> cycle 1 더 증가, 어디서 증가할지는 더 생각 (각 단계 내? or for문 안에서?)
			-> 각 단계를 하다가도 cycle이 끝나면 중간에 그만둬야 함 -> 각 단계 내에서 cycle 증가 해야할 듯
			-> generally cycle grows by 1
			-> But cycle grows by 2 when bubble is inserted (when branch is taken, 

	
	6. Load - use hazard -> MEM stage


		main의 형태
		for(cycle=0;cycle<given_cycle;){
		pc_change=0; 돌릴 때마다 초기화
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
	2. MEM HAZARD -> load use data hazard의 mem hazard도 한번에 해결 가능 (bypassing은 ID stage에서 해결)
		-MEM/WB.dst_reg_id==ID/EX.rs
		-MEM/WB.dst_reg_id==ID/EX.rt
		
	3. load-use data hazard (mem, id)
		-idex.rt==ifid.rs
		-idex.rt==ifid.rt
		-idex.cont_op.memRead
	exmem.cont_op.regWrite==1
	exmem.reg_dst_id != 0
	이거 두 개는 bubble 이후 hazard 어차피 다시 발생하는데, 그때 또 점검하기 때문에 여기서 굳이 안해줘도 된다. 


	*/

	//MEM hzd 먼저하고, EX hzd 있으면 다시 초기화 

	if (loc_memwb.cont_op.RegWrite == 1 && loc_memwb.dst_reg_id != 0 && (loc_memwb.dst_reg_id == loc_idex.rs)
		&& !(loc_exmem.cont_op.RegWrite == 1 && loc_exmem.dst_reg_id != 0 && (loc_exmem.dst_reg_id == loc_idex.rs))) {
		//MEM-Hzd of rs without EX-Hzd of rs -> forwardA = 1
		idex.cont_op.ForwardA = 1;
		loc_idex.rs_val = loc_memwb.data;
		//지금 ex stage에서 rs_val을 당장 계산의 재료로 쓰는 거니까 loc_idex.rs_val에 바뀐 값을 저장해줘야함
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
			exmem.alu_res = ((loc_idex.rs_val + loc_idex.imm) - 0x10000000) / 4;//접근하고자 하는 memory 주소/4
			//lw
			//lw $2, 1073741824($10)
		}
		else if (!strncmp(loc_idex.opcode, "001101", 6)) {
			printf("ori in ex\n");
			exmem.alu_res = loc_idex.rs_val | (0x0000FFFF & loc_idex.imm);
			//ori
		}
		else if (!strncmp(loc_idex.opcode, "101011", 6)) {
			exmem.alu_res= ((loc_idex.rs_val + loc_idex.imm) - 0x10000000) / 4;//rt에 저장되어 있던 값을 저장할 memory의 주소값 계산
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
	//필요함 -> sw 명령어 : id에서 읽은 register 내의 값을 WB stage에서 활용해야함
	exmem.cont_op = loc_idex.cont_op;
}

int MEM(char* middle, int* DMem) {

	//printf("im in mem stage\n");

	/*
	MEM에서 할 일
	1. load, store 구현
	2. alu_res 또는 read data MEM/WB로 넘겨주기
		- load의 경우 data에 read data
		- 나머지(regWrite==1)의 경우, alu_res 넘겨주기 
	3. reg_dst_id MEM/WB로 넘겨주기 
	
	*/
	

	/*
	* memory 쓰는 것만 .. load store .. 끝
	* 
	typedef struct EX_MEM {
	char opcode[7];//opcode[6]은 null로 초기화
	char funct[7];//funct[6]은 null로 초기화
	char shamt[6];
	int alu_res;
	int dst_reg_id;
	int rs_val;
	int rt_val;
	int rd_val;
	int rs;
	int rt;//ex stage에서 rt와 rd중 어떤게 real dst인지 RegDst로 판별
	int rd;
	int imm;
	CO cont_op;
	}EXMEM;

	typedef struct MEM_WB {
		char opcode[7];//opcode[6]은 null로 초기화ㅇ
		char funct[7];//funct[6]은 null로 초기화ㅇ
		char shamt[6];ㅇ
		int data;//reg에 저장될 계산 결과든, mem에서 load해온 값이든 WB stage에서 muxing
		-> 각 if 문 안에서 개별적으로 data에 저장
		int dst_reg_id;ㅇ
		int rs_val;ㅇ
		int rt_val;ㅇ
		int rd_val;ㅇ
		int rs;ㅇ
		int rt;ㅇ
		int rd;ㅇ
		int imm;ㅇ
		CO cont_op;ㅇ
	}MEMWB;
	*/


	//load, store의 경우, alu_res에 mem 주소가 들어있음 
	if (!strncmp(loc_exmem.opcode, "100011", 6)) {
		printf("lw in mem stage\n");
		memwb.data = DMem[loc_exmem.alu_res];
		printf("lw is loading value 0x%08x from memory address 0x%08x\n", DMem[loc_exmem.alu_res], loc_exmem.alu_res);
		printf("loaded value 0x%08x from mem\n", memwb.data);
		/*
		WB stage에서 Reg[memwb.reg_dst_id]=memwb.data; 해주면 됨 
		mem에서도 reg에 저장하면 안됨,data로 옮겨놨다가, WB에서 register에 저장해야함
		Reg[Regi(rt)] = DMem[((Reg[Regi(rs)] + bintoDeci(Imm, 1)) - 0x10000000) / 4];
		DMem은 정수형 배열(4 byte 단위)이기 때문에 0x4의 주소값을 갖는 메모리의 데이터(1 byte 단위)를 얻고 싶다면,
		DMem[0x1]에 접근해야한다. 
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
		//그 이외 : ex stage에서 계산한 결과를 WB stage에서 reg에 저장하는 경우 
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
	
	WB에서 할일
	1. lw 처리하기 ㅇ
		- MEM stage에서 Reg[reg_dst_id]에 저장안하고 
		memwb.data에 load해온 값 저장해놨음
	2. Register에 값 저장해야하는 명령어들 처리해주기 
		- rd : adㅇd suㅇb anㅇd oㅇr sㅇlt 
		- rt : aㅇddi aㅇndi oㅇri sㅇlti luㅇi
	3. sw는 할거 없을 껄
		- mem stage에서 DMEM에 저장해줬음 끝

	** register 출력할 때, 
	writeback stage가 target register를 update한 이후의 register value를 출력하랭
	-> WB stage에서 Reg[reg_dst_id]에 저장까지 다 하기

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
	1:bin 파일 이름,
	2:실행할 명령어 개수
	3:어떤거 출력할지 (reg or mem)
	4:mem일때 시작주소
	5:mem일때 읽을 메모리 개수(4 byte 단위)
	*/
	int cy_number = 1;
	int memstart = 1;
	int memnum = 1;
	char mr[4];
	if (argc < 4) { //bin 파일 이름, cycle 개수까지만 들어온 경우
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
	char middle[33];
	//middle은 2진수 문자열 형태인 32 bit인 0 or 1 하나를 char 하나에 저장하는 것
	middle[32] = '\0';
	int pc;//last의 시작점 




	for (int out = 0; out < cy_number/*number은 input으로 받을 것임 횟수*/;) {
		pc = Reg[32] * 8; //만약 Reg[32]에 4가 저장되어있으면 두번째 명령어를 읽으라는 소리 = last[32]를 읽어야 함
		for (int in = 0; in < 32; in++) { //32bit씩 읽어서 middle에 저장함. (middle = 명령어 한 줄)
			//printf("out is %d\n", out);
			middle[in] = last[pc]; //last는 bit단위로 전체 명령어가 저장되어 있고, pc register는 byte 단위로 저장되어 있음 
			pc++;
			//middle은 instruction 하나씩 저장되어 있음 
			//in은 middle의 인덱스 
			//만약 Reg[32]에 4가 저장되어있으면 두번째 명령어를 읽으라는 소리 = last[32]를 읽어야 함
		}
		out++;//cycle 수 

		if_f = 1;
		
		if (if_f = 1 && id_f == 0 && ex_f == 0 && mem_f == 0 && wb_f == 0) {
			//if stage만 활성화
			IF(middle, Reg);
			id_f = 1;
			if (PCWrite == 1) {
				//PCWrite는 IF의 j, ID의 beq bne load use data hazard (ID stage 내의 if문) 
				//PCWrite가 1일때만 Reg[32] generally 하게 4 증가한다.
				//pc값 기존껄로 똑같이 읽어도 cycle은 증가하기 
				//Reg[32]를 증가하지 않는 경우 
				/*
				1. beq, bne, j -> PCWrite=0; && 알아서 자기들이 target address(Reg[32]값) 초기화 해준다
				2. load - use data hazard -> 이전과 똑같은 Reg[32]를 쓰되, cycle은 증가시키기  -> PCWrite =0;
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
