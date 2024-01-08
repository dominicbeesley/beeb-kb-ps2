#include <stdio.h>

#include "pico/stdlib.h"
#include "ps2comm.h"
#include "hardware/timer.h"


// adapted from https://github.com/Harvie/ps2dev/blob/master/src/ps2dev.cpp

//since for the device side we are going to be in charge of the clock,
//the two defines below are how long each _phase_ of the clock cycle is
#define CLKFULL 40
// we make changes in the middle of a phase, this how long from the
// start of phase to the when we drive the data line
#define CLKHALF 20

// DB: I've changed this to be as per spec and done a proper test at start
// for host holding clock low
#define BYTEWAIT 50

// Timeout if computer not sending for 30ms
#define TIMEOUT 30

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

uint pin_clk;
uint pin_dat;

uint8_t leds;

void HI(uint pin) {
	gpio_set_dir(pin, GPIO_IN);
	//gpio_put(pin, 1);
}

void LO(uint pin) {
	gpio_put(pin, 0);
	gpio_set_dir(pin, GPIO_OUT);
}

void SET(uint pin, uint8_t v) {
	if (v)
		HI(pin);
	else
		LO(pin);
}

void ps2c_init(uint _pin_clk, uint _pin_dat) {
	pin_clk = _pin_clk;
	pin_dat = _pin_dat;

	gpio_init(pin_clk);
	gpio_init(pin_dat);
	gpio_set_pulls(pin_clk, 1, 0);
	gpio_set_pulls(pin_dat, 1, 0);

	HI(pin_clk);
	HI(pin_dat);
	leds = 0xFF;
}

int write_bit(uint8_t b) {
	
	SET(pin_dat, b);
	busy_wait_us(CLKHALF);
	// device sends on falling clock
	LO(pin_clk);	// start bit
	busy_wait_us(CLKFULL);
	HI(pin_clk);
	busy_wait_us(CLKHALF);

	if (!gpio_get(pin_clk))
	{
		HI(pin_dat);
		return PS2C_ERR_INT;
	}
	return 0;
}

uint8_t last_data;

int write(uint8_t data) {

	busy_wait_us(CLKHALF);

	// check for clock being held low by host

	if (!gpio_get(pin_clk))
		return PS2C_ERR_INH;

	// check for data being held low by host
	if (!gpio_get(pin_dat))
		return PS2C_ERR_REQ;

	busy_wait_us(BYTEWAIT);

	// check for clock being held low by host

	if (!gpio_get(pin_clk))
		return PS2C_ERR_INH;

	// check for data being held low by host
	if (!gpio_get(pin_dat))
		return PS2C_ERR_REQ;

	
	int r;
	r = write_bit(0);	// send start bit
	if (r)
		return PS2C_ERR_INH; // just an inhibit dont need a full retransmit?

	uint8_t par = 1; // odd parity
	for (int x = 0; x < 8; x++) {
		r = write_bit(data & 1);
		if (r) return r;
		par += (data & 1);
		data >>= 1;		
	}

	//parity bit
	r = write_bit(par & 1);
	if (r) return r;

	//stop bit
	r = write_bit(1);
	//if (r) return r; - check I think this can be ignored?

	return 0;

}

uint8_t readbit() {
	LO(pin_clk);
	busy_wait_us(CLKFULL);
	HI(pin_clk);
	busy_wait_us(CLKFULL);
	return gpio_get(pin_dat)?0xFF:0;	
}

int read(uint8_t *c) {

	//belt and braces - reset clk/dat to idle
	HI(pin_clk);
	HI(pin_dat);

	int i = (TIMEOUT * 1000) / CLKFULL;
	while (gpio_get(pin_dat) && i > 0) { i --; }
	if (i <0)
		return PS2C_ERR_TO;

	*c = 0;
	uint8_t par = 1;

	for (int i = 0; i < 8; i++) {
		uint8_t b = readbit()?0xFF:0;
		*c = (*c >> 1) | (b & 0x80);
		par += (b & 1);
	}
	uint8_t b = readbit()?1:0;
	par += b;

	b = readbit()?1:0;

	write_bit(0); //stop bit ack
	HI(pin_dat);

	if (!b)
		return PS2C_ERR_FE;
	if (par & 1)
		return PS2C_ERR_PAR;
	else
		return 0;
}

#define READ(x) \
	r = read(&x); \
	if (r) { \
		printf("HRDER:%d\n", r); \
		return r; \
	}

// call this once per millisecond
int ps2c_tick(void) {

	//belt and braces - reset clk/dat to idle
	HI(pin_clk);
	HI(pin_dat);

	if (gpio_get(pin_dat) && gpio_get(pin_clk))
		return 0; // host not requesting anything

	printf("HOST:REQ\n");
	while (!gpio_get(pin_clk)) {
		busy_wait_us(BYTEWAIT);
	}

	busy_wait_us(CLKFULL);

	printf("HOST:GO...\n");

	uint8_t c;
	int r;

	READ(c)

	printf("HOST:%02X\n", (int)c);

	uint8_t d;
	switch (c) {
		case 0xFF: //reset
			write(0xFA);	//ack
			write(0xAA);	//ack
			leds = 0;
			printf("RESET\n");
			break;
		case 0xFE: //resend last
			busy_wait_us(BYTEWAIT);
			write(last_data);
			printf("RESEND: %02X\n", (int)last_data);
			break;
		case 0xF5: //disable
			write(0xFA);	//ack
			printf("DISABLE: %02X\n", (int)last_data);
			break;
		case 0xF4: //enable
			write(0xFA);	//ack
			printf("ENABLE: %02X\n", (int)last_data);
			break;
		case 0xF3: //repeat rate - do nothing with this
			write(0xFA);	//acknowledge reset
			READ(d)
			printf("TRATE: %d\n", (int)d);
			break;
		case 0xF2: //identify
			write(0xFA);	//acknowledge reset
			write(0xAB);
			write(0x83);
			printf("ID\n");
			break;
		case 0xF0: //read / set scan code set
			write(0xFA);	//ack
			READ(d)
			printf("SCAN: %d\n", (int)d);
			if (!c)
				write(1);
			break;
		case 0xEE: //echo
			write(0xEE);
			printf("ECHO\n");
			break;
		case 0xED: // set LEDs
			write(0xFA); // ack
			READ(d)
			printf("LEDS: %d\n", (int)d);
			leds = d;
			break;
		default:
			printf("UK:%02X\n", (int)c);
			break;
	}

	return c;
}

int ps2c_keydown(uint16_t code) {
	if (code & 0xFF00) {
		write(code >> 8);
	}
	return write(code);

}
int ps2c_keyup(uint16_t code) {
	if (code & 0xFF00) {
		write(code >> 8);
	}
	int r = write(0xF0);
	if (r) return r;
	return write(code);
}

uint8_t ps2c_getleds(void) {
	return leds;
}
