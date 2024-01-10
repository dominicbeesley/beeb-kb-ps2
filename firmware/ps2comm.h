#ifndef __PS2COMM_H__
#define __PS2COMM_H__

void ps2c_init(uint pin_clk, uint pin_dat);

// call this once per millisecond
int ps2c_tick(void); 

int ps2c_keydown(uint16_t code);
int ps2c_keyup(uint16_t code);

uint8_t ps2c_getleds(void);

// The host was inhibiting a write - nothing was sent
#define PS2C_ERR_INH -1

// The host interrupted a write - the packet should be resent
#define PS2C_ERR_INT -2

// The host is requesting a read - data was held low, clock high
#define PS2C_ERR_REQ -3

// The host sent a bad packet - parity error
#define PS2C_ERR_PAR -4

// The host didn't send an expected response in time
#define PS2C_ERR_TO -5

// The host didn't send a stop bit
#define PS2C_ERR_FE -6

// The host didn't signal a start bit after pulling clock low
#define PS2C_ERR_HFE -7

#endif