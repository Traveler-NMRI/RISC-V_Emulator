#include <stdio.h>
#include <stdint.h>
#include <Windows.h>
bool Instruction_Execution();
unsigned char fetch8();
void release_mem();
#define MEMSIZE 16*1024*1024
#define RISCV_SUPPORT "RV32I (MAFC will be supported in the future)"
uint32_t x_reg[32];
uint32_t pc;
void* memory;
unsigned int main(){
    wchar_t filepath[260];
    char loadpoint[10];
    uint32_t loadaddr_dec;
    printf("[RISC-V Simulator] Instruction set:%s\nMemory size:%d bytes from 0x00000000 to 0x%08X\n",RISCV_SUPPORT,MEMSIZE,MEMSIZE-1);
    printf("[Application]Executable file path:");
    if(fgetws(filepath,260,stdin)==NULL){
        printf("\n[Application]File path error!\n");
        return 1;
    }
    size_t len = wcslen(filepath);
    if(len > 0 && filepath[len -1] == L'\n'){
        filepath[len - 1]=L'\0';
    }
    HANDLE hFile=CreateFileW(
        filepath,
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if(hFile == INVALID_HANDLE_VALUE){
        printf("[Application]File open failed. Error code:%d\n",GetLastError());
        return 1;
    }
    LARGE_INTEGER fileSize;
    if(!GetFileSizeEx(hFile,&fileSize)){
        printf("[Application]File size get failed. Error code:%d\n",GetLastError());
        CloseHandle(hFile);
        return 1;
    }
    if((fileSize.QuadPart >= MEMSIZE) || (fileSize.LowPart != fileSize.QuadPart)){
        printf("[Application]File too big.\n");
        CloseHandle(hFile);
        return 1;
    }
    printf("[Application]Memory address to load into(default 0x1000):0x");
    int result=scanf("%[0-9a-fA-F]",loadpoint);
    if(result == 1){
        sscanf(loadpoint,"%x",&loadaddr_dec);
    }else if((result == EOF) || (getchar() == '\n')){
        loadaddr_dec=0x1000;
    }
    else{
        printf("[Application]Input error.\n");
        CloseHandle(hFile);
        return 1;
    }
    if(loadaddr_dec+fileSize.LowPart > MEMSIZE){
        printf("[Application]The file is too large or the load address is too high, memory will overflow after loading.\n");
        CloseHandle(hFile);
        return 1;
    }
    printf("[Application]%d byte(s) will be loaded from 0x%08X to 0x%08X\n",fileSize.LowPart,loadaddr_dec,loadaddr_dec+fileSize.LowPart);
    SIZE_T mem_size = MEMSIZE;
    memory = VirtualAlloc(
        nullptr,
        mem_size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
    if(memory == nullptr){
        printf("[Application]Memory allocate failed. Error code:%d\n",GetLastError());
        return 1;
    }
    printf("[Application]Memory allocated successfully.\n");
    memset(memory,0,MEMSIZE);
    DWORD bytesread;
    if(!ReadFile(hFile,(void*)(((uintptr_t)memory)+loadaddr_dec),fileSize.LowPart,&bytesread,nullptr)){
        printf("[Application]%d byte(s) failed to load into memory. Error code:%d\n",bytesread,GetLastError());
        release_mem();
        CloseHandle(hFile);
    }
    printf("[Application]%d byte(s) loaded successfully.\n",bytesread);
    CloseHandle(hFile);
    //printf("[Application]PC(default 0x%08X):",loadaddr_dec);
    pc=loadaddr_dec;
    printf("[Application]Start execution. PC->0x%08X\n",loadaddr_dec);
    while(pc<=MEMSIZE){
        if(!Instruction_Execution()){
            printf("[Application]Instruction execution error.\n");
            for(int i=0;i<=3;i++){
                for(int j=0;j<=7;j++){
                    printf("x%d:0x%08X ",i*8+j,x_reg[i*8+j]);
                }
                printf("\n");
            }
            printf("PC:0x%08X\n",pc);
            break;
        }
    }
    release_mem();
    return 0;
}
bool Instruction_Execution(){
    unsigned char opcode=fetch8(),op2,op3,op4;
    uint32_t uncompressed_opcode;
    uint16_t compressed_opcode;
    if((opcode & 0x03)==0x03){//RV32I RV32M RV32A RV32F 32bit
        op2=fetch8();
        op3=fetch8();
        op4=fetch8();
        uncompressed_opcode=(op4<<24)+(op3<<16)+(op2<<8)+opcode;
        if((uncompressed_opcode & 0x7F)==0x37){//lui rd,imm
            uint32_t imm=(uncompressed_opcode & 0xFFFFF000);
            uint8_t rd=(uncompressed_opcode & 0xF80) >> 7;
            if(rd!=0){
                x_reg[rd]=imm;
            }
            printf("lui x%d,0x%05X\n",rd,imm>>12);
            return true;
        }else if((uncompressed_opcode & 0x7F)==0x17){//auipc rd,imm
            uint32_t imm=(uncompressed_opcode & 0xFFFFF000);
            uint8_t rd=(uncompressed_opcode & 0xF80) >> 7;
            if(rd!=0){
                x_reg[rd]=imm+(pc-4);
            }
            printf("auipc x%d,0x%05X\n",rd,imm>>12);
            return true;
        }else if((uncompressed_opcode & 0x7F)==0x13){
            uint8_t func3=(uncompressed_opcode & 0x7000)>>12;
            uint8_t rd=(uncompressed_opcode & 0xF80) >> 7;
            uint8_t rs1=(uncompressed_opcode & 0xF8000) >> 15;
            int32_t imm = (int32_t)(((int32_t)uncompressed_opcode >> 20));
            uint8_t shamt=(uncompressed_opcode & 0x1F00000)>>20;
            if(func3==0){//addi rd,rs1,imm
                if(rd != 0){
                    x_reg[rd] = x_reg[rs1] + imm;
                    printf("addi x%d,x%d,%d\n", rd, rs1, imm);
                }               
                return true;
            }else if(func3==0x02){//slti rd,rs1,imm
                if(rd!=0){
                    x_reg[rd] = (int32_t)x_reg[rs1] < imm ? 1 : 0;
                }
                printf("slti x%d, x%d, %d\n", rd, rs1, imm);
                return true;
            }else if(func3==0x03){//sltiu rd,rs1,imm
                if(rd!=0){
                        x_reg[rd] = x_reg[rs1] < (uint32_t)imm ? 1 : 0;
                        printf("sltiu x%d,x%d,%03X\n",rd,rs1,imm);
                }
                return true;
            }else if(func3==0x04){//xori rd,rs1,imm
                if(rd!=0){
                        x_reg[rd] = x_reg[rs1] ^ imm;
                        printf("xori x%d,x%d,%03X\n",rd,rs1,imm);
                }
                return true;
            }else if(func3==0x06){//ori rd,rs1,imm
                if(rd!=0){
                        x_reg[rd] = x_reg[rs1] | imm;
                        printf("ori x%d,x%d,%03X\n",rd,rs1,imm);
                }
                return true;
            }else if(func3==0x07){//andi rd,rs1,imm
                if(rd!=0){
                        x_reg[rd] = x_reg[rs1] & imm;
                        printf("andi x%d,x%d,%03X\n",rd,rs1,imm);
                }
                return true;
            }else if(func3==0x01){//slli rd,rs1,imm
                if(rd!=0){
                        x_reg[rd] = x_reg[rs1] << shamt;
                        printf("slli x%d,x%d,%d\n",rd,rs1,shamt);
                }
                return true;
            }else if((func3==0x05) && !((uncompressed_opcode & 0x40000000)>>30)){//srli rd,rs1,imm
                if(rd!=0){
                        x_reg[rd] = x_reg[rs1] >> shamt;
                        printf("srli x%d,x%d,%d\n",rd,rs1,shamt);
                }
                return true;
            }else if((func3==0x05) && ((uncompressed_opcode & 0x40000000)>>30)){//srai rd,rs1,imm
                if(rd!=0){
                        x_reg[rd] = (int32_t)((int32_t)x_reg[rs1] >> shamt);
                        printf("srai x%d,x%d,%d\n",rd,rs1,shamt);
                }
                return true;
            }
        }else if((uncompressed_opcode & 0x7F)==0x33){
            uint8_t rd=(uncompressed_opcode & 0xF80) >> 7;
            uint8_t rs1=(uncompressed_opcode & 0xF8000) >> 15;
            uint8_t rs2=(uncompressed_opcode & 0x1F00000) >> 20;
            uint8_t func3=(uncompressed_opcode & 0x7000) >> 12;
            bool su=(uncompressed_opcode & 0x40000000) >> 30;
            if(func3==0 && !su){//add rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd]=x_reg[rs1]+x_reg[rs2];
                }
                printf("add x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==0 && su){//sub rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd]=x_reg[rs1]-x_reg[rs2];
                }
                printf("sub x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==1){//sll rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd]=x_reg[rs1] << (uint8_t)(x_reg[rs2] & 0x1F);
                }
                printf("sll x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==2){//slt rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd] = (int32_t)x_reg[rs1] < (int32_t)x_reg[rs2] ? 1 : 0;
                }
                printf("slt x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==3){//sltu rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd] = x_reg[rs1] < x_reg[rs2] ? 1 : 0;
                }
                printf("sltu x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==4){//xor rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd] = x_reg[rs1] ^ x_reg[rs2];
                }
                printf("xor x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==5 && !su){//srl rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd] = x_reg[rs1] >> (uint8_t)(x_reg[rs2] & 0x1F);
                }
                printf("srl x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==5 && su){//sra rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd] = (int32_t)((int32_t)x_reg[rs1] >> (uint8_t)(x_reg[rs2] & 0x1F));
                }
                printf("sra x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==6){//or rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd] = x_reg[rs1] | x_reg[rs2];
                }
                printf("or x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }else if(func3==7){//and rd,rs1,rs2
                if(rd!=0){
                    x_reg[rd] = x_reg[rs1] & x_reg[rs2];
                }
                printf("and x%d,x%d,x%d\n",rd,rs1,rs2);
                return true;
            }
        }
        printf("[Application]Unknown non-compressed instruction 0x%08X at 0x%08X\n",uncompressed_opcode,pc-4);
    }else{//RV32C 16bit
        op2=fetch8();
        compressed_opcode=(op2<<8)+opcode;
        printf("[Application]Unknown compressed instruction 0x%04X at 0x%08X\n",compressed_opcode,pc-2);
    }
    return false;
}
unsigned char fetch8(){
    if (pc >= MEMSIZE) {
        printf("[Application]Memory access error: PC out of bounds (0x%08X)\n", pc);
        release_mem();
        exit(1);
    }
    return *((unsigned char*)memory + pc++);
}
void release_mem(){
    if(VirtualFree(memory,0,MEM_RELEASE) == 0){
        printf("[Application]Memory release failed. Error code:%d\n",GetLastError());
        return;
    }
    printf("[Application]Memory released successfully.\n");
    return;
}