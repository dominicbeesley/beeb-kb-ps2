#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/sync.h"

#include "ps2comm.h"
#include "beeb-mouse.h"
#include "pins.h"

struct ps2c ps2_ms;
volatile int mouseX, mouseY;
bool stream_enabled;
uint8_t last_b; // button state last time round
int last_mX, last_mY;
int sample_rate;
int sample_counter;

typedef enum {
	IM_PS2,
	IM_03_SCROLL,
	IM_04_SCROLL
} intelli_mode_t;

typedef enum {
	IM_U_IDLE, // no unlock in progress
	IM_U_200, // first "200"
	IM_U_200_2, // second "200"
	IM_U_100
} intelli_unlock_t;

intelli_mode_t mode;
intelli_unlock_t unlock;

void gpio_callback(uint gpio, uint32_t event_mask)
{
    if (gpio == GPIO_AMX_X1) {
        mouseX += gpio_get(GPIO_AMX_X2)?-1:1;
    } else if (gpio == GPIO_AMX_Y1) {
        mouseY += gpio_get(GPIO_AMX_Y2)?-1:1;
    }

}

static int iabs(int x) {
    if (x >= 0)
        return x;
    else
        return -x;
}

static void set_defaults() {
    unlock = IM_U_IDLE;
    mode = IM_PS2;
	sample_rate = 20;
	sample_counter = 0;
}

void mouse_init(void) {

    mouseX = 0;
    mouseY = 0;

    // AMX mouse
    gpio_init(GPIO_AMX_BTN_L);
    gpio_set_dir(GPIO_AMX_BTN_L, GPIO_IN);
    gpio_set_pulls(GPIO_AMX_BTN_L, true, false);
    gpio_init(GPIO_AMX_BTN_M);
    gpio_set_dir(GPIO_AMX_BTN_M, GPIO_IN);
    gpio_set_pulls(GPIO_AMX_BTN_M, true, false);
    gpio_init(GPIO_AMX_BTN_R);
    gpio_set_dir(GPIO_AMX_BTN_R, GPIO_IN);
    gpio_set_pulls(GPIO_AMX_BTN_R, true, false);
    gpio_init(GPIO_AMX_X1);
    gpio_set_dir(GPIO_AMX_X1, GPIO_IN);
    gpio_set_pulls(GPIO_AMX_X1, true, false);
    gpio_init(GPIO_AMX_X2);
    gpio_set_dir(GPIO_AMX_X2, GPIO_IN);
    gpio_set_pulls(GPIO_AMX_X2, true, false);
    gpio_init(GPIO_AMX_Y1);
    gpio_set_dir(GPIO_AMX_Y1, GPIO_IN);
    gpio_set_pulls(GPIO_AMX_Y1, true, false);
    gpio_init(GPIO_AMX_Y2);
    gpio_set_dir(GPIO_AMX_Y2, GPIO_IN);
    gpio_set_pulls(GPIO_AMX_Y2, true, false);

    gpio_set_irq_enabled_with_callback(GPIO_AMX_X1, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled(GPIO_AMX_Y1, GPIO_IRQ_EDGE_RISE, true);

    ps2c_init(&ps2_ms, GPIO_PS2MS_CLK_PIN, GPIO_PS2MS_DAT_PIN);

    sleep_ms(200);

    ps2c_write(&ps2_ms, 0xAA);
    ps2c_write(&ps2_ms, 0x00);

	stream_enabled = false;
	last_b = 0xFF;
	last_mX = 0;
	last_mY = 0;
	set_defaults();

}


static void send_packet(uint8_t buttons, int mX, int mY) {
    uint8_t m;
    m = buttons +
        0x08 +
        ((mX<0)?0x10:0) +
        ((mY<0)?0x20:0)
        ;                
    ps2c_write(&ps2_ms, m);
    ps2c_write(&ps2_ms, mX & 0xFF);
    ps2c_write(&ps2_ms, mY & 0xFF);
    if (mode != IM_PS2)
    	ps2c_write(&ps2_ms, 0x00);
	
	last_b = buttons;
	last_mX = mX;
	last_mY = mY;
}

#define READ(x) \
    r = ps2c_read(&ps2_ms, &x); \
    if (r) { \
        printf("MS EER RD:%d %02X\n", r, (int)x); \
        return r; \
    }

#define WRITE(x) \
    r = ps2c_write(&ps2_ms, x); \
    if (r) { \
        printf("MS EER WR:%d %02X\n", r, (int)x); \
        return r; \
    }


int mouse_scan(void) {
	int r = 0;
    int mX, mY;

    uint8_t cmd;

    if (ps2c_tick(&ps2_ms, &cmd) == PS2C_ERR_REQ) {
        uint8_t x;
        printf("MOUSEREQ: %02X\n", cmd);
        intelli_unlock_t unlock_next = IM_U_IDLE;
        switch (cmd) {
        	case 0xFF:
        		// reset command
				WRITE(0xFA);
				sleep_ms(1);
				WRITE(0xAA);
				WRITE(0x00);
				break;
			case 0xFE:
				// resend
				send_packet(last_b, last_mX, last_mY);
				break;
			case 0XF6:
				// set defaults
				WRITE(0xFA);
				set_defaults();
				break;
			case 0xF5:
				WRITE(0xFA);
				stream_enabled = false;
				break;
			case 0xF4:
				WRITE(0xFA);
				stream_enabled = true;
				break;
			case 0xF3:
				WRITE(0xFA);
				READ(x);
				sample_rate = x;
				if (unlock == IM_U_IDLE && x == 200)
					unlock_next = IM_U_200;
				else if (unlock == IM_U_200 && x == 200)
					unlock_next = IM_U_200_2;
				else if (unlock == IM_U_200 && x == 100)
					unlock_next = IM_U_100;
				else if (unlock == IM_U_200_2 && x == 80)
					mode = IM_04_SCROLL;
				else if (unlock == IM_U_100 && x == 80)
					mode = IM_03_SCROLL;

				WRITE(0xFA);
				printf("MS SAMPLERATE: %d (%d %d)\n", sample_rate, unlock_next, mode);
				break;
			case 0xF2:
				WRITE(0xFA);
				switch (mode) {
					case IM_03_SCROLL:
						WRITE(0x03);
						break;
					case IM_04_SCROLL:
						WRITE(0x04);
						break;
					default:
						WRITE(0x00);							
				}
				break;
			default:
				//TODO: remote, wrap modes, resolution etc
				WRITE(0xFA);//spoof
				break;

        }

        unlock = unlock_next;
    }


	sample_counter += sample_rate;
	if (sample_counter > 1000) {
		sample_counter = 0;
	    uint32_t is = save_and_disable_interrupts();

	    mX = mouseX;
	    mouseX = 0;
	    mY = mouseY;
	    mouseY = 0;

	    restore_interrupts(is);
	    uint8_t b = (gpio_get(GPIO_AMX_BTN_L)?0:1) +
	        (gpio_get(GPIO_AMX_BTN_R)?0:2) +
	        (gpio_get(GPIO_AMX_BTN_M)?0:4);


	    if (b != last_b || mX || mY) {

	        printf("MOUSE %02X %d x %d\n", (int)b, mX, mY);
			
			if (stream_enabled)
				send_packet(b, mX, mY);
	        last_b = b;
	    }
	}

	return r;
}