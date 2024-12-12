#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define MEMORY_SIZE 4096
#define PROGRAM_START 0x200
#define FRAMERATE 60
#define CHIP_SPEED 500
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE 10

#define first(instr)  ((instr & 0xF000) >> 12)
#define second(instr) ((instr & 0x0F00) >> 8)
#define third(instr)  ((instr & 0x00F0) >> 4)
#define fourth(instr) ((instr & 0x000F) >> 0)
#define nnn(instr)    ((instr & 0x0FFF) >> 0)
#define kk(instr)     ((instr & 0x00FF) >> 0)

// SDL2 related variables
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;
SDL_Event e;
bool game_active = true;
LARGE_INTEGER frequency, start, end, elapsed;
double ta1 , ta2;

typedef struct {
    unsigned short OC;            // Current opcode
    unsigned char V[16];          // Registers V0 to VF
    unsigned short I;             // Index register
    unsigned short PC;            // Program counter
    unsigned char memory[MEMORY_SIZE];   // Memory (4KB)
    unsigned char keys[16];       // 
    unsigned char screen[SCREEN_WIDTH * SCREEN_HEIGHT]; // Display buffer
    unsigned char SP;             // Stack pointer
    unsigned short stack[16];     // Call stack
    unsigned char draw_flag;      // Flag to redraw the screen
    unsigned char DT;    // Delay Timer Interupt
    unsigned char ST;    // Sound Timer Interupt
} chip8_t;

chip8_t chip;
unsigned char chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0
    0x20, 0x60, 0x20, 0x20, 0x70,   // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,   // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
    0xF0, 0x80, 0xF0, 0x80, 0x80    // F
};

void initChip();
bool initSDL();
void updateDisplay();
void closeSDL();
void handleInput();
double ticksToMilliseconds(LARGE_INTEGER ticks, LARGE_INTEGER frequency);
void emulateChip();
void loadProgramToChip(const char* filename);
void dump_memory(uint16_t start_address, uint16_t length);

int main(int argc, char **argv) {
    if (!initSDL()) {
        printf("SDL initialization failed!\n");
        return -1;
    }

    initChip();
    loadProgramToChip("program.rom");
    dump_memory(0x200,47);
     // Query performance frequency (ticks per second)
    QueryPerformanceFrequency(&frequency);

    // Initial time
    QueryPerformanceCounter(&start);

    while (game_active) {
        QueryPerformanceCounter(&end);

        // Calculate delta time
        elapsed.QuadPart = end.QuadPart - start.QuadPart;
        double delta = ticksToMilliseconds(elapsed, frequency);

        // Get the current time (in microseconds)
        QueryPerformanceCounter(&start) ; // Update start time for the next frame

        // Accumulate elapsed time
        ta1 += delta;
        ta2 += delta;

        if (ta2 >= (1000/FRAMERATE)) {
            handleInput();
            updateDisplay();

            if (chip.draw_flag) {
                printf("updated display! at PC: 0x%x \n", chip.PC);
                chip.draw_flag = 0;
            }
            // ta2 -= (1000/FRAMERATE);
            ta2 = 0;
        }

        if (ta1 >= (1000/CHIP_SPEED)) {
            emulateChip();
            // ta1 -= (1000/CHIP_SPEED);
            ta1 = 0;
        }


    }
    return 0;
}

void initChip() {
    chip.PC = PROGRAM_START;
    chip.OC = 0;
    chip.I = 0;
    chip.SP = 0;
    
    // Clear display
    for(int i = 0; i < (SCREEN_HEIGHT * SCREEN_WIDTH); ++i)
        chip.screen[i] = 0;

    // Clear stack
    for(int i = 0; i < 16; ++i)
        chip.stack[i] = 0;

    for(int i = 0; i < 16; ++i) {
        chip.V[i] = 0;
        chip.keys[i] =  0;
    }
        
    // Clear memory
    for(int i = 0; i < 4096; ++i)
        chip.memory[i] = 0;
        
    for(int i = 0; i < 80; ++i)
        chip.memory[i] = chip8_fontset[i]; // Place font in memory
    
    chip.DT = 0;
    chip.ST = 0;

    chip.draw_flag = true;
}

// Initialize SDL and create the window
bool initSDL() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Create window
    window = SDL_CreateWindow("Chip-8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Create texture for the display
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                SCREEN_WIDTH, SCREEN_HEIGHT);
    if (texture == NULL) {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    return true;
}
// Update the screen with the current display buffer
void updateDisplay() {
    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];

    // Map the 64x32 CHIP-8 screen into the texture buffer
    for (int y = 0; y < SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            // Assuming chip.screen contains 0 (off) and 1 (on)
            uint8_t pixel_on = chip.screen[y * SCREEN_WIDTH + x];
            pixels[y * SCREEN_WIDTH + x] = pixel_on ? 0xFFFFFFFF : 0x00000000; // White for on, Black for off
        }
    }

    // Update the SDL texture with the prepared pixel buffer
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

// Close SDL and clean up
void closeSDL() {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// Map keyboard keys to Chip-8 keys
void handleInput() {
    // Poll events
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            // Handle quit event
            return;
        }
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            bool keyState = (e.type == SDL_KEYDOWN);
            switch (e.key.keysym.sym) {
                case SDLK_1: chip.keys[0x1] = keyState; break;
                case SDLK_2: chip.keys[0x2] = keyState; break;
                case SDLK_3: chip.keys[0x3] = keyState; break;
                case SDLK_4: chip.keys[0xC] = keyState; break;
                case SDLK_q: chip.keys[0x4] = keyState; break;
                case SDLK_w: chip.keys[0x5] = keyState; break;
                case SDLK_e: chip.keys[0x6] = keyState; break;
                case SDLK_r: chip.keys[0xD] = keyState; break;
                case SDLK_a: chip.keys[0x7] = keyState; break;
                case SDLK_s: chip.keys[0x8] = keyState; break;
                case SDLK_d: chip.keys[0x9] = keyState; break;
                case SDLK_f: chip.keys[0xE] = keyState; break;
                case SDLK_z: chip.keys[0xA] = keyState; break;
                case SDLK_x: chip.keys[0x0] = keyState; break;
                case SDLK_c: chip.keys[0xB] = keyState; break;
                case SDLK_v: chip.keys[0xF] = keyState; break;
            }
        }
    }
}

// Function to convert ticks to milliseconds
double ticksToMilliseconds(LARGE_INTEGER ticks, LARGE_INTEGER frequency) {
    return (double)ticks.QuadPart * 1000.0 / (double)frequency.QuadPart;
}

void emulateChip() {
    // Opcode Fetching
    chip.OC = (chip.memory[chip.PC] << 8) | (chip.memory[chip.PC + 1]); 
    
    unsigned char Vx = (chip.OC & 0x0F00) >> 8;
    unsigned char Vy = (chip.OC & 0x00F0) >> 4;
    printf("OPCODE: 0x%x", chip.OC);
    // Opcode Decode
    switch (first(chip.OC))
    {    
    case 0x0:
        switch (kk(chip.OC)) {
            case 0xE0: // CLS
                for (int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++) {
                    chip.screen[i] = 0x0;
                }
                chip.draw_flag = 1;
                chip.PC+=2;
                break;
            case 0xEE: // RET
                chip.SP--;
                chip.PC = chip.stack[chip.SP];
                chip.PC+=2;
                break;
            case 0x0: // SYS addr
                printf("SYS addr executed, ignored for modern emulation.\n");
                printf("OPCODE: 0x%x\n", chip.OC);
                break;
        }
        break;
    case 0x1: // JP Addr to <XXX> <- <000>
        chip.PC = nnn(chip.OC);
        break;
    case 0x2: // CALL Addr to <XXX> <- <000>
        chip.stack[chip.SP] = chip.PC;
        chip.SP++;
        chip.PC = nnn(chip.OC);
        break;
    case 0x3: // SE Vx , byte 
        if (chip.V[Vx] == kk(chip.OC))  {
            printf("OPCODE: 0x%x succesfully compared: V:%x and kk:%x\n",chip.OC,chip.V[Vx], kk(chip.OC));
            chip.PC+=2;
        }
        chip.PC+=2;
        break;
    case 0x4: // SNE Vx, byte
        if(chip.V[Vx] != kk(chip.OC)) {
            chip.PC+=2; 
        }
        chip.PC+=2;
        break;
    case 0x5: // SE Vx , Vy
        if(chip.V[Vx] == chip.V[Vy]) {
            chip.PC+=2;
        }
        chip.PC+=2;
        break;
    case 0x6: // LD Vx, byte
        chip.V[Vx] = kk(chip.OC); 
        chip.PC+=2;
        break;
    case 0x7: // ADD Vx , byte
        chip.V[Vx] += kk(chip.OC); 
        chip.PC+=2;
        break;
    case 0x8:
        switch(fourth(chip.OC)) {
            case 0x0: // 0x8XY0: Sets VX to the value of VY
				chip.V[Vx] = chip.V[Vy];
				chip.PC += 2;
				break;
            case 0x1: // OR Vx, Vy
                chip.V[Vx] |= chip.V[Vy];
                chip.PC+=2;
                break;
            case 0x2: // AND Vx, Vy
                chip.V[Vx] &= chip.V[Vy];
                chip.PC+=2;
                break; 
            case 0x3: // XOR Vx, Vy
                chip.V[Vx] ^= chip.V[Vy];
                chip.PC+=2;
                break;
            case 0x4: // ADD Vx, Vy
                if (chip.V[Vy] > (0xFF - chip.V[Vx])){
                    chip.V[0xF] = 1;
                }else {
                    chip.V[0xF] = 0;
                }
                chip.V[Vx] += chip.V[Vy];
                chip.PC+=2;
                break;
            case 0x5: // SUB Vx, Vy
                if (chip.V[Vy] > chip.V[Vx]){
                    chip.V[0xF] = 0;
                }else {
                    chip.V[0xF] = 1;
                }
                chip.V[Vx] -= chip.V[Vy];
                chip.PC+=2;
                break;
            case 0x6: // SHR Vx  {, Vy}
                chip.V[0xF] = chip.V[Vx] & 0x1;
                chip.V[Vx] >>= 1;
                chip.PC+=2;
                break;
            case 0x7: // SUBN Vx, Vy
                if (chip.V[Vx] > chip.V[Vy]) {
                    chip.V[0xF] = 0;
                } else {
                    chip.V[0xF] = 1;
                }
                chip.V[Vx] = chip.V[Vy] - chip.V[Vx];
                chip.PC+=2;
                break;
            case 0xE: // SHL Vx {, Vy}
                chip.V[0xF] = chip.V[Vx] >> 7;
                chip.V[Vx] <<= 1;
                chip.PC+=2;
                break;
        }
        break;
    case 0x9: // SNE Vx, Vy
        if(chip.V[Vx] != chip.V[Vy]) {
            chip.PC+=2;
        }
        chip.PC+=2;
        break; 
    case 0xA: // LD I, addr
        chip.I = nnn(chip.OC);
        chip.PC+=2;
        break;
    case 0xB: // JP V0, addr
        chip.PC = nnn(chip.OC) + chip.V[0x0];
        break;
    case 0xC:
        chip.V[Vx] = (rand() % 0xFF) & kk(chip.OC);
        chip.PC+=2;
        break;
    case 0xD:
			unsigned short height = fourth(chip.OC);
			unsigned short pixel;

			chip.V[0xF] = 0;
			for (int yline = 0; yline < height; yline++)
			{
				pixel = chip.memory[chip.I + yline];
				for(int xline = 0; xline < 8; xline++)
				{
					if((pixel & (0x80 >> xline)) != 0)
					{
						if(chip.screen[(chip.V[Vx] + xline + ((chip.V[Vy] + yline) * SCREEN_WIDTH))] == 1)
						{
							chip.V[0xF] = 1;                                    
						}
						chip.screen[chip.V[Vx] + xline + ((chip.V[Vy] + yline) * SCREEN_WIDTH)] ^= 1;
					}
				}
			}
						
			chip.draw_flag = true;			
			chip.PC += 2;
        break;
    case 0xE: 
        switch (kk(chip.OC)) 
        {
            case 0x9E: // SKP Vx
                if (chip.keys[chip.V[Vx]] != 0) {
                    chip.PC += 2; 
                }
                chip.PC+=2;
                break;
            case 0xA1: // SKNP Vx
                if (chip.keys[chip.V[Vx]] == 0) {
                    chip.PC += 2; 
                }
                chip.PC+=2;
                break;
        }
        break;
    case 0xF:
        switch (kk(chip.OC)) 
        {
            case 0x07: // LD Vx, DT
                chip.V[Vx] = chip.DT;
                chip.PC+=2;
                break;
            case 0x0A: // LD Vx, K
                bool keyPress = false;
                for (int i = 0; i < 16; i++) {
                    if (chip.keys[i]) { // Key is pressed
                        chip.V[Vx] = i; // Store the key value in Vx
                        keyPress = true;
                    }
                }
                if (!keyPress) return;

                chip.PC+=2;
                break;
            case 0x15: // LD DT, Vx
                chip.DT = chip.V[Vx]; 
                chip.PC+=2;
                break;
            case 0x18: // LD ST, Vx
                chip.ST = chip.V[Vx];
                chip.PC+=2;
                break;
            case 0x1E: // ADD I, Vx
                if (chip.I + chip.V[Vx] > 0xFFF)
                    chip.V[0xF] = 1;
                else
                    chip.V[0xF] = 0; 
                chip.I += chip.V[Vx];
                chip.PC+=2;
                break;
            case 0x29: // LD F, Vx
                chip.I = chip.V[Vx] * 5;
                chip.PC+=2;
                break;
            case 0x33: // LD B, Vx
                unsigned char value = chip.V[Vx];

                // Calculate the BCD representation
                chip.memory[chip.I]     = value / 100;       // Hundreds digit
                chip.memory[chip.I + 1] = (value / 10) % 10; // Tens digit
                chip.memory[chip.I + 2] = (value % 100) % 10;        // Ones digit
                chip.PC+=2;
                break;
            case 0x55: // LD [I], Vx
                for (int j = 0; j <= Vx; j++) {
                    chip.memory[chip.I + j] = chip.V[j];
                }
                chip.I += Vx + 1;
                chip.PC+=2;
                break;
            case 0x65: // LD Vx, [I]
                for (int j = 0; j <= Vx; j++) {
                    chip.V[j] = chip.memory[chip.I + j];
                }
                chip.I += Vx + 1;
                chip.PC+=2;
                break;
        }
        break;
    default:
         printf("Unknown opcode: 0x%X000\n", first(chip.OC));
        break;
    }
    // printf("Process Counter: %x \n", chip.PC);
    printf("PC: %04X, I: %04X, V0: %02X, V1: %02X, V2: %02X\n", chip.PC, chip.I, chip.V[0], chip.V[1], chip.V[2]);


    // Update Timers
    if (chip.DT > 0)
        --chip.DT;

    if (chip.ST > 0) {
        if (chip.ST == 1) {
            printf("Beep");
        }
        --chip.ST;
    }
    
}
void loadProgramToChip(const char* filename) {
     FILE* file = fopen(filename, "rb"); // Open file in binary mode
    if (!file) {
        printf("Failed to open ROM file: %s\n", filename);
        exit(1);
    }

    // Seek to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    if (file_size > (MEMORY_SIZE - 0x200)) {
        printf("Error: ROM file too large to fit in memory.\n");
        fclose(file);
        exit(1);
    }

    // Read the ROM into memory starting at 0x200
    fread(&chip.memory[0x200], sizeof(uint8_t), file_size, file);

    printf("Loaded ROM: %s (%ld bytes)\n", filename, file_size);
    fclose(file);
} 

void dump_memory(uint16_t start_address, uint16_t length) {
    if (start_address >= MEMORY_SIZE) {
        printf("Error: Start address out of bounds!\n");
        return;
    }

    uint16_t end_address = start_address + length;
    if (end_address > MEMORY_SIZE) {
        end_address = MEMORY_SIZE; // Prevent overflow
    }

    printf("Memory Dump: Starting at 0x%03X, Length: %u bytes\n", start_address, length);
    for (uint16_t i = start_address; i <= end_address; i++) {
        printf("0x%03X: 0x%02X\n", i, chip.memory[i]);
    }
}