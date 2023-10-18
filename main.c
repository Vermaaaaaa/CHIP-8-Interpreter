#include <stdio.h>
#include "D:\SDL2-2.28.4\include\SDL.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>



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

typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
} emu_state;


typedef struct{
    emu_state state;
    uint8_t ram[4096]; //Ram for the chip 8
    bool display[64*32]; //Decided to have seperate arrays for display instead of a pointer to ram. May change if I move onto super chip
    uint16_t stack[48]; 
    uint8_t V[16]; //Registers from V0-Vf
    uint16_t I; //Index Register  

} chip8_type;



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

void set_config(config_type *config){
    *config = (config_type){
        .fg_colour = 0xFFFFFFFF, // White 
        .bg_colour = 0x00000000 // Black 
    };

}

int init_chip8(chip8_type *chip8){
    chip8->state = RUNNING;
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

    switch(event.type){
        case SDL_QUIT:{chip8->state = QUIT; return; break;} 
        case SDL_KEYDOWN:{
        switch(event.key.keysym.sym){
            case SDLK_ESCAPE:{chip8->state = QUIT; break;} // Used keycode so the chip 8 emualtor won't be platform specific
            default:{break;}
        }



            return; 
            break;
        
        
        
        
        }
        case SDL_KEYUP:{return; break;}
        default:{break;}
    }
}

int main(int argc, char **argv){
    (void) argc; // Prevents Compiler Error from unused variables 
    (void) argv;

    // Intialise SDL 
    sdl_type sdl = {0}; //Create SDL "Object"
    config_type config = {0};
    chip8_type chip8 = {0}; 
    if(!init_sdl(&sdl)){exit(EXIT_FAILURE);}
    //if(!set_config(&config)){exit(EXIT_FAILURE);}
    if(!init_chip8(&chip8)){exit(EXIT_FAILURE);}

    clear_screen(&sdl, config);

    while(chip8.state!=QUIT){
        //Delay for 60Hz
        /*We need to calculate the time elapsed by instructions running and minus this from the delay 
        so the chip clocks at 60Hz still 
        Use the system clock to measure the time diff
        so SDL_Delay should be SDL_Delay(16 - elapsed time);
        */

        user_input(&chip8);
        SDL_Delay(16);
        update_screen(&sdl);

    }


    



    
  

    //Ends SDL
    end(&sdl);
    exit(EXIT_SUCCESS);
    return 0;



}