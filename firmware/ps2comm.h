#ifndef __PS2COMM_H__
#define __PS2COMM_H__

void ps2c_init(uint pin_clk, uint pin_dat);

// call this once per millisecond
int ps2c_tick(void); 

int ps2c_keydown(uint16_t code);
int ps2c_keyup(uint16_t code);

uint8_t ps2c_getleds(void);

#endif