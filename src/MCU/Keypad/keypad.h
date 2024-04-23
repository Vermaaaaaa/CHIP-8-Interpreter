#ifndef KEYPAD_H
#define KEYPAD_H

#include "mbed.h"


typedef enum{
    One,
    Two,
    Three,
    C_key,
    Four,
    Five,
    Six,
    D_key,
    Seven,
    Eight,
    Nine,
    E_key,
    A_key,
    Zero,
    B_key,
    F_key,
    No_Key
}inputs;

typedef enum{
    PRESSED,
    HELD,
    RELEASED,
    IDLE
}states;


class Keypad{
    public:
        Keypad(PinName col3, PinName col2, PinName col1, PinName col0, PinName row0, PinName row1, PinName row2, PinName row3);
        ~Keypad();
        inputs get_key_pressed();
        inputs get_released_key(int rows, int cols);
    private:
        DigitalIn row_inputs[4];
        DigitalOut col_outputs[4];
        states prev_key_states[4][4];
        const inputs keys[4][4]{
            {One, Two, Three, C_key},
            {Four, Five, Six, D_key},
            {Seven, Eight, Nine, E_key},
            {A_key, Zero, B_key, F_key}
        };
};


#endif