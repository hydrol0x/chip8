// Chip-8 Emulator

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define STACK_SIZE 16
#define DISP_WIDTH 64
#define DISP_HEIGHT 32

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
    uint8_t  memory[0xFFF+1]; // 4096 memory locations
    bool     disp[DISP_WIDTH][DISP_HEIGHT];
    uint8_t  dtimer;  // delay timer
    uint8_t  stimer;  // sound timer that beeps as long as it is greater than 0
    uint8_t  reg[16]; 
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

int main () {
    FILE *fp;
    
    fp = fopen("ibmlogo.ch8", "rb");
    if (!fp) {
        perror("Failed to read in file");
        exit(EXIT_FAILURE);
    }
    unsigned char buf[2]; 
    int i = 1;
    while (fread(buf, 1, 2, fp)==2) {
        fprintf(stdout, "%02x%02x   ", buf[0], buf[1]);
        if (i%8 == 0) printf("\n");
        i++;
    };
    printf("\n");
    disp_test_pattern();
    print_display();
    fclose(fp);
}
