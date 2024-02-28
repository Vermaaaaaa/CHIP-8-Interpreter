#include "mbed.h"
#include "N5110.h"
#include <chrono>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmath>
#include "chip8-logo.h" 
#include "ibm-logo.h"



N5110 lcd(PC_7, PA_9, PB_10, PB_5, PB_3, PA_10);
InterruptIn joystick_button(A5);
DigitalOut user_led(LED1);

volatile int g_button_flag = 0;
int state = 0;

Timer t;

typedef enum{
    COSMAC,
    AMIGA, 
}emu_type;

typedef enum{
    IBM,
    TIMENDUS
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
    instr_type inst;
    bool draw;
} chip8_type;

char buffer[14] = {0};

void init_chip8(chip8_type *chip8, const config_type config){
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



    uint8_t *timendus_data = static_cast<uint8_t *>(malloc(104*sizeof(uint8_t)));
    uint8_t *ibm_data = static_cast<uint8_t *>(malloc(84*sizeof(uint8_t)));


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
    switch(config.rom_choice){
        case(TIMENDUS):{memcpy(chip8->ram + entry, timendus_data_src, sizeof(timendus_data_src)); break;}
        case(IBM):{memcpy(chip8->ram + entry, ibm_data_src, sizeof(ibm_data_src)); break;}
    }
    


    chip8->pc = entry;
    chip8->state = RUNNING;
    chip8->stkptr = &chip8->stack[0];

    free(timendus_data);
    free(ibm_data);

     //Success
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

void update_timers(chip8_type *chip8){
    if(chip8->delay_timer > 0){chip8->delay_timer--;}

    if(chip8->sound_timer > 0){chip8->sound_timer--;}
}

void draw(chip8_type *chip8, const config_type config){
    lcd.clear();
    // Loop through display pixels, draw a rectangle per pixel to the screen
    for (uint32_t i = 0; i < sizeof chip8->display; i++) {
        const unsigned int x0 = 10 + (i % config.res_x) * config.sf_x;
        const unsigned int y0 = 8 + (i / config.res_x) * config.sf_y;
        if (chip8->display[i]) {
            // Pixel is on, draw foreground color
            lcd.drawRect(x0, y0, config.sf_x, config.sf_y, config.fg_colour);
        } 
        else {
            lcd.drawRect(x0, y0, config.sf_x, config.sf_y, config.bg_colour);
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
        sprintf(buffer, "       %d", i);
        lcd.printString(buffer, 0, 2);
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


int main(){
    lcd.init(LPH7366_1);
    lcd.setContrast(0.55);      //set contrast to 55%
    lcd.setBrightness(0.5);     //set brightness to 50% (utilises the PWM)
    lcd.clear();

    joystick_button.fall(&isr);
    joystick_button.mode(PullUp);
    user_led = state;

    config_type config = {AMIGA, FILL_BLACK, FILL_WHITE, 64, 32, 700, 1, 1, IBM};

    chip8_type *chip8 = static_cast<chip8_type *>(malloc(sizeof(chip8_type)));
    chip8->stkptr = static_cast<uint16_t *>(malloc(sizeof(uint16_t)));
    chip8->stkptr = nullptr;
    init_chip8(chip8, config);


    while(chip8->state != QUIT){
        if(g_button_flag){
            g_button_flag = 0;
            state = !state; 
            user_led = state;
            switch(chip8->state){
                case(RUNNING):{chip8->state = PAUSED; break;}
                case(PAUSED):{chip8->state = RUNNING; break;}
                case(QUIT):{printf("error"); break;}
            }
        }
        if(chip8->state  == PAUSED){continue;}

        t.start();

        for(int i = 0; i < config.insts_per_sec / 60; i++){
            emulate(chip8,&config);
        }

        t.stop();

        const std::chrono::milliseconds elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(t.elapsed_time());
        const std::chrono::milliseconds emu_timing = 17ms;


        ThisThread::sleep_for(emu_timing > elapsed_time ? emu_timing - elapsed_time : std::chrono::milliseconds(0));


        if(chip8->draw){
            draw(chip8, config); 
            chip8->draw = false;
        }
        update_timers(chip8);
        
    }

    free(chip8);
    free(chip8->stkptr);

    end();

    return 0;
}








/*
Implement DXYN

Need to make this compatible with the microcontroller:

- Speakers 
- Inputs
- A way for the user to select roms 
- Turn up and down volume 

- A way for the user to reset the device so they can load a new rom - Interrupt
- Turn off and on the device - Interrupt
- Pause/resume Emulator - Interrupt
- Handled to an extent

- Screen - Either distored image or not use the full screen but centre the 2 images
*/