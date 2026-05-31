// Chip-8 Emulator

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include <ncurses.h>
#include <locale.h>

#define STACK_SIZE 16
#define DISP_WIDTH 64
#define DISP_HEIGHT 32
#define FONT_SIZE   0x80 
#define KEY_HOLD_DELAY_MS 50

#define NC_DBG(text) mvprintw(DISP_HEIGHT+1,0,"Debug: " #text);

#define TODO(text) assert(!"TODO: " #text)

const unsigned char fontset[FONT_SIZE] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0,		// 0
	0x20, 0x60, 0x20, 0x20, 0x70,		// 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0,		// 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0,		// 3
	0x90, 0x90, 0xF0, 0x10, 0x10,		// 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0,		// 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0,		// 6
	0xF0, 0x10, 0x20, 0x40, 0x40,		// 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0,		// 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0,		// 9
	0xF0, 0x90, 0xF0, 0x90, 0x90,		// A
	0xE0, 0x90, 0xE0, 0x90, 0xE0,		// B
	0xF0, 0x80, 0x80, 0x80, 0xF0,		// C
	0xE0, 0x90, 0x90, 0x90, 0xE0,		// D
	0xF0, 0x80, 0xF0, 0x80, 0xF0,		// E
	0xF0, 0x80, 0xF0, 0x80, 0x80		// F
};

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
    uint16_t pc; // program counter
    uint16_t I; // memory pointer register
    Stack    stack;
    byte     memory[0xFFF+1]; // 4096 memory locations
    bool     disp[DISP_HEIGHT][DISP_WIDTH];
    byte     dtimer;  // delay timer
    byte     stimer;  // sound timer that beeps as long as it is greater than 0
    byte     reg[16]; 
    int      cur_key;
    time_t   last_kp_time; 
} Chip_state;
Chip_state chip = {0};

bool error = false;

void ncurses_display_dsp() {
    for (int y=0; y<DISP_HEIGHT; y+=1){
        for (int x=0; x<DISP_WIDTH; x+=1){
            //mvprintw(y,x,"%s", chip.disp[y][x] ? "" : " ");
            move(y,x);
            if (chip.disp[y][x]) { addstr("█"); }
            else { addstr(" "); }
        }
    }
}

void ncurses_display_mem() {
    mvprintw(DISP_HEIGHT+2, 0, "memory: [");
    for (int i=0; i<20; i+=2) {
        uint16_t instruction = (chip.memory[chip.pc + i] << 8) | chip.memory[chip.pc + i + 1];
        printw("%04x,", instruction);
    }
    printw("]");
}

void ncurses_display_pc() {
    move(DISP_HEIGHT+3, 0); clrtoeol();
    uint16_t instruction = (chip.memory[chip.pc] << 8) | chip.memory[chip.pc + 1];
    mvprintw(DISP_HEIGHT+3, 0, "pc: %hu\t instruction: %04x", chip.pc, instruction);
}

void ncurses_display_keypress() {
    move(DISP_HEIGHT+4, 0); clrtoeol();
    mvprintw(DISP_HEIGHT+4, 0, "current key: %01x", chip.cur_key!=-1 ? chip.cur_key : 9);
    //if (chip.cur_key
}

void set_test_pattern() {
    for (int y=0; y<DISP_HEIGHT ; y++){
        for (int x=0; x<DISP_WIDTH; x++){
            chip.disp[y][x] = x%2==0 ? true : false;
        }
    }
}
void load_font_set() {
    memcpy(chip.memory, fontset, sizeof(fontset));
    return;
}

void init_chip() {
    srand(time(NULL));
    load_font_set();
    stack_init(&chip.stack);
    chip.pc = 0x200;
    chip.cur_key = -1;
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

#define first(op)  ((op>>12)& 0xF)
#define second(op) ((op>>8) & 0xF)
#define third(op)  ((op>>4) & 0xF)
#define fourth(op) (op      & 0xF)
#define NNN(op) (op & 0xFFF) // last 3 nibbles
#define NN(op) (op & 0xFF)   // last 2 nibbles (second byte)

void clear_disp() {
    memset(chip.disp, 0, sizeof(chip.disp));
}

void display(byte x_coord, byte y_coord, size_t n) {
    for (int y_off=0; y_off<n; y_off++) {
        int dy = y_coord + y_off;
        if (dy>=DISP_HEIGHT) break;
        byte sprite_slice = chip.memory[chip.I + y_off];
        for (int x_off = 0; x_off < 8; x_off++ ){
            int dx = x_coord + x_off;
            if (dx>=DISP_WIDTH) break;

            int current_bit = (sprite_slice>>(7-x_off)) & 0x1;

            if (!current_bit) { continue; } // 0 bit doesn't change anything 

            if (chip.disp[dy][dx]) { // 1 ^ 1 = 0 , so this dsp bit turns off hence set flag
                chip.reg[0xF] = 1;
            }
            chip.disp[dy][dx]^=current_bit;
        } 
    }
}

bool key_press(byte key_value) {
    return chip.cur_key == key_value;
}

// void set_keypress(int key_value) {
//     time_t now = time(NULL);
// }

int map_key_press(int kb_key) {
    switch (kb_key) {
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 0xC;
        case 'q':
            return 4;
        case 'w':
            return 5;
        case 'e':
            return 6;
        case 'r':
            return 0xD;
        case 'a':
            return 7;
        case 's':
            return 8;
        case 'd':
            return 9;
        case 'f':
            return 0xE;
        case 'z':
            return 0xA;
        case 'x':
            return 0;
        case 'c':
            return 0xB;
        case 'v':
            return 0xF;
        default:
            return -1;
    }
}


typedef enum {
    SUCCESS=1,
    STACK_UNDERFLOW,
    DRAW_OOB_MEM,
    DRAW_OOB_REG,
    ILLEGAL_INS,
} DecodeErr;

DecodeErr decode(op_t op) {
    byte x = second(op); // note each of these is actually a nibble not a byte
    byte y = third(op);
    byte vx = chip.reg[x];
    byte vy = chip.reg[y];
    switch (first(op)) {
        case 0x0:
            switch (NNN(op)) {
                case (0x0E0): // CLS
                    NC_DBG("Clearing screen");
                    clear_disp();
                    break;
                case (0x0EE):; // RET
                    if (stack_pop(&chip.stack, &chip.pc) < 0) { return STACK_UNDERFLOW; };
                    break;
                default: // SYS addr -- effectively NOP for our emulator
                    return SUCCESS;  
            }
            break;
        case 0x1: // JMP
            NC_DBG("Jump");
            chip.pc = NNN(op);  // jump to NNN, i.e set the program counter to the address
            break; 
        case 0x2:
            NC_DBG("Call");
            stack_push(&chip.stack, chip.pc);
            chip.pc = NNN(op);
            break;
        case 0x3:
            NC_DBG("Skip Eq");
            if (vx == NN(op)) { chip.pc += 2; }
            break;
        case 0x4:
            NC_DBG("Skip Neq");
            if (vx != NN(op)) { chip.pc += 2; }
            break;
        case 0x5:
            NC_DBG("Skip Neq");
            if (vx == vy) { chip.pc += 2; }
            break;
        case 0x6: // LD (load register)
            // our instruction is 0x6Xkk, where we load KK into the Xth register
            NC_DBG("Load");
            chip.reg[x] = NN(op);
            break;  
        case 0x7: // ADD
            // 0x7Xkk, add the value `kk` to register X
            NC_DBG("Add to Vx");
            chip.reg[x] += NN(op);
            break;
        case 0x8:;
            switch (fourth(op)) {
                case 0:
                    NC_DBG("SET Vx=Vy");
                    chip.reg[x]=vy;
                    break;
                case 1:
                    NC_DBG("OR");
                    chip.reg[x] |= vy;
                    break;
                case 2:
                    NC_DBG("AND");
                    chip.reg[x] &= vy;
                    break;
                case 3:
                    NC_DBG("XOR");
                    chip.reg[x] ^= vy;
                    break;
                case 4:;
                    NC_DBG("Sum");
                    uint sum = vx+vy;
                    if (sum > 255) { chip.reg[0xF] = 1; } // set carry if exceed byte size 
                    else {chip.reg[0xf] = 0;}

                    chip.reg[x] = sum; // note this will be truncated
                    break;
                case 5: 
                    NC_DBG("Subtract Vx-Vy");
                    uint sub = vx-vy;
                    if (vx >= vy) { chip.reg[0xF] = 1; } // set borrow bit, i.e we *didn't* borror b/c vx was greater
                    else { chip.reg[0xf] = 0; }

                    chip.reg[x] = sub;
                    break;
                case 6:
                    NC_DBG("Shift Right");
                    if (vx & 0x80) { chip.reg[0xf] = 1; } // MSB is 1
                    else { chip.reg[0xf] = 0; } 
                    chip.reg[x] >>= 1;
                    break;
                case 7:
                    NC_DBG("Subtract Vy-Vx");
                    sub = vy-vx;
                    if (vy >= vx) { chip.reg[0xF] = 1; } 
                    else { chip.reg[0xf] = 0; }
                    chip.reg[x] = sub;
                    break;
                case 0xE:
                    NC_DBG("Shift Left");
                    if (vx & 0x80) { chip.reg[0xf] = 1; } // MSB is 1
                    else { chip.reg[0xf] = 0; } 
                    chip.reg[x] <<= 1;
                    break;
            }
            break;
        case 0x9:
            NC_DBG("Skip Vx!=Vy");
            if (vx != vy) { chip.pc += 2; }
            break;
        case 0xA: // LD I -- load NNN into I register
            NC_DBG("Load I");
            chip.I = NNN(op);
            break;
        case 0xB:
            NC_DBG("Jump with offset");
            chip.pc = NNN(op)+chip.reg[0];
            break;
        case 0xC:
            NC_DBG("Rand");
            int r = rand(); 
            chip.reg[x] = r & NN(op); // r is int but it should be truncated ..
            break;
        case 0xD: ;// DRW Vx, Vy, n -- display sprite at mem loc I -- I+n in (V_x,V_y)
            // 0xDxyn
            NC_DBG("Draw");
            if (x>=sizeof(chip.reg) || y>=sizeof(chip.reg)) { 
                return DRAW_OOB_REG;
            }
            byte n = fourth(op);
            if (chip.I+n > sizeof(chip.memory)) {
                return DRAW_OOB_MEM;
            }
            byte x_coord = vx;
            byte y_coord = vy;
            chip.reg[0xF] = 0; // set register F (index F-1) to 0
            display(x_coord%DISP_WIDTH, y_coord%DISP_HEIGHT, n);
            break;
        case 0xE:
            switch(NN(op)) {
                case 0x9E:
                    NC_DBG("KEY PRESS");
                    if (key_press(vx)) { chip.pc+=2; };
                    break;
                case 0xA1:
                    NC_DBG("KEY NOT PRESS");
                    if (!key_press(vx)) { chip.pc+=2; };
                    break;
            }
            break;
        case 0xF:
            switch (NN(op)) {
                case 0x07:
                    NC_DBG("Set Vx to dtimer");
                    chip.reg[x]=chip.dtimer;
                    break;
                case 0x0A:
                    NC_DBG("Wait for keypress");
                    if (chip.cur_key==-1) {
                        chip.pc -= 2; // rewind the execution
                    } else {
                        chip.reg[x] = chip.cur_key;
                    }
                    break;
                case 0x15:
                    NC_DBG("Set dtimer to Vx");
                    chip.dtimer = vx;
                    break;
                case 0x18:
                    NC_DBG("Set stimer to Vx");
                    chip.stimer = vx;
                    break;
                case 0x1E:
                    NC_DBG("Set I += Vx");
                    chip.I += vx;
                    break;
                case 0x29:
                    NC_DBG("Load sprite for number");
                    chip.I = vx * 5; // each font char is 5 bytes, starting at mem loc 0 
                    break;
                case 0x33:
                    NC_DBG("Load BCD rep");
                    // Vx is max 255
                    chip.memory[chip.I] = vx / 100;
                    chip.memory[chip.I+1] = (vx/10) % 10;
                    chip.memory[chip.I+2] = vx % 10;
                    break;
                case 0x55:
                    NC_DBG("Store reg 0-Vx in Mem[I]-Mem[I+x]");
                    for (int i=0; i<=x; i++){
                        chip.memory[chip.I + i] = chip.reg[i];
                    }
                    chip.I += x + 1;
                    break;
                case 0x65:
                    NC_DBG("Fill reg 0-Vx with values from Mem[I]-Mem[I+x]");
                    for (int i=0; i<=x; i++){
                        chip.reg[i] = chip.memory[chip.I+i];
                    }
                    chip.I += x + 1;
                    break;

            }
            break;
        default:
            return ILLEGAL_INS;
    }
    return SUCCESS;
}


void run() {
    setlocale(LC_ALL, "");

    cbreak();
    noecho();
    initscr();
    keypad(stdscr, TRUE);
    timeout(15);

    while (1) {
        int key = getch();
        if (key!=ERR) {
            chip.cur_key = map_key_press(key);
        } else {
            chip.cur_key = -1;
        }
        //chip.cur_key = 
        
        ncurses_display_dsp();
        ncurses_display_mem();
        ncurses_display_pc();
        ncurses_display_keypress();

        op_t op = fetch_instruction();
        mvprintw(DISP_HEIGHT,0,"decoding: %04x", op);
        decode(op);

     //   usleep(200);
        //while (getch()!='n') {}

        if (chip.dtimer > 0) { chip.dtimer--; }
        if (chip.stimer>0)   { chip.stimer--; } 

        refresh();
        move(DISP_HEIGHT+1, 0); clrtoeol(); // clear the debug message
        usleep(1000);
    }
}

int main (int argc, char **argv) {
    if (argc!=2){
        printf("Usage: %s <rom file>\n", argv[0]);
        exit(1);
    }
    init_chip();

    FILE *fp;
    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Chip 8 Error: Failed to open data file");
        exit(EXIT_FAILURE);
    }
    fread(&chip.memory[0x200], 1, sizeof(chip.memory)-0x200, fp); //  load data from 0x200 which is standard
    run();
    //disp_test_pattern();
    fclose(fp);
}
