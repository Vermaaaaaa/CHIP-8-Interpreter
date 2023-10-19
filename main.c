#define SDL_MAIN_HANDLED

#include <stdio.h>
#include "D:\SDL2-2.28.4\include\SDL.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <windows.h>




//SDL object
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_type;
//Create a struct that holds our pointer to a window (More OOP approach)


//Config Object
typedef struct {
    uint32_t bg_colour;
    uint32_t fg_colour;

} config_type;


//Enum for emulation state
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
} emu_state;

//Chip 8 object
typedef struct{
    emu_state state;
    uint8_t ram[4096]; //Ram for the chip 8
    bool display[64*32]; //Decided to have seperate arrays for display instead of a pointer to ram. May change if I move onto super chip
    uint16_t stack[48]; 
    uint8_t V[16]; //Registers from V0-Vf
    uint16_t I; //Index Register  
    uint16_t pc;
    uint8_t delay_timer;
    uint8_t sound_timer; //60Hz timers in chip 8
    bool keypad[16]; //Check if keypad is in off or on state
    const char *rom_name; // Get a command line dir for rom to load into ram
} chip8_type;


//Initialiser for sdl object 
int init_sdl(sdl_type *sdl){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0){
        SDL_Log("SDL couldn't Initialise %s\n", SDL_GetError());
        return 1; // Returns 1 to show that SDL wasn't initalised
    }

    sdl-> window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640,
        480,
        SDL_WINDOW_OPENGL  
    ); //Creates Window using our pointer with said Parameters 

    if(!sdl->window){SDL_Log("Could not create Window %s\n", SDL_GetError()); return 1;} //If window can't open throw error

    sdl->renderer = SDL_CreateRenderer(
        sdl->window,
        -1,
        SDL_RENDERER_ACCELERATED
    ); //Creates Renderer using our pointer with said Parameters 

    if(!sdl->renderer){SDL_Log("Could not create Renderer %s\n", SDL_GetError()); return 1;} //If renderer can't initialise throw error

    return 0; // Success
}



void end(sdl_type *sdl){
    SDL_DestroyRenderer(sdl->renderer); // Destroys the Renderer
    SDL_DestroyWindow(sdl->window); // Destroys the window
    SDL_Quit(); //Shutsdown SDL
    //sdl = NULL;
}


//Set configuration for emulator (Could be better in a config file YAML,JSON, INI etc.)
void set_config(config_type *config){
    *config = (config_type){
        .fg_colour = 0xFFFFFFFF, // White 
        .bg_colour = 0x00000000 // Black 
    };

}

int init_chip8(chip8_type *chip8 , const char rom_name[]){
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
    FILE *rom = fopen(rom_name, "rb"); // Set File object to read bytes as the file is raw
    if(!rom){SDL_Log("ROM file %s is invalid or does not exist\n" ,rom_name); return 1;} //Return error if file cannot be opened

    fseek(rom, SEEK_SET, SEEK_END); // Set the cursor of the file from start to end
    const size_t rom_size = ftell(rom); // Using cursor, determines rom size
    const size_t max_size = sizeof chip8->ram - entry; // Maximum size of memory that can be allocated to programs as the from 0x0 - 0x200 is not available
    if(rom_size > max_size){SDL_Log("ROM size is too big, Max size: %zu, ROM size: %z" , max_size, rom_size); return 1;} // Return error if file size is too big 

    fread(&chip8->ram[entry], rom_size, 1, rom);

    fclose(rom); //Close rom file 


    chip8->pc = entry;
    chip8->state = RUNNING;
    chip8->rom_name = rom_name;
    

    return 0; //Success
}

void clear_screen(sdl_type *sdl, const config_type config){
    const uint8_t r  = (uint8_t) (config.bg_colour >> 24); //Convert our background from 32 bit to 8 so each can be read as a seperate rgb value
    const uint8_t g  = (uint8_t) (config.bg_colour >> 16);
    const uint8_t b  = (uint8_t) (config.bg_colour >> 8);
    const uint8_t a  = (uint8_t) (config.bg_colour >> 0);

    //Set Renderer Colour to background
    SDL_SetRenderDrawColor(sdl->renderer, r,g,b,a);
    SDL_RenderClear(sdl ->renderer);
}

void update_screen(sdl_type *sdl){
    SDL_RenderPresent(sdl->renderer);

}

void user_input(chip8_type *chip8){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        switch(event.type){
        case SDL_QUIT:{chip8->state = QUIT; return; break;} 
        case SDL_KEYDOWN:{
            switch(event.key.keysym.sym){
                case SDLK_ESCAPE:{chip8->state = QUIT; break;} // Used keycode so the chip 8 emualtor won't be platform specific
                case SDLK_p:{chip8->state = PAUSED; break;}
                default:{break;}
        }
            return; 
            break;
        }
        
        case SDL_KEYUP:{return; break;}
        default:{break;}
    }



    } 
}

int main(int argc, char **argv){
    (void) argc; // Prevents Compiler Error from unused variables 
    (void) argv;

    // Intialise SDL 
    sdl_type sdl = {0}; //Create SDL "Object"
    config_type config = {0};
    chip8_type chip8 = {0}; 
    const char *rom_name = argv[1];
    if(!init_sdl(&sdl)){exit(EXIT_FAILURE);}
    //if(!set_config(&config)){exit(EXIT_FAILURE);}
    if(!init_chip8(&chip8, rom_name)){exit(EXIT_FAILURE);}

    clear_screen(&sdl, config);

    while(true){
        if(chip8.state  == PAUSED){continue;}
        //Delay for 60Hz
        /*We need to calculate the time elapsed by instructions running and minus this from the delay 
        so the chip clocks at 60Hz still 
        Use the system clock to measure the time diff
        so SDL_Delay should be SDL_Delay(16 - elapsed time);
        */

        user_input(&chip8);
        SDL_Delay(50000);
        update_screen(&sdl);
        

    }


    



    
  

    //Ends SDL
    end(&sdl);
    exit(EXIT_SUCCESS);
    return 0;



}