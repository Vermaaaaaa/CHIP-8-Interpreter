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
} config_type;


//Enum for emulation state
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
} emu_state;

typedef struct{
    uint16_t opcode;
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
        config->res_x,
        config->res_y,
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

    char *token = strtok(line, "=");
    if (token != NULL) {
        strlcpy(key_copy, token, sizeof(key_copy));
        token = strtok(NULL, "(");
        if (token != NULL) {
            strlcpy(value_copy, token, sizeof(value_copy));
            // Trim whitespaces, newline, tab, and return carriage
            strlcpy(key, strtok(key_copy, " \t\n\r"), sizeof(key_copy));
            strlcpy(value, strtok(value_copy, " \t\n\r"), sizeof(value_copy));
        }
    }
}

void read_in_config(config_type *config){
    char line[100]; //Specify Max line Length (Will re assess as dont need to initalise memory that wont be used)
    FILE *config_f = fopen("config.txt", "r"); 

    if(config_f == NULL){SDL_Log("Config File does not contain anything");}

    char key[50];
    char value[50];

    while(fgets(line, sizeof(line), config_f)){
        fileparser(line, key, value);
        if(strcmp(key, "bg_colour") == 0){config->bg_colour = (uint32_t)strtoul(value, NULL, 0);}
        else if(strcmp(key, "fg_colour") == 0){config->fg_colour = (uint32_t)strtoul(value, NULL, 0);}
        else if(strcmp(key, "res_x") == 0){config->res_x = atoi(value);}
        else if(strcmp(key, "res_y") == 0){config->res_y = atoi(value);}
        else if(strcmp(key, "rom_name") == 0){strlcpy(config->rom_name, value, sizeof(value));}
        else if(strcmp(key, "emulator_type") == 0){config->choice = atoi(value);}
        else{SDL_Log("Not getting read properly");}
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
    

    return 1; //Success
}

void clear_screen(sdl_type *sdl, config_type *config){
    const uint8_t r  = (uint8_t) (config->bg_colour >> 24); //Convert our background from 32 bit to 8 so each can be read as a seperate rgb value
    const uint8_t g  = (uint8_t) (config->bg_colour >> 16);
    const uint8_t b  = (uint8_t) (config->bg_colour >> 8);

    //Set Renderer Colour to background
    SDL_SetRenderDrawColor(sdl->renderer, r,g,b,(uint8_t)config->bg_colour);
    SDL_RenderClear(sdl->renderer);
}

void update_screen(sdl_type *sdl){
    SDL_RenderPresent(sdl->renderer);
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


void emulate(chip8_type *chip8, config_type* config){
    //bool carry; // Set our carry flag

    chip8->inst.opcode = (chip8->ram[chip8->pc] << 8 | chip8->ram[chip8->pc+1]); //Have to or 2 bytes as one opcode is 2 bytes long 
    chip8->pc += 2;

    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF; // Immediate Memory address, we want to mask of the last 3 nibbles
    chip8->inst.NN = chip8->inst.opcode & 0x00FF; // 8bit immediate number 
    chip8->inst.N = chip8->inst.opcode & 0x000F; //N nibble 
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x000F; // X register 
    chip8->inst.Y = (chip8->inst.opcode << 4) & 0x000F; // Y register 


    

    switch((chip8->inst.opcode & 0xF000) >> 12){ //Masks opcode so we only get 0xA000 where A is our Opcode
        case 0x0:{
            switch(chip8->inst.NN){
                case 0xE0:{memset(&chip8->display, false, sizeof(chip8->display)); break;} //Clear display
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
            switch((chip8->inst.opcode & 0x000F) >> 12){ 
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
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = (chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y]) ? 1 : 0;
                    break; 
                    }
                case(0x6):{
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = chip8->V[chip8->inst.X] & 0x1;
                    chip8->V[chip8->inst.X] >>= 1;
                    break;
                }
                case(0x7):{
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = (chip8->V[chip8->inst.Y] >= chip8->V[chip8->inst.X]) ? 1 : 0;
                    break;
                }
                case(0xE):{
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = chip8->V[chip8->inst.X] & 0x1;
                    chip8->V[chip8->inst.X] <<= 1;
                    break;
                }

            }
        }
        break;
        case(0x9):{if(chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]){chip8->pc += 2; }break;}
        case(0xA):{chip8->I = chip8->inst.NNN; break;}
        case(0xB):{
            if(chip8->inst.X == 0x0){chip8->pc = chip8->inst.NNN + chip8->V[0];} //Case BNNN
            else{chip8->pc = chip8->inst.NNN + chip8->V[chip8->inst.X];} //Case BXNN    
            break;
        }
        case(0xC):{srand(time(NULL)); uint8_t random = rand(); chip8->V[chip8->inst.X] = random & chip8->inst.NN; break;}
        case(0xD):{break;}
        case(0xE):{
            switch(chip8->inst.NN){
                case(0x9E):{if(chip8->keypad[chip8->V[chip8->inst.X]]){chip8->pc += 2;} break;}
                case(0xA1):{if(!chip8->keypad[chip8->V[chip8->inst.X]]){chip8->pc += 2;} break;}
            }
        }
            break;
        case(0xF):{
            switch(chip8->inst.NN){
                case(0x07):{break;}
                case(0x15):{break;}
                case(0x18):{break;}
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
                }
            }
        }

    }
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
        //Delay for 60Hz
        /*We need to calculate the time elapsed by instructions running and minus this from the delay 
        so the chip clocks at 60Hz still 
        Use the system clock to measure the time diff
        so SDL_Delay should be SDL_Delay(16 - elapsed time);
        */

        SDL_Delay(16);
        update_screen(&sdl);
        

    }


    



    
  

    //Ends SDL
    end(&sdl);
    exit(EXIT_SUCCESS);
    return 0;



}