#include "mbed.h"
#include "N5110.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmath>
#include "ibm-logo.h"
#include "pong.h"
#include "Joystick.h"




N5110 lcd(PC_7, PA_9, PB_10, PB_5, PB_3, PA_10);
InterruptIn joystick_button(A5);
Joystick stick(ARDUINO_UNO_A3, ARDUINO_UNO_A4);
DigitalIn but(BUTTON1);
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
    PONG
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

typedef enum{
    MAIN,
    PAUSE,
    SETTINGS
}menu_type;

char str_buffer[14] = {0};

void main_screen();
void settings_screen(menu_type menu);

void init_config(config_type *config){
    config->emu_choice = AMIGA;
    config->rom_choice = PONG;
    config->sf_x = 1;
    config->sf_y = 1;
    config->res_x = 64;
    config->res_y = 32;
    config->bg_colour = FILL_BLACK;
    config->fg_colour = FILL_WHITE;
    config->insts_per_sec = 700;
}

void init_chip8(chip8_type *chip8, const config_type* config){
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
    switch(config->rom_choice){
        case(IBM):{memcpy(chip8->ram + entry, ibm_data_src, sizeof(ibm_data_src)); break;}
        case(PONG):{memcpy(chip8->ram + entry, pong_src, sizeof(pong_src)); break;}
    }
    


    chip8->pc = entry;
    chip8->state = RUNNING;
    chip8->stkptr = &chip8->stack[0];

   

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

void menu_input(int* current_bank, menu_type state){
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
            default:{break;}
        }

}

void pause_menu(){
    const char pause_l[] = "PAUSED";
    const char settings_l[] = "Settings";
    const char main_menu_l[] = "Main Menu";
    const char rst_l[] = "Reset";
    const char power_off_l[] = "Power OFF";

    memcpy(str_buffer, pause_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 24, 0);

    memcpy(str_buffer, settings_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 18, 2);

    memcpy(str_buffer, main_menu_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 15, 3);

    memcpy(str_buffer, rst_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 27, 4);

    memcpy(str_buffer, power_off_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 15, 5);
}

void button_input(bool* select){
    int but_state = but.read();
    switch(but_state){
        case(0):{*select = 1; break;}
        case(1):{*select = 0; break;} //case pressed, forexample settings menu 
    }
}

void pause_screen(){
    printf("In Pause Menu\n");
    lcd.clear();
    lcd.printChar('>', 9, 2);
    pause_menu();
    lcd.refresh();



    int current_bank = 2;
    bool select = 0;

    while(g_button_flag == 0){
        if(select == true){break;};
        button_input(&select);
        menu_input(&current_bank, PAUSE);
        lcd.clear();
        pause_menu();
        lcd.printChar('>', 9, current_bank);
        lcd.refresh();
        
        ThisThread::sleep_for(200ms);
    }

    
    switch(select){
        case(false):{break;}
        case(true):{
            switch(current_bank){
                case(2):{settings_screen(PAUSE); break;}
                default:{break;}
            }

        }
    }


    lcd.clear();
}

void main_menu(){
    const char title_l[] = "RAHUL'S C8";
    const char start_l[] = "START";
    const char settings_l[] = "Settings";
    const char power_off_l[] = "Power OFF";

    memcpy(str_buffer, title_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 12, 0);

    memcpy(str_buffer, start_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 27, 2);

    memcpy(str_buffer, settings_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 18, 3);

    memcpy(str_buffer, power_off_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 15, 4);

}

void settings_menu(){
    const char title_l[] = "SETTINGS";
    const char screen_l[] = "Screen";
    const char audio_l[] = "Audio";
    const char emulation_l[] = "Emulation";
    const char back_l[] = "BACK";

    memcpy(str_buffer, title_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 18, 0);

    memcpy(str_buffer, screen_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 24, 2);

    memcpy(str_buffer, audio_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 27, 3);

    memcpy(str_buffer, emulation_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 15, 4);

    memcpy(str_buffer, back_l, sizeof(str_buffer));
    lcd.printString(str_buffer, 30, 5);

}


void settings_screen(menu_type menu){
    printf("In Settings Menu\n");
    printf("Menu type passed: %s \n", menu ? "PAUSE" : "MAIN");
    lcd.clear();
    lcd.printChar('>', 9, 2);
    settings_menu();
    lcd.refresh();

    int current_bank = 2;
    bool select = false;

    while(!select){
        menu_input(&current_bank, PAUSE);
        button_input(&select);
        lcd.clear();
        settings_menu();
        lcd.printChar('>', 9, current_bank);
        lcd.refresh();
        
        ThisThread::sleep_for(200ms);
    }

    printf("Current Bank: %d\n", current_bank);
    printf("Select: %s\n", select ? "TRUE" : "FALSE" );
    printf("Menu type after settings while loop: %s \n", menu ? "PAUSE" : "MAIN");
    

    switch(menu){
        case(PAUSE):{
            switch(current_bank){
                case(5):{pause_screen(); break;}
                default:{break;}
            }
        }
        case(MAIN):{
             switch(current_bank){
                case(5):{main_screen(); break;}
                default:{break;}
            }
        }
        default:{break;}
    }
    


}

void main_screen(){
    printf("In main menu\n");
    lcd.clear();
    lcd.printChar('>', 9, 2);
    main_menu();
    lcd.refresh();

    int current_bank = 2;
    bool select = false;

    while(!select){
        menu_input(&current_bank, MAIN);
        button_input(&select);
        lcd.clear();
        main_menu();
        lcd.printChar('>', 9, current_bank);
        lcd.refresh();
        
        ThisThread::sleep_for(200ms);
    }


    switch(current_bank){
        case(3):{settings_screen(MAIN); break;}
        case(2):{break;}
    }
    
}




int main(){
    stick.init();
    lcd.init(LPH7366_1);
    lcd.setContrast(0.55);      //set contrast to 55%
    lcd.setBrightness(0.5);     //set brightness to 50% (utilises the PWM)
    lcd.clear();

    joystick_button.fall(&isr);
    joystick_button.mode(PullUp);
    user_led = state;

    config_type *config = static_cast<config_type *>(malloc(sizeof(config_type)));
    init_config(config); 

    chip8_type *chip8 = static_cast<chip8_type *>(malloc(sizeof(chip8_type)));
    chip8->stkptr = static_cast<uint16_t *>(malloc(sizeof(uint16_t)));
    chip8->stkptr = nullptr;
    init_chip8(chip8, config);

    main_screen();

    while(chip8->state != QUIT){
        if(g_button_flag){
            g_button_flag = 0;
            state = !state; 
            user_led = state;
            switch(chip8->state){
                case(RUNNING):{chip8->state = PAUSED; printf("PAUSED\n"); break;}
                case(PAUSED):{chip8->draw = true; draw(chip8, config); chip8->state = RUNNING; printf("RUNNING\n"); break;}
                case(QUIT):{printf("error"); break;}
            }
        }
        switch(chip8->state){
            case(PAUSED):{
                pause_screen(); 
                continue;
            }
            case(RUNNING):{
                
                t.start();

                for(int i = 0; i < config->insts_per_sec / 60; i++){
                    emulate(chip8,config);
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
                break;
            }
            case(QUIT):{break;}
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

Need to make this compatible with the microcontroller:

- Speakers
    - PWM controlled
    - Play a tone when device is turned on/off
    - Play a tone for the games running 


- Inputs
    - Designed button circuit using an LPF and schmitt trigger
    - Need to hook up to oscilloscope and check waveform produced so the microcontroller can read from the design
    - Program button functionality - Need to detect button being held down


-Create Main menu for Device boot
    -A way for the user to select roms
    -Explain how to play game


- Create a Pause menu - Interrupt
    -Pause/resume Emulator - Interrupt for when the emulator is running
    - A way for the user to reset the device so they can load a new rom - Polling
    - Turn up and down volume - Polling


- Turn off and on the device - Interrupt

*/

/*
- main -> start -> paused -> settings -> paused -> main - should go back into game not main








*/