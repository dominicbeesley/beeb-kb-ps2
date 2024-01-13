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

void ps2c_init(struct ps2c *p, uint _pin_clk, uint _pin_dat) {
	p->pin_clk = _pin_clk;
	p->pin_dat = _pin_dat;

	gpio_init(p->pin_clk);
	gpio_init(p->pin_dat);
	gpio_set_pulls(p->pin_clk, 1, 0);
	gpio_set_pulls(p->pin_dat, 1, 0);

	HI(p->pin_clk);
	HI(p->pin_dat);
}

int ps2c_write_bit(struct ps2c *p, uint8_t b) {
	
	SET(p->pin_dat, b);
	busy_wait_us(CLKHALF);
	// device sends on falling clock
	LO(p->pin_clk);	// start bit
	busy_wait_us(CLKFULL);
	HI(p->pin_clk);
	busy_wait_us(CLKHALF);

	if (!gpio_get(p->pin_clk))
	{
		HI(p->pin_dat);
		return PS2C_ERR_INT;
	}
	return 0;
}

int ps2c_write(struct ps2c *p, uint8_t data) {

	busy_wait_us(CLKHALF);

	// check for clock being held low by host

	if (!gpio_get(p->pin_clk))
		return PS2C_ERR_INH;

	// check for data being held low by host
	if (!gpio_get(p->pin_dat))
		return PS2C_ERR_REQ;

	busy_wait_us(BYTEWAIT);

	// check for clock being held low by host

	if (!gpio_get(p->pin_clk))
		return PS2C_ERR_INH;

	// check for data being held low by host
	if (!gpio_get(p->pin_dat))
		return PS2C_ERR_REQ;

	
	int r;
	r = ps2c_write_bit(p, 0);	// send start bit
	if (r)
		return PS2C_ERR_INH; // just an inhibit dont need a full retransmit?

	uint8_t par = 1; // odd parity
	for (int x = 0; x < 8; x++) {
		r = ps2c_write_bit(p, data & 1);
		if (r) return r;
		par += (data & 1);
		data >>= 1;		
	}

	//parity bit
	r = ps2c_write_bit(p, par & 1);
	if (r) return r;

	//stop bit
	r = ps2c_write_bit(p, 1);
	//if (r) return r; - check I think this can be ignored?

	return 0;

}

uint8_t ps2c_readbit(struct ps2c *p) {
	LO(p->pin_clk);
	busy_wait_us(CLKFULL);
	HI(p->pin_clk);
	busy_wait_us(CLKFULL);
	return gpio_get(p->pin_dat)?0xFF:0;	
}

int ps2c_read(struct ps2c *p, uint8_t *c) {

	//belt and braces - reset clk/dat to idle
	HI(p->pin_clk);
	HI(p->pin_dat);

	busy_wait_us(CLKFULL); // give time for pull ups

	//we may have entered either expecting/wanting something from host
	//or the host may already have pulled data/clock low

	int i;

	if (!gpio_get(p->pin_clk)) {
		//clock already pulled low wait for it to be release and check data is low for start
		while (!gpio_get(p->pin_clk)) {
			busy_wait_us(CLKHALF);
		}
		if (gpio_get(p->pin_dat)) {
			return PS2C_ERR_HFE;
		}
		// we've not had the start bit - start reading...
	} else {
		//we're expecting something but timeout if we don't get it
		i = (TIMEOUT * 1000) / CLKFULL;
		while (gpio_get(p->pin_clk) && i > 0) { i --; }
		if (i <0)
			return PS2C_ERR_TO;
		if (gpio_get(p->pin_dat)) {
			return PS2C_ERR_HFE;
		}
	}

	busy_wait_us(CLKFULL);

	printf("HOST:GO...\n");

	*c = 0;
	uint8_t par = 1;

	for (int i = 0; i < 8; i++) {
		uint8_t b = ps2c_readbit(p)?0xFF:0;
		*c = (*c >> 1) | (b & 0x80);
		par += (b & 1);
	}
	uint8_t b = ps2c_readbit(p)?1:0;
	par += b;

	//stop
	LO(p->pin_clk);
	busy_wait_us(CLKFULL);
	HI(p->pin_clk);
	busy_wait_us(CLKFULL);

	// ack
	SET(p->pin_dat, 0);
	LO(p->pin_clk);	// start bit
	busy_wait_us(CLKFULL);
	HI(p->pin_clk);
	HI(p->pin_dat);

	if (par & 1)
		return PS2C_ERR_PAR;
	else
		return 0;
}

// call this once per millisecond or so to check for host commands
int ps2c_tick(struct ps2c *p, uint8_t *cmd) {

	//belt and braces - reset clk/dat to idle
	HI(p->pin_clk);
	HI(p->pin_dat);

	busy_wait_us(CLKFULL); // give time for pull ups

	if (gpio_get(p->pin_dat) && gpio_get(p->pin_clk))
		return 0; // host not requesting anything

	int r = ps2c_read(p, cmd);
    if (r) { 
        printf("TICK ERR:%d %02X\n", r, (int)*cmd); 
        return r; 
    }
	
	return PS2C_ERR_REQ;	// signal a request
}



