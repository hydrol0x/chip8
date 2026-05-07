// Chip-8 Emulator

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define STACK_SIZE 16
#define DISP_WIDTH 64
#define DISP_HEIGHT 32

#define TODO(text) assert(!"TODO: " #text)

typedef uint8_t  byte;

typedef struct {
    uint16_t stack[STACK_SIZE];
    int      ptr;
} Stack;

void stack_init(Stack *stack) {
    stack->ptr = -1;
}

int stack_push(Stack *stack, const uint16_t value) {
    if (stack->ptr > STACK_SIZE-1) return -1;
    stack->stack[++stack->ptr] = value;
    return stack->ptr;
}

bool stack_is_empty(const Stack *stack) {
    return stack->ptr==-1;
}

int stack_pop(Stack *stack, uint16_t *value) {
    if (stack_is_empty(stack)) return -1;
    *value = stack->stack[stack->ptr--];
    return stack->ptr;
}

typedef struct {
    size_t  pc; // program counter
    uint16_t I; // memory pointer register
    Stack    stack;
    byte     memory[0xFFF+1]; // 4096 memory locations
    bool     disp[DISP_WIDTH][DISP_HEIGHT];
    byte     dtimer;  // delay timer
    byte     stimer;  // sound timer that beeps as long as it is greater than 0
    byte     reg[16]; 
} Chip_state;
Chip_state chip = {0};

void print_display() {
    for (int i=0; i<DISP_HEIGHT; i++){
        for (int j=0; j<DISP_WIDTH; j++){
            printf("%s", chip.disp[j][i] ? "##" : "  ");
        }
        printf("\n");
    }
}

void disp_test_pattern() {
    for (int i=0; i<DISP_WIDTH; i++){
        for (int j=0; j<DISP_HEIGHT; j++){
            chip.disp[i][j] = i%2==0 ? true : false;
        }
    }
}
void load_font_set() {
    //TODO: load the font data into 0x000-0x080 from some file
    return;
}

void init_chip() {
    load_font_set();
    stack_init(&chip.stack);
    chip.pc = 0x200;
}

typedef uint16_t op_t; // each instruction is two bytes 0xNNNN


op_t fetch_instruction() {
    if (chip.pc + 2 > sizeof(chip.memory)) { 
        printf("Fatal error, attempted to fetch instruction out of bounds of memory!\n");
        exit(EXIT_FAILURE);
    }
    byte byte_a = chip.memory[chip.pc++];
    byte byte_b = chip.memory[chip.pc++];
    return (byte_a << 8) | byte_b;
}

#define first(op)  (op>>12)& 0xF
#define second(op) (op>>8) & 0xF
#define third(op)  (op>>4) & 0xF
#define fourth(op) op      & 0xF
#define NNN(op) op & 0xFFF // last 3 nibbles
#define NN(op) op & 0xFF   // last 2 nibbles (second byte)

void clear_disp() {
    memset(chip.disp, 0, sizeof(chip.disp));
}

void decode(op_t op) {
    switch (first(op)) {
        case 0x0:
            switch (NNN(op)) {
                case (0x0E0): // CLS
                    clear_disp();
                    break;
                case (0x0EE): // RET
                    TODO( "return"); 
                    break;
                default: // SYS addr -- effectively NOP for our emulator
                    return;  
            }
            break;
        case 0x1: // JMP
            chip.pc = NNN(op);  // jump to NNN, i.e set the program counter to the address
            break; 
        case 0x6: // LD (load register)
            // our instruction is 0x6Xkk, where we load KK into the Xth register
            chip.reg[second(op)] = NN(op);
            break;  
        case 0x7: // ADD
            // 0x7Xkk, add the value `kk` to register X
            chip.reg[second(op)] += NN(op);
            break;
        case 0xA: // LD I -- load NNN into I register
            chip.I = NNN(op);
            break;
        case 0xD: ;// DRW Vx, Vy, n -- display sprite at mem loc I -- I+n in (V_x,V_y)
            // 0xDxyn
            size_t x = second(op);
            size_t y = third(op);
            if (x>=sizeof(chip.reg) || y>=sizeof(chip.reg)) { 
                printf("Fatal error, out of bounds register access in draw instruction\n");
                exit(EXIT_FAILURE);
            }
            size_t n = fourth(op);
            if (chip.I+n > sizeof(chip.memory)) {
                printf("Fatal error, out of bounds memory access in draw instruction\n");
                exit(EXIT_FAILURE);
            }
            byte x_coord = chip.reg[x];
            byte y_coord = chip.reg[y];
            break;
    }
}

void run() {
    while (1) {
        op_t op = fetch_instruction();
        decode(op);
    }
}

int main () {
    init_chip();

    FILE *fp;
    fp = fopen("ibmlogo.ch8", "rb");
    if (!fp) {
        perror("Failed to open data file");
        exit(EXIT_FAILURE);
    }
    fread(&chip.memory[0x200], 1, sizeof(chip.memory)-0x200, fp); //  load data from 0x200 which is standard
    run();
    disp_test_pattern();
    print_display();
    fclose(fp);
}
