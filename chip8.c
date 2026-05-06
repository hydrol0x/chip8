// Chip-8 Emulator

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define STACK_SIZE 16

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
    uint8_t  disp[64][32];
    uint8_t  dtimer;  // delay timer
    uint8_t  stimer;  // sound timer that beeps as long as it is greater than 0
    uint8_t  reg[16]; 
} Chip_state;
Chip_state chip = {0};

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
}
