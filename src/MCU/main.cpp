#include "mbed.h"
#include "N5110/N5110.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmath> 
#include "Joystick/Joystick.h"
#include "Roms/blitz.h"
#include "Roms/wall.h"
#include "Roms/si.h"
#include "Roms/tetris.h"
#include "Roms/breakout.h"
#include "Roms/merlin.h"
#include "Keypad/keypad.h"





N5110 lcd(PC_7, PA_9, PB_10, PB_5, PB_3, PA_10);
InterruptIn joystick_button(PC_10);
Joystick stick(PC_3, PC_2);
Keypad keypad(ARDUINO_UNO_A5, ARDUINO_UNO_A4, ARDUINO_UNO_A3, ARDUINO_UNO_A2, ARDUINO_UNO_A1, ARDUINO_UNO_A0, PH_0, PH_1);
DigitalIn but(BUTTON1);
DigitalOut user_led(LED1);
PwmOut speaker(PC_8);

volatile int g_button_flag = 0;
int state = 0;

Timer t;

constexpr uint32_t entry = 0x200; //Entry point for roms to be loaded into memory

constexpr uint8_t font[] = {
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


typedef enum{
    COSMAC,
    AMIGA, 
}emu_type;

typedef enum{
    BLITZ,
    BREAKOUT,
    SI,
    MERLIN,
    TETRIS,
    WALL,
}rom;

//Config Object
typedef struct {
    emu_type emu_choice;
    FillType bg_colour;
    FillType fg_colour;
    int res_x;
    int res_y;
    int insts_per_sec;
    unsigned int sf_x;
    unsigned int sf_y;
    rom rom_choice;
    float contrast;
    float brightness;
    float volume;
    int freq;
    int clk_speed;
} config_type;

//Enum for emulation state
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
    OFF
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
    instr_type inst;
    bool draw;
} chip8_type;

typedef enum{
    MAIN,
    PAUSE,
    SETTINGS,
    SCREEN,
    AUDIO,
    EMU,
    GAME,
    HOW

}menu_type;

char str_buffer[14] = {0};

void main_screen(config_type* config, chip8_type *chip8);
void settings_screen(menu_type menu, config_type *config, chip8_type *chip8);
void button_input(bool* select);
void game_select_screen(config_type *config, chip8_type* chip8);

void init_config(config_type *config){
    config->emu_choice = AMIGA;
    config->clk_speed = 60;
    config->rom_choice = BLITZ;
    config->sf_x = 1;
    config->sf_y = 1;
    config->res_x = 64;
    config->res_y = 32;
    config->bg_colour = FILL_BLACK;
    config->fg_colour = FILL_WHITE;
    config->insts_per_sec = 700;
    config->brightness = 0.5f;
    config->contrast = 0.55f;
    config->volume = 0.5f;
    config->freq = 1000;

}

void init_chip8(chip8_type *chip8, const config_type* config){
    
    memset(chip8->ram, 0, sizeof(chip8->ram));
    memset(chip8->display, 0, sizeof(chip8->display));
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memset(chip8->V, 0, sizeof(chip8->V));
    chip8->I = 0;
    chip8->pc = 0;
    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    memset(chip8->keypad, false, sizeof(chip8->keypad));
    chip8->draw = false;

    //Load Font
    memcpy(chip8->ram, font, sizeof(font));

    //Load Rom


    chip8->pc = entry;
    chip8->state = QUIT;
    chip8->stkptr = &chip8->stack[0];

   

     //Success
}

void reset(chip8_type *chip8){

    memset(chip8->display, 0, sizeof(chip8->display));
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memset(chip8->V, 0, sizeof(chip8->V));
    chip8->I = 0;
    chip8->pc = 0;
    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    memset(chip8->keypad, false, sizeof(chip8->keypad));
    chip8->draw = false;
    chip8->pc = entry;
    chip8->state = RUNNING;
    chip8->stkptr = &chip8->stack[0];
}

void game_set(config_type *config, chip8_type* chip8){
    memset(chip8->ram + entry, 0, sizeof(chip8->ram) - entry);
    switch(config->rom_choice){
        case(BLITZ):{memcpy(chip8->ram + entry, blitz_data, sizeof(blitz_data)); break;}
        case(BREAKOUT):{memcpy(chip8->ram + entry, breakout_data, sizeof(breakout_data)); break;}
        case(WALL):{memcpy(chip8->ram + entry, wall_data, sizeof(wall_data)); break;}
        case(SI):{memcpy(chip8->ram + entry, si_data, sizeof(si_data)); break;}
        case(TETRIS):{memcpy(chip8->ram + entry, tetris_data, sizeof(tetris_data)); break;}
        case(MERLIN):{memcpy(chip8->ram + entry, merlin_data, sizeof(merlin_data)); break;}
    }
    chip8->state = RUNNING;
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
                    chip8->V[chip8->inst.X] = static_cast<uint8_t>(result);
                    break;
                }
                case(0x5):{
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = (chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y]) ? 1 : 0;
                    break; 
                    }
                case(0x6):{
                    switch(config->emu_choice){
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
                    switch(config->emu_choice){
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
            switch(config->emu_choice){
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
                    switch(config->emu_choice){
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
                switch(config->emu_choice){
                    case(COSMAC):{for(int i = 0; i <= chip8->inst.X; i++){chip8->ram[chip8->I+i] = chip8->V[i];} chip8->I = chip8->inst.X + 1; break;}
                    case(AMIGA):{for(int i = 0; i <= chip8->inst.X; i++){chip8->ram[chip8->I+i] = chip8->V[i];} break;}
                }
                break;
            }
            case(0x65):{
                switch(config->emu_choice){
                    case(COSMAC):{for(int i = 0; i <= chip8->inst.X; i++){chip8->V[i] = chip8->ram[chip8->I+i];} chip8->I = chip8->inst.X + 1; break;}
                    case(AMIGA):{for(int i = 0; i <= chip8->inst.X; i++){chip8->V[i] = chip8->ram[chip8->I+i];} break;}
                }
                break;
            }
            
            }
        }

    }
}
void sound_timer_play(config_type* config){
    speaker.period(1.0f/config->freq);
    speaker.write(config->volume);
}

void sound_timer_stop(){
    speaker.write(0.0f);
}

void update_timers(chip8_type *chip8, config_type* config){
    if(chip8->delay_timer > 0){chip8->delay_timer--;}

    if(chip8->sound_timer > 0){
        chip8->sound_timer--;
        sound_timer_play(config);
    }
    else{
        sound_timer_stop();
    }
}

void draw(chip8_type *chip8, const config_type *config){
    lcd.clear();
    // Loop through display pixels, draw a rectangle per pixel to the screen
    for (uint32_t i = 0; i < sizeof chip8->display; i++) {
        const unsigned int x0 = 10 + (i % config->res_x) * config->sf_x;
        const unsigned int y0 = 8 + (i / config->res_x) * config->sf_y;
        if (chip8->display[i]) {
            // Pixel is on, draw foreground color
            lcd.drawRect(x0, y0, config->sf_x, config->sf_y, config->fg_colour);
        } 
        else {
            lcd.drawRect(x0, y0, config->sf_x, config->sf_y, config->bg_colour);
        }
    }
    lcd.refresh();
}

void end(){
    lcd.clear();
   
    int i = 5;
    while(i > 0){
        lcd.clear();
        lcd.printString("Shutting down", 0, 1);
        sprintf(str_buffer, "       %d", i);
        lcd.printString(str_buffer, 0, 2);
        lcd.refresh();
        ThisThread::sleep_for(1s);
        i--;
    }

    lcd.clear();
    lcd.printString("   Goodbye", 0, 1);
    lcd.refresh();
    ThisThread::sleep_for(1s);

    lcd.clear();
    lcd.turnOff();
}


void isr(){
    g_button_flag = 1;
}

void menu_input(int* current_bank, menu_type state, config_type* config){
        Direction dir = stick.get_direction();
        switch(state){
            case(MAIN):{
                switch(dir){
                    default:{break;}
                    case(N):{
                        switch(*current_bank){
                            case(2):{*current_bank = 4; break;}
                            default:{(*current_bank)--; break;}
                        }
                        break;
                    }
                    case(S):{
                        switch(*current_bank){
                            case(4):{(*current_bank) = 2; break;}
                            default:{(*current_bank)++; break;}
                        }
                        break;
                    }
                }
                break;
            }
            case(PAUSE):{
                switch(dir){
                    default:{break;}
                    case(N):{
                        switch(*current_bank){
                            case(2):{(*current_bank) = 5; break;}
                            default:{(*current_bank)--; break;}
                        }
                        break;
                    }
                    case(S):{
                        switch(*current_bank){
                            case(5):{(*current_bank) = 2; break;}
                            default:{(*current_bank)++; break;}
                        }
                        break;
                    }
                }
                break;
            }
            case(SCREEN):{
                switch(dir){
                    default:{break;}
                    case(N):{
                        switch(*current_bank){
                            case(2):{*current_bank = 4; break;}
                            default:{(*current_bank)--; break;}
                        }
                        break;
                    }
                    case(S):{
                        switch(*current_bank){
                            case(4):{(*current_bank) = 2; break;}
                            default:{(*current_bank)++; break;}
                        }
                        break;
                    }
                    case(E):{
                        switch(*current_bank){
                            case(2):{
                                if(config->contrast >= 1.0f){config->contrast = 0.0f; break;}
                                else{config->contrast = config->contrast + 0.1f; break;}
                            }
                            case(3):{
                                if(config->brightness >= 1.0f){config->brightness = 0.0f; break;}
                                else{config->brightness = config->brightness + 0.1f; break;}
                            }
                            default:{break;}
                        }

                        break;
                    }
                    case(W):{
                        switch(*current_bank){
                            case(2):{
                                if(config->contrast <= 0.0f){config->contrast = 1.0f; break;}
                                else{config->contrast = config->contrast - 0.1f; break;}

                            }
                            case(3):{
                                if(config->brightness <= 0.0f){config->brightness = 1.0f; break;}
                                else{config->brightness = config->brightness - 0.1f; break;}
                            }
                            default:{break;}
                        }
                        break;
                    }
                }
                break;
            }
            case(AUDIO):{
                switch(dir){
                    default:{break;}
                    case(N):{
                        switch(*current_bank){
                            case(2):{(*current_bank) = 4; break;}
                            default:{(*current_bank)--; break;}
                        }
                        break;
                    }
                    case(S):{
                        switch(*current_bank){
                            case(4):{(*current_bank) = 2; break;}
                            default:{(*current_bank)++; break;}
                        }
                        break;
                    }
                    case(E):{
                        switch(*current_bank){
                            default:{break;}
                            case(2):{
                                if(config->volume >= 1.0f){config->volume = 0.0f; break;}
                                config->volume = config->volume + 0.1;
                                break;
                            }
                            case(3):{
                                switch(config->freq){
                                    case(1000):{config->freq = 0; break;}
                                    default:{config->freq = config->freq + 100; break;}
                                }
                                break;
                            }
                        }
                        break;
                    }
                    case(W):{
                        switch(*current_bank){
                            default:{break;}
                            case(2):{
                                if(config->volume <= 0.0f){config->volume = 1.0f; break;}
                                else{config->volume = config->volume - 0.1f; break;}
                            }
                            case(3):{
                                switch(config->freq){
                                    case(0):{config->freq = 1000; break;}
                                    default:{config->freq = config->freq - 100; break;}
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
                break;
            }
            case(EMU):{
                switch(dir){
                    default:{break;}
                    case(N):{
                        switch(*current_bank){
                            case(2):{(*current_bank) = 4; break;}
                            default:{(*current_bank)--; break;}
                        }
                        break;
                    }
                    case(S):{
                        switch(*current_bank){
                            case(4):{(*current_bank) = 2; break;}
                            default:{(*current_bank)++; break;}
                        }
                        break;
                    }
                    case(E):{
                        switch(*current_bank){
                            default:{break;}
                            case(2):{
                                switch(config->emu_choice){
                                    case(COSMAC):{config->emu_choice = AMIGA; break;}
                                    case(AMIGA):{config->emu_choice = COSMAC; break;}
                                }
                                break;
                            }
                            case(3):{
                                switch(config->clk_speed){
                                    case(300):{config->clk_speed = 10; break;}
                                    default:{config->clk_speed = config->clk_speed + 10; break;}
                                }
                                break;
                            }
                        }
                        break;
                    }
                    case(W):{
                        switch(*current_bank){
                            default:{break;}
                            case(2):{
                                switch(config->emu_choice){
                                    case(COSMAC):{config->emu_choice = AMIGA; break;}
                                    case(AMIGA):{config->emu_choice = COSMAC; break;}
                                }
                                break;
                            }
                            case(3):{
                                switch(config->clk_speed){
                                    case(10):{config->clk_speed = 300; break;}
                                    default:{config->clk_speed = config->clk_speed - 10; break;}
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
                break;

            }
            case(GAME):{
                switch(dir){
                    default:{break;}
                    case(N):{
                        switch(*current_bank){
                            case(2):{(*current_bank) = 4; break;}
                            default:{(*current_bank)--; break;}
                        }
                        break;
                    }
                    case(S):{
                        switch(*current_bank){
                            case(4):{(*current_bank) = 2; break;}
                            default:{(*current_bank)++; break;}
                        }
                        break;
                    }
                    case(E):{
                        switch(*current_bank){
                            default:{break;}
                            case(2):{
                                switch(config->rom_choice){
                                    case(BLITZ):{config->rom_choice = BREAKOUT; break;}
                                    case(BREAKOUT):{config->rom_choice = MERLIN; break;}
                                    case(MERLIN):{config->rom_choice = SI; break;}
                                    case(SI):{config->rom_choice = TETRIS; break;}
                                    case(TETRIS):{config->rom_choice = WALL; break;}
                                    case(WALL):{config->rom_choice = BLITZ; break;}
                                }
                                break;
                            }
                        }
                        break;
                    }
                    case(W):{
                        switch(*current_bank){
                            default:{break;}
                            case(2):{
                                switch(config->rom_choice){
                                    case(WALL):{config->rom_choice = TETRIS; break;}
                                    case(TETRIS):{config->rom_choice = SI; break;}
                                    case(SI):{config->rom_choice = MERLIN; break;}
                                    case(MERLIN):{config->rom_choice = BREAKOUT; break;}
                                    case(BREAKOUT):{config->rom_choice = BLITZ; break;}
                                    case(BLITZ):{config->rom_choice = WALL; break;}
                                }
                                break;
                            }
                        }
                        break;
                    }

                    
                }
                break;
            }
        }


}

void pause_menu(){

    lcd.printString("PAUSED", 24, 0);
    lcd.printString("Settings", 18, 2);
    lcd.printString("Main Menu", 15, 3);
    lcd.printString("Reset", 27, 4);
    lcd.printString("Power OFF", 15, 5);
}

void main_menu(){
    lcd.printString("RAHUL'S CHIP-8", 0, 0);
    lcd.printString("START", 27, 2);
    lcd.printString("Settings", 18, 3);
    lcd.printString("Power OFF", 15, 4);
}

void game_selection_menu(config_type *config){
    lcd.printString("GAMES", 27, 0);
    lcd.printString("GAMES: ", 12, 2);
    switch(config->rom_choice){
        default:{break;}
        case(BLITZ):{lcd.printString("BLITZ", 48, 2); break;}
        case(SI):{lcd.printString("SI", 48, 2); break;}
        case(WALL):{lcd.printString("WALL", 48, 2); break;}
        case(BREAKOUT):{lcd.printString("B'OUT", 48, 2); break;}
        case(TETRIS):{lcd.printString("TETRIS", 48, 2); break;}
        case(MERLIN):{lcd.printString("MERLIN", 48, 2); break;}
    }
    lcd.printString("HOW TO PLAY", 12, 3);
    lcd.printString("BACK", 30, 4);


}

void how_menu(config_type *config){
    switch(config->rom_choice){
        case(BLITZ):{
            lcd.printString("Blitz", 27, 0);
            lcd.printString("Emu Type:  Any", 0, 1);
            lcd.printString("Controls:5 to ", 0, 2);
            lcd.printString("start And bomb", 0, 3);
            lcd.printString(" end on crash ", 0, 4);
            lcd.printString(">    BACK     ", 0, 5); 
            break;
        }
        case(BREAKOUT):{
            lcd.printString("   Breakout   ", 0, 0);
            lcd.printString("Emu Type:  Any",0 , 1);
            lcd.printString("Controls: 5 to",0 , 2);
            lcd.printString("start, 4&6 to ",0 , 3);
            lcd.printString("move, 20 balls",0 , 4);
            lcd.printString(">    BACK     ", 0, 5);
            break;
        }
        case(MERLIN):{
            lcd.printString("    Merlin    ", 0, 0);
            lcd.printString("Emu Type:  Any", 0, 1);
            lcd.printString("  Controls:  ",0 , 2);
            lcd.printString("   4 5 7 8   ",0 , 3);
            lcd.printString("to select tile",0 , 4);
            lcd.printString(">    BACK     ", 0, 5);
            break;
        }
        case(TETRIS):{
            lcd.printString("    Tetris    ", 0, 0);
            lcd.printString("Emu Type:  Any", 0, 1);
            lcd.printString("Controls: 5&6 ",0 , 2);
            lcd.printString("left, right 4 ",0 , 3);
            lcd.printString("  to rotate  ",0 , 4);
            lcd.printString(">    BACK     ", 0, 5);
            break;
        }
        case(SI):{
            lcd.printString("Space Invaders", 0, 0);
            lcd.printString("Emu Type:Amiga", 0, 1);
            lcd.printString("Controls: 4&6 ",0 , 2);
            lcd.printString("left, right 5 ",0 , 3);
            lcd.printString("   to shoot   ",0 , 4);
            lcd.printString(">    BACK     ", 0, 5);
            break;
        }
        case(WALL):{
            lcd.printString("     Wall     ", 0, 0);
            lcd.printString("Emu Type:  Any", 0, 1);
            lcd.printString("Controls: 1 up",0 , 2);
            lcd.printString("    2 down    ",0 , 3);
            lcd.printString(">    BACK     ", 0, 5);
            break;
        }
    }
}

void how_screen(config_type* config, chip8_type* chip8){
    lcd.clear();
    how_menu(config);
    lcd.refresh();

    int current_bank = 5;
    bool how_select = false;
    

    while(!how_select){
        button_input(&how_select);
        lcd.clear();
        how_menu(config);
        lcd.refresh();
    }

    game_select_screen(config, chip8);
}


void game_select_screen(config_type *config, chip8_type* chip8){
    lcd.clear();
    lcd.printChar('>', 6, 2);
    game_selection_menu(config);
    lcd.refresh();

    int current_bank = 2;
    bool game_select = false;
    

    while(!game_select){
        menu_input(&current_bank, GAME, config);
        button_input(&game_select);
        lcd.clear();
        game_selection_menu(config);
        lcd.printChar('>', 6, current_bank);
        lcd.refresh();
        ThisThread::sleep_for(50ms);
    }

     switch(current_bank){
        case(2):{game_set(config, chip8); break;}
        case(3):{how_screen(config, chip8); break;}
        case(4):{main_screen(config, chip8); break;}
        default:{break;}
     }   
}

void settings_menu(){

    lcd.printString("SETTINGS", 18, 0);
    lcd.printString("Screen", 24, 2);
    lcd.printString("Audio", 27, 3);
    lcd.printString("Emulation", 15, 4);
    lcd.printString("BACK", 30, 5);

}

void screen_menu(config_type* config){

    lcd.printString("SCREEN", 24, 0);
    lcd.printString("Cst: ", 18, 2);
    lcd.printString("Brt: ", 18, 3);
    lcd.printString("BACK", 30, 4);

    sprintf(str_buffer, "%.1f", config->contrast);
    lcd.printString(str_buffer, 48 , 2);
    sprintf(str_buffer, "%.1f", config->brightness);
    lcd.printString(str_buffer, 48, 3);

}

void audio_menu(config_type* config){

    lcd.printString("AUDIO", 32, 0);
    lcd.printString("Vol: ", 18, 2);
    lcd.printString("Freq:", 12, 3);
    lcd.printString("BACK", 30, 4);

    sprintf(str_buffer, "%.1f", config->volume);
    lcd.printString(str_buffer, 48 , 2);

    sprintf(str_buffer, "%d", config->freq);
    lcd.printString(str_buffer, 54 , 3);


}

void emu_menu(config_type* config){

    lcd.printString("EMULATION", 15, 0);
    lcd.printString("Type: ", 12, 2);
    lcd.printString("Clk:", 12, 3);
    lcd.printString("BACK", 30, 4);

    switch(config->emu_choice){
        case(COSMAC):{lcd.printString("COSMAC", 48, 2); break;}
        case(AMIGA):{lcd.printString("AMIGA", 48, 2); break;}
    }

    sprintf(str_buffer, "%d", config->clk_speed);
    lcd.printString(str_buffer, 54 , 3);

}

void audio_screen(menu_type menu, config_type *config, chip8_type* chip8){
    lcd.clear();
    lcd.printChar('>', 6, 2);
    audio_menu(config);
    lcd.refresh();

    int current_bank = 2;
    bool audio_select = false;
    

    while(!audio_select){
        menu_input(&current_bank, AUDIO, config);
        button_input(&audio_select);
        if(audio_select == true && current_bank != 4){audio_select = false;}
        lcd.clear();
        audio_menu(config);
        lcd.printChar('>', 6, current_bank);
        lcd.refresh();

        
        ThisThread::sleep_for(50ms);
    }

    switch(current_bank){
        case(4):{settings_screen(menu, config, chip8); break;}
        default:{break;}
    }
}

void emu_screen(menu_type menu, config_type *config, chip8_type *chip8){
    lcd.clear();
    lcd.printChar('>', 6, 2);
    emu_menu(config);
    lcd.refresh();

    int current_bank = 2;
    bool emu_select = false;
    

    while(!emu_select){
        menu_input(&current_bank, EMU, config);
        button_input(&emu_select);
        if(emu_select == true && current_bank != 4){emu_select = false;}
        lcd.clear();
        emu_menu(config);
        lcd.printChar('>', 6, current_bank);
        lcd.refresh();

        
        ThisThread::sleep_for(50ms);
    }

    switch(current_bank){
        case(4):{settings_screen(menu, config, chip8); break;}
        default:{break;}
    }
}


void screen_set_screen(menu_type menu, config_type *config, chip8_type* chip8){
    lcd.clear();
    lcd.printChar('>', 6, 2);
    screen_menu(config);
    lcd.refresh();

    int current_bank = 2;
    bool screen_select = false;
    

    while(!screen_select){
        menu_input(&current_bank, SCREEN, config);
        button_input(&screen_select);
        if(screen_select == true && current_bank != 4){screen_select = false;}
        lcd.clear();
        screen_menu(config);
        lcd.printChar('>', 6, current_bank);
        lcd.setContrast(config->contrast);
        lcd.setBrightness(config->brightness);
        lcd.refresh();

        
        ThisThread::sleep_for(50ms);
    }

    switch(current_bank){
        case(4):{settings_screen(menu, config, chip8); break;}
        default:{break;}
    }
}


void return_main(chip8_type* chip8, config_type* config){
    memset(chip8->ram + entry, 0, sizeof(chip8->ram) - entry);
    memset(chip8->display, 0, sizeof(chip8->display));
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memset(chip8->V, 0, sizeof(chip8->V));
    chip8->I = 0;
    chip8->pc = 0;
    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    memset(chip8->keypad, false, sizeof(chip8->keypad));
    chip8->draw = false;

    config->rom_choice = BLITZ;
    chip8->state = QUIT;

}


void button_input(bool* select){
    int but_state = but.read();
    ThisThread::sleep_for(200ms);
    switch(but_state){
        case(0):{*select = true; break;}
        case(1):{*select = false; break;} //case pressed, forexample settings menu 
    }
}

void pause_screen(config_type* config, chip8_type* chip8){
    lcd.clear();
    lcd.printChar('>', 9, 2);
    pause_menu();
    lcd.refresh();



    int current_bank = 2;
    bool pause_select = false;

    while(g_button_flag == 0){
        menu_input(&current_bank, PAUSE, config);
        button_input(&pause_select);
        if(pause_select == true){break;}
        lcd.clear();
        pause_menu();
        lcd.printChar('>', 9, current_bank);
        lcd.refresh();
        
        ThisThread::sleep_for(50ms);
    }

    switch(pause_select){
            case(true):{
                switch (current_bank){
                case(2):{settings_screen(PAUSE, config, chip8); break;}
                case(3):{return_main(chip8, config); break;}
                case(4):{reset(chip8); break;}
                default:{break;}
                }
                case(false):{break;}
            }
        }



    lcd.clear();
}



void settings_screen(menu_type menu, config_type* config, chip8_type* chip8){
    lcd.clear();
    lcd.printChar('>', 9, 2);
    settings_menu();
    lcd.refresh();

    int current_bank = 2;
    bool settings_select = false;

    while(!settings_select){
        menu_input(&current_bank, PAUSE, config);
        button_input(&settings_select);
        lcd.clear();
        settings_menu();
        lcd.printChar('>', 9, current_bank);
        lcd.refresh();
        
        ThisThread::sleep_for(50ms);
    }


    switch(menu){
        case(PAUSE):{
            switch(current_bank){
                case(2):{screen_set_screen(PAUSE, config, chip8); break;}
                case(3):{audio_screen(PAUSE, config, chip8); break;}
                case(4):{emu_screen(PAUSE, config, chip8); break;}
                case(5):{pause_screen(config, chip8); break;}
                default:{break;}
            }
            break;
        }
        case(MAIN):{
             switch(current_bank){
                case(2):{screen_set_screen(MAIN, config, chip8); break;}
                case(3):{audio_screen(MAIN, config, chip8); break;}
                case(4):{emu_screen(MAIN, config, chip8); break;}
                case(5):{main_screen(config, chip8); break;}
                default:{break;}
            }
            break;
        }
        default:{break;}
    }
    


}

void main_screen(config_type* config, chip8_type *chip8){
    lcd.clear();
    lcd.printChar('>', 9, 2);
    main_menu();
    lcd.refresh();

    int current_bank = 2;
    bool main_select = false;

    while(!main_select){
        menu_input(&current_bank, MAIN, config);
        button_input(&main_select);
        lcd.clear();
        main_menu();
        lcd.printChar('>', 9, current_bank);
        lcd.refresh();
        
        ThisThread::sleep_for(50ms);
    }


    switch(current_bank){
        case(3):{settings_screen(MAIN, config, chip8); break;}
        case(2):{game_select_screen(config, chip8); break;}
    }
    
}

void game_input(chip8_type *chip8){
    

    inputs pressed_input = keypad.get_key_pressed();
    

    switch(pressed_input){
        case(One):{chip8->keypad[0x1] = true; break;}
        case(Two):{chip8->keypad[0x2] = true; break;}
        case(Three):{chip8->keypad[0x3] = true; break;}
        case(C_key):{chip8->keypad[0xC] = true; break;}

        case(Four):{chip8->keypad[0x4] = true; break;}
        case(Five):{chip8->keypad[0x5] = true; break;}
        case(Six):{chip8->keypad[0x6] = true; break;}
        case(D_key):{chip8->keypad[0xD] = true; break;}

        case(Seven):{chip8->keypad[0x7] = true;  break;}
        case(Eight):{chip8->keypad[0x8] = true; break;}
        case(Nine):{chip8->keypad[0x9] = true; break;}
        case(E_key):{chip8->keypad[0xE] = true; break;}

        case(A_key):{chip8->keypad[0xA] = true;  break;}
        case(Zero):{chip8->keypad[0x0] = true; break;}
        case(B_key):{chip8->keypad[0xB] = true; break;}
        case(F_key):{chip8->keypad[0xF] = true; break;}

        default:{break;}
    }

    for(int i = 0; i < 4; i++){
        for(int j = 0; j < 4; j++){
            inputs released_input = keypad.get_released_key(i, j);
            switch(released_input){
                case(One):{chip8->keypad[0x1] = false; break;}
                case(Two):{chip8->keypad[0x2] = false; break;}
                case(Three):{chip8->keypad[0x3] = false; break;}
                case(C_key):{chip8->keypad[0xC] = false; break;}

                case(Four):{chip8->keypad[0x4] = false;  break;}
                case(Five):{chip8->keypad[0x5] = false; break;}
                case(Six):{chip8->keypad[0x6] = false; break;}
                case(D_key):{chip8->keypad[0xD] = false; break;}

                case(Seven):{chip8->keypad[0x7] = false; break;}
                case(Eight):{chip8->keypad[0x8] = false; break;}
                case(Nine):{chip8->keypad[0x9] = false; break;}
                case(E_key):{chip8->keypad[0xE] = false; break;}

                case(A_key):{chip8->keypad[0xA] = false; break;}
                case(Zero):{chip8->keypad[0x0] = false; break;}
                case(B_key):{chip8->keypad[0xB] = false; break;}
                case(F_key):{chip8->keypad[0xF] = false; break;}

                default:{break;}

            }
        }
    }
}




int main(){
    stick.init();
    lcd.init(LPH7366_1);
    config_type *config = static_cast<config_type *>(malloc(sizeof(config_type)));
    init_config(config); 


    chip8_type *chip8 = static_cast<chip8_type *>(malloc(sizeof(chip8_type)));
    chip8->stkptr = static_cast<uint16_t *>(malloc(sizeof(uint16_t)));
    chip8->stkptr = nullptr;
    init_chip8(chip8, config);




    lcd.setContrast(0.55);      //set contrast to 55%
    lcd.setBrightness(0.5);     //set brightness to 50% (utilises the PWM)
    lcd.clear();

    joystick_button.fall(&isr);
    joystick_button.mode(PullUp);
    user_led = state;




    

    main_screen(config, chip8);
    g_button_flag = 0;

    while(chip8->state != OFF){
        if(g_button_flag){
            ThisThread::sleep_for(200ms);
            g_button_flag = 0;
            state = !state;
            user_led = state;
            switch(chip8->state){
                case(RUNNING):{chip8->state = PAUSED; break;}
                case(PAUSED):{chip8->draw = true; draw(chip8, config); chip8->state = RUNNING; break;}
                case(QUIT):{break;}
            }
        }

        switch(chip8->state){
            case(PAUSED):{
                pause_screen(config, chip8); 
                break;
            }
            case(RUNNING):{
                game_input(chip8);
                
                t.start();

                for(int i = 0; i < config->insts_per_sec / config->clk_speed; i++){
                    emulate(chip8,config);
                }

                t.stop();

                const std::chrono::milliseconds elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(t.elapsed_time());
                const int duration_ms = static_cast<int>(std::ceil(1.0 / (float)config->clk_speed * 1000));

                std::chrono::milliseconds emu_timing(duration_ms);

                ThisThread::sleep_for(emu_timing > elapsed_time ? emu_timing - elapsed_time : std::chrono::milliseconds(0));


                if(chip8->draw){
                    draw(chip8, config); 
                    chip8->draw = false;
                }
                update_timers(chip8, config);
                break;
            }
            case(QUIT):{
                main_screen(config, chip8);
                break;
            }
        }
        
    }

    free(config);
    free(chip8->stkptr);
    free(chip8);

    end();

    return 0;
}








/*
Implement DXYN


- Speakers
    - PWM controlled
    - Play a tone when device is turned on/off
    - Play a tone for the games running 


- Turn off and on the device - Interrupt

*/

