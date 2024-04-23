#include "mbed.h"
#include "keypad.h"

Keypad::Keypad(PinName col3, PinName col2, PinName col1, PinName col0, PinName row0, PinName row1, PinName row2, PinName row3)
    : row_inputs{row0, row1, row2, row3},
      col_outputs{col0, col1, col2, col3} {
          row_inputs[0].mode(PullUp);
          row_inputs[1].mode(PullUp);
          row_inputs[2].mode(PullUp);
          row_inputs[3].mode(PullUp);
          

          memset(prev_key_states, IDLE, sizeof(prev_key_states));
          
}

inputs Keypad::get_key_pressed(){
    inputs pressed_key = No_Key;

    // Iterate over each column
    for(int cols = 0; cols < 4; cols++){
        col_outputs[cols] = 0; // Activate current column

        // Iterate over each row
        for(int rows = 0; rows < 4; rows++){
            // Check if a key is pressed and it wasn't detected as pressed in the previous iteration
            if(!row_inputs[rows].read()){
                if(prev_key_states[rows][cols] == IDLE || prev_key_states[rows][cols] == RELEASED){pressed_key = keys[rows][cols]; prev_key_states[rows][cols] = PRESSED;}
                pressed_key = keys[rows][cols]; // Store the pressed key
                ThisThread::sleep_for(50ms);
            }
            else {
                prev_key_states[rows][cols] = RELEASED; // Update the state of the key as not pressed
            }
        }

        col_outputs[cols] = 1; // Deactivate current column
    }

    return pressed_key; // Return the pressed key
}

inputs Keypad::get_released_key(int rows, int cols){
    if(prev_key_states[rows][cols] == RELEASED){return keys[rows][cols];}
    return No_Key;
}





