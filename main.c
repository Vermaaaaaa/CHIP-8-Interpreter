#define SDL_MAIN_HANDLED

#include <stdio.h>
#include "D:\SDL2-2.28.4\include\SDL.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <windows.h>
#include <string.h>
#include <time.h>




size_t strlcpy(char *dest, const char *src, size_t size) {
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    while (src[i] != '\0') {
        ++i;
    }
    return i;
}


//SDL object
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_type;
//Create a struct that holds our pointer to a window (More OOP approach)

typedef enum{
    COSMAC,
    AMIGA, 
}emu_type;


//Config Object
typedef struct {
    emu_type choice;
    uint32_t bg_colour;
    uint32_t fg_colour;
    int res_x;
    int res_y;
    char rom_name[50];
    int insts_per_sec;
} config_type;


//Enum for emulation state
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
} emu_state;

typedef union{
    uint16_t full_op;
    uint8_t data[2];
} opcode_t;

typedef struct{
    opcode_t opcode;
    uint16_t NNN;   //Constants in instruction set, declaring like this would decrease space complexity
    uint8_t NN;     
    uint8_t N;      
    uint8_t X;      
    uint8_t Y;      
} instr_type;

//Chip 8 object
typedef struct{
    emu_state state;
    uint8_t ram[4096]; //Ram for the chip 8
    bool display[64*32]; //Decided to have seperate arrays for display instead of a pointer to ram. May change if I move onto super chip
    uint16_t stack[48]; 
    uint16_t *stkptr;
    uint8_t V[16]; //Registers from V0-Vf
    uint16_t I; //Index Register  
    uint16_t pc;
    uint8_t delay_timer;
    uint8_t sound_timer; //60Hz timers in chip 8
    bool keypad[16]; //Check if keypad is in off or on state
    const char *rom_name; // Get a command line dir for rom to load into ram
    instr_type inst;
    bool draw;
} chip8_type;



//Initialiser for sdl object 
int init_sdl(sdl_type *sdl, config_type *config){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0){
        SDL_Log("SDL couldn't Initialise %s\n", SDL_GetError());
        return 0; // Returns 0 to show that SDL wasn't initalised
    }

    sdl-> window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config->res_x * 20,
        config->res_y * 20,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE  
    ); //Creates Window using our pointer with said Parameters 

    if(!sdl->window){SDL_Log("Could not create Window %s\n", SDL_GetError()); return 0;} //If window can't open throw error

    sdl->renderer = SDL_CreateRenderer(
        sdl->window,
        -1,
        SDL_RENDERER_ACCELERATED
    ); //Creates Renderer using our pointer with said Parameters 

    if(!sdl->renderer){SDL_Log("Could not create Renderer %s\n", SDL_GetError()); return 0;} //If renderer can't initialise throw error

    return 1; // Success
}

void fileparser(char *line , char* key, char* value){
   // Make copies of the original strings for modification
    char key_copy[50];
    char value_copy[50];

    char* line_saveptr = line;
    char* keyc_saveptr = key_copy;
    char* valc_saveptr = value_copy;


    char *token = strtok_r(line, "=", &line_saveptr);
    if (token != NULL) {
        strlcpy(key_copy, token, sizeof(key_copy));
        token = strtok_r(NULL, "(", &line_saveptr);
        if (token != NULL) {
            strlcpy(value_copy, token, sizeof(value_copy));
            // Trim whitespaces, newline, tab, and return carriage
            strlcpy(key, strtok_r(key_copy, " \t\n\r", &keyc_saveptr), sizeof(key_copy));
            strlcpy(value, strtok_r(value_copy, " \t\n\r", &valc_saveptr), sizeof(value_copy));
        }
    }
}



void read_in_config(config_type *config){

    FILE *config_f = fopen("config.txt", "r"); 

    if(config_f == NULL){SDL_Log("Config File does not contain anything");}

    char line[100];

    char key[50];
    char value[50];

    while(fgets(line, sizeof(line), config_f)){
        fileparser(line, key, value);
        if(!strncmp(key, "bg_colour", 9)){config->bg_colour = (uint32_t)strtoul(value, NULL, 0);}
        else if(!strncmp(key, "fg_colour", 9)){config->fg_colour = (uint32_t)strtoul(value, NULL, 0);}
        else if(!strncmp(key, "res_x", 5)){config->res_x = atoi(value);}
        else if(!strncmp(key, "res_y", 5)){config->res_y = atoi(value);}
        else if(!strncmp(key, "rom_name", 8)){strlcpy(config->rom_name, value, sizeof(value));}
        else if(!strncmp(key, "emulator_type", 14)){config->choice = atoi(value);}
        else if(!strncmp(key, "insts_per_second", 17)){config->insts_per_sec = atoi(value);}
        else{SDL_Log("Please Check config and readme files for correct configurations");}
        }
    
    

    fclose(config_f);
}

void end(sdl_type *sdl){
    SDL_DestroyRenderer(sdl->renderer); // Destroys the Renderer
    SDL_DestroyWindow(sdl->window); // Destroys the window
    sdl->window = NULL;
    SDL_Quit(); //Shutsdown SDL
}

int init_chip8(chip8_type *chip8, config_type *config){
    const uint32_t entry = 0x200; //Entry point for roms to be loaded into memory
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    }; //Fonts used by the CHIP 8


    memset(chip8, 0, sizeof(chip8_type));

    //Load Font
    memcpy(chip8->ram, font, sizeof(font));


    //Open/Load ROM
    FILE *rom = fopen(config->rom_name, "rb"); // Set File object to read bytes as the file is raw
    if(!rom){SDL_Log("ROM file %s is invalid or does not exist\n" ,config->rom_name); return 0;} //Return error if file cannot be opened

    fseek(rom, SEEK_SET, SEEK_END); // Set the cursor of the file from start to end
    const size_t rom_size = ftell(rom); // Using cursor, determines rom size
    const size_t max_size = sizeof chip8->ram - entry; // Maximum size of memory that can be allocated to programs as the from 0x0 - 0x200 is not available
    rewind(rom); //Rewind the cursor to read later
    if(rom_size > max_size){SDL_Log("ROM size is too big, Max size: %zu, ROM size: %zu" , max_size, rom_size); return 0;} // Return error if file size is too big 

    fread(&chip8->ram[entry], rom_size, 1, rom);

    fclose(rom); //Close rom file 


    chip8->pc = entry;
    chip8->state = RUNNING;
    chip8->rom_name = config->rom_name;
    chip8->stkptr = &chip8->stack[0];

    return 1; //Success
}

void clear_screen(sdl_type *sdl, config_type *config){
    const uint8_t r  = (config->bg_colour >> 24) & 0xFF; //Convert our background from 32 bit to 8 so each can be read as a seperate rgb value
    const uint8_t g  = (config->bg_colour >> 16) & 0xFF;
    const uint8_t b  = (config->bg_colour >> 8) & 0xFF;
    const uint8_t a  = (config->bg_colour >> 0) & 0xFF;

    //Set Renderer Colour to background
    SDL_SetRenderDrawColor(sdl->renderer, r,g,b,a);
    SDL_RenderClear(sdl->renderer);
}



void user_input(chip8_type *chip8, sdl_type *sdl, config_type *config){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        switch(event.type){
        case SDL_QUIT:{chip8->state = QUIT; break;} 
        case SDL_KEYDOWN:{
            switch(event.key.keysym.sym){
                case SDLK_ESCAPE:{SDL_Log("CHIP 8 is no longer running");chip8->state = QUIT; break;} // Used keycode so the chip 8 emualtor won't be platform specific
                case SDLK_p:{
                    if(chip8->state == RUNNING){chip8->state = PAUSED; SDL_Log("CHIP 8 is now paused");}
                    else{chip8->state = RUNNING; SDL_Log("CHIP 8 is now running");} 
                    break;
                }
                case SDLK_1:{chip8->keypad[0x1] =true; break;} //Handling Inputs 
                case SDLK_2:{chip8->keypad[0x2] =true; break;}
                case SDLK_3:{chip8->keypad[0x3] =true; break;}
                case SDLK_4:{chip8->keypad[0xC] =true; break;}

                case SDLK_q:{chip8->keypad[0x4] =true; break;}
                case SDLK_w:{chip8->keypad[0x5] =true; break;}
                case SDLK_e:{chip8->keypad[0x6] =true; break;}
                case SDLK_r:{chip8->keypad[0xD] =true; break;}

                case SDLK_a:{chip8->keypad[0x7] =true; break;}
                case SDLK_s:{chip8->keypad[0x8] =true; break;}
                case SDLK_d:{chip8->keypad[0x9] =true; break;}
                case SDLK_f:{chip8->keypad[0xE] =true; break;}

                case SDLK_z:{chip8->keypad[0xA] =true; break;}
                case SDLK_x:{chip8->keypad[0x0] =true; break;}
                case SDLK_c:{chip8->keypad[0xB] =true; break;}
                case SDLK_v:{chip8->keypad[0xF] =true; break;}
            }
            break;
        }
        
        case SDL_KEYUP:{
            switch(event.key.keysym.sym){

                case SDLK_1:{chip8->keypad[0x1] =false; break;} //Handling Inputs 
                case SDLK_2:{chip8->keypad[0x2] =false; break;}
                case SDLK_3:{chip8->keypad[0x3] =false; break;}
                case SDLK_4:{chip8->keypad[0xC] =false; break;}

                case SDLK_q:{chip8->keypad[0x4] =false; break;}
                case SDLK_w:{chip8->keypad[0x5] =false; break;}
                case SDLK_e:{chip8->keypad[0x6] =false; break;}
                case SDLK_r:{chip8->keypad[0xD] =false; break;}

                case SDLK_a:{chip8->keypad[0x7] =false; break;}
                case SDLK_s:{chip8->keypad[0x8] =false; break;}
                case SDLK_d:{chip8->keypad[0x9] =false; break;}
                case SDLK_f:{chip8->keypad[0xE] =false; break;}

                case SDLK_z:{chip8->keypad[0xA] =false; break;}
                case SDLK_x:{chip8->keypad[0x0] =false; break;}
                case SDLK_c:{chip8->keypad[0xB] =false; break;}
                case SDLK_v:{chip8->keypad[0xF] =false; break;}
            }
        }
            break;
        case SDL_WINDOWEVENT:{
            switch(event.window.event){
                case SDL_WINDOWEVENT_RESIZED:{
                    SDL_GetWindowSize(sdl->window, &config->res_x, &config->res_y);
                    SDL_DestroyRenderer(sdl->renderer);
                        sdl->renderer = SDL_CreateRenderer(
                            sdl->window,
                            -1,
                            SDL_RENDERER_ACCELERATED
                        ); //Creates Renderer using our pointer with said Parameters 
                        clear_screen(sdl, config);
                    break;
                }
            }
            break;
        }
            
    }
}


}

bool check_keypad(chip8_type *chip8, uint8_t *key_value){
    for(uint8_t i = 0; i < 16 && *key_value == 0xFF ; i++){if(chip8->keypad[i]){*key_value = i; return true;}}
    return false;
}


void emulate(chip8_type *chip8, config_type *config){
    //bool carry; // Set our carry flag

    //Have to or 2 bytes as one opcode is 2 bytes long 
    chip8->inst.opcode.data[0] = chip8->ram[chip8->pc+1];
    chip8->inst.opcode.data[1] = chip8->ram[chip8->pc];

    chip8->pc += 2;

    chip8->inst.NNN = chip8->inst.opcode.full_op & 0x0FFF; // Immediate Memory address, we want to mask of the last 3 nibbles
    chip8->inst.NN = chip8->inst.opcode.full_op & 0x0FF; // 8bit immediate number 
    chip8->inst.N = chip8->inst.opcode.full_op & 0x0F; //N nibble 
    chip8->inst.X = (chip8->inst.opcode.full_op >> 8) & 0x0F; // X register 
    chip8->inst.Y = (chip8->inst.opcode.full_op >> 4) & 0x0F; // Y register



    

    switch((chip8->inst.opcode.full_op & 0xF000) >> 12){ //Masks opcode so we only get 0xA000 where A is our Opcode
        case 0x0:{
            switch(chip8->inst.NN){
                case 0xE0:{memset(&chip8->display[0], false, sizeof(chip8->display)); chip8->draw = true; break;} //Clear display
                case 0xEE:{chip8->pc = *--chip8->stkptr; break;} //Pop off current subroutine and set pc to that subroutine
            }
            break;
        }
        case(0x1):{chip8->pc = chip8->inst.NNN; break;} // Jump to address NNN
        case(0x2):{*chip8->stkptr++ = chip8->pc; chip8->pc = chip8->inst.NNN; break;}
        case(0x3):{if(chip8->V[chip8->inst.X] == chip8->inst.NN){chip8->pc += 2;} break;} //If VX is equal to NN increment PC
        case(0x4):{if(chip8->V[chip8->inst.X] != chip8->inst.NN){chip8->pc += 2;} break;} //If VX is not equal to NN increment PC
        case(0x5):{if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]){chip8->pc += 2;} break;} // If VX == VY increment PC
        case(0x6):{chip8->V[chip8->inst.X] = chip8->inst.NN; break;} //Set VX = NN
        case(0x7):{chip8->V[chip8->inst.X] += chip8->inst.NN; break;} // Increment VX by the value NN
        case(0x8):{
            switch((chip8->inst.opcode.full_op & 0x000F)){
                case(0x0):{chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y]; break;}
                case(0x1):{chip8->V[chip8->inst.X] = chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]; break;}
                case(0x2):{chip8->V[chip8->inst.X] = chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]; break;}
                case(0x3):{chip8->V[chip8->inst.X] = chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]; break;}
                case(0x4):{
                    uint16_t result = chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = (result > 0xFF) ? 1 : 0;
                    chip8->V[chip8->inst.X] = (uint8_t)result; 
                    break;
                }
                case(0x5):{
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = (chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y]) ? 1 : 0;
                    break; 
                    }
                case(0x6):{
                    switch(config->choice){
                        case(COSMAC):{
                                chip8->V[0xF] = (chip8->V[chip8->inst.Y] & 0x1);
                                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] >> 1;
                                break;
                        }
                        case(AMIGA):{
                                chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x1);
                                chip8->V[chip8->inst.X] >>= 1;
                                break;
                        }
                    }
                    break;
                }
                case(0x7):{
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = (chip8->V[chip8->inst.Y] >= chip8->V[chip8->inst.X]) ? 1 : 0;
                    break;
                }
                case(0xE):{
                    switch(config->choice){
                            case(COSMAC):{
                                chip8->V[0xF] = (chip8->V[chip8->inst.Y] & 0x80) >> 7;
                                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] << 1;
                                break;
                        }
                        case(AMIGA):{
                                chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
                                chip8->V[chip8->inst.X] <<= 1;
                                break;
                        }

                    }
                    break;
                }

            }
        }
        break;
        case(0x9):{if(chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]){chip8->pc += 2; }break;}
        case(0xA):{chip8->I = chip8->inst.NNN; break;}
        case(0xB):{
            switch(config->choice){
                case(COSMAC):{
                    chip8->pc = chip8->inst.NNN + chip8->V[0];
                    break;
                }
                case(AMIGA):{
                    chip8->pc = chip8->inst.NNN + chip8->V[chip8->inst.X];
                    break;
                }
            }   
            break;
        }
        case(0xC):{srand(time(NULL)); uint8_t random = rand(); chip8->V[chip8->inst.X] = random & chip8->inst.NN; break;}
        case(0xD):{
             // 0xDXYN: Draw N-height sprite at coords X,Y; Read from memory location I;
            //   Screen pixels are XOR'd with sprite bits, 
            //   VF (Carry flag) is set if any screen pixels are set off; This is useful
            //   for collision detection or other reasons.
            
            uint8_t X_coord = chip8->V[chip8->inst.X] % 64;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % 32;
            const uint8_t orig_X = X_coord; // Original X value

            chip8->V[0xF] = 0;  // Initialize carry flag to 0   

            // Loop over all N rows of the sprite
            for (uint8_t i = 0; i < chip8->inst.N; i++) {
                // Get next byte/row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X;   // Reset X for next row to draw

                for (int8_t j = 7; j >= 0; j--) {
                    // If sprite pixel/bit is on and display pixel is on, set carry flag
                    bool *pixel = &chip8->display[Y_coord * 64 + X_coord]; 
                    const bool sprite_bit = (sprite_data & (1 << j));

                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;  
                    }

                    // XOR display pixel with sprite pixel/bit to set it on or off
                    *pixel ^= sprite_bit;

                    // Stop drawing this row if hit right edge of screen
                    if (++X_coord >= 64) break;
                }

                // Stop drawing entire sprite if hit bottom edge of screen
                if (++Y_coord >= 32) break;
            }
            chip8->draw = true; // Will update screen on next 60hz tick
            break;
        }
        case(0xE):{
            switch(chip8->inst.NN){
                case(0x9E):{if(chip8->keypad[chip8->V[chip8->inst.X]]){chip8->pc += 2;} break;}
                case(0xA1):{if(!chip8->keypad[chip8->V[chip8->inst.X]]){chip8->pc += 2;} break;}
            }
        }
            break;
        case(0xF):{
            switch(chip8->inst.NN){
                case(0x07):{chip8->V[chip8->inst.X] = chip8->delay_timer; break;}
                case(0x0A):{
                    uint8_t key_value = 0xFF;
                    bool key_pressed = false;
                    key_pressed = check_keypad(chip8, &key_value);

                    if(!key_pressed){chip8->pc -= 2; break;}
                    chip8->V[chip8->inst.X] = key_value;
                    break;
                }
                case(0x15):{chip8->delay_timer = chip8->V[chip8->inst.X]; break;}
                case(0x18):{chip8->sound_timer = chip8->V[chip8->inst.X]; break;}
                case(0x1E):{
                    switch(config->choice){
                        case 0:{chip8->I += chip8->V[chip8->inst.X]; break;}
                        case 1:{
                            uint32_t result = chip8->I + chip8->V[chip8->inst.X]; 
                            chip8->V[0xF] = (result > 0xFFF) ? 1 : 0;
                            chip8->I = (uint16_t)result;
                            break;
                        }
                    }
                    break;
                }
                case(0x29):{chip8->I = chip8->V[chip8->inst.X] * 5; break;}
                case(0x33):{
                    uint8_t va = chip8->V[chip8->inst.X];
                    chip8->ram[chip8->I+2] = va % 10;
                    va /= 10;
                    chip8->ram[chip8->I+1] = va % 10;
                    va /= 10;
                    chip8->ram[chip8->I] = va;
                    break;
                }
            case(0x55):{
                switch(config->choice){
                    case(COSMAC):{for(int i = 0; i <= chip8->inst.X; i++){chip8->ram[chip8->I+i] = chip8->V[i];} chip8->I = chip8->inst.X + 1; break;}
                    case(AMIGA):{for(int i = 0; i <= chip8->inst.X; i++){chip8->ram[chip8->I+i] = chip8->V[i];} break;}
                }
                break;
            }
            case(0x65):{
                switch(config->choice){
                    case(COSMAC):{for(int i = 0; i <= chip8->inst.X; i++){chip8->V[i] = chip8->ram[chip8->I+i];} chip8->I = chip8->inst.X + 1; break;}
                    case(AMIGA):{for(int i = 0; i <= chip8->inst.X; i++){chip8->V[i] = chip8->ram[chip8->I+i];} break;}
                }
                break;
            }
            
            }
        }

    }
}

void update_timers(chip8_type *chip8){
    if(chip8->delay_timer > 0){chip8->delay_timer--;}
    if(chip8->sound_timer > 0){
        chip8->sound_timer--;
        //Need some code to beep here


        }




}

void draw(const sdl_type sdl, chip8_type *chip8){
   SDL_Rect rect = {.x = 0, .y = 0, .w = 20, .h = 20};



    // Loop through display pixels, draw a rectangle per pixel to the SDL window
    for (uint32_t i = 0; i < sizeof chip8->display; i++) {
        // Translate 1D index i value to 2D X/Y coordinates
        // X = i % window width
        // Y = i / window width
        rect.x = (i % 64) * 10;
        rect.y = (i / 32) * 10;

        if (chip8->display[i]) {
            // Pixel is on, draw foreground color
            SDL_SetRenderDrawColor(sdl.renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(sdl.renderer, &rect);
        } 
        else {
            SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl.renderer);

}

int main(int argc, char *argv[]){
    (void) argc;
    (void) argv;
    config_type config = {0};
    read_in_config(&config);

    // Intialise SDL 
    sdl_type sdl = {0}; //Create SDL "Object"
    chip8_type chip8 = {0}; 

    if(!init_sdl(&sdl, &config)){exit(EXIT_FAILURE);}
    if(!init_chip8(&chip8, &config)){exit(EXIT_FAILURE);}

    clear_screen(&sdl, &config);

    while(chip8.state != QUIT){
        user_input(&chip8, &sdl, &config);
        if(chip8.state  == PAUSED){continue;}

        const uint64_t start_time = SDL_GetPerformanceCounter();

        for(int i = 0; i < config.insts_per_sec / 60; i++){
            emulate(&chip8,&config);
        }

        const uint64_t end_time = SDL_GetPerformanceCounter();

        const uint64_t elapsed_time = end_time - start_time * 1000 / SDL_GetPerformanceFrequency();

        SDL_Delay(16.67f - elapsed_time);

        if(chip8.draw){draw(sdl,&chip8); chip8.draw = false;}
        update_timers(&chip8);
 
        

    }


    



    
  

    //Ends SDL
    end(&sdl);
    exit(EXIT_SUCCESS);
    return 0;



}


/*
Implement DXYN and drawing
audio

00E0
2NNN
6XNN
7XNN
8XY7
BNNN
CXNN
DXYN
EX9E
EXA1
FX07
FX15
FX18
FX1E
FX0A
FX29




Super Chip
Xo chip 

*/