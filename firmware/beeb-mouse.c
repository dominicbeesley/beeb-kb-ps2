#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/sync.h"

#include "ps2comm.h"
#include "beeb-mouse.h"
#include "pins.h"

struct ps2c ps2_ms;
volatile int mouseX, mouseY;

void gpio_callback(uint gpio, uint32_t event_mask)
{
    if (gpio == GPIO_AMX_X1) {
        mouseX += gpio_get(GPIO_AMX_X2)?-1:1;
    } else if (gpio == GPIO_AMX_Y1) {
        mouseY += gpio_get(GPIO_AMX_Y2)?-1:1;
    }

}

int iabs(int x) {
    if (x >= 0)
        return x;
    else
        return -x;
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

}

uint8_t last_b; // button state last time round

int mouse_scan(void) {
    uint8_t m;

    int mX, mY;
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

        printf("MOUSE %02X %d x %d\n", (int)m, mX, mY);

        uint8_t cmd;
        if (ps2c_tick(&ps2_ms, &cmd) == PS2C_ERR_REQ) {
            printf("MOUSEREQ: %d\n", cmd);
        }

        m = b +
            0x08 +
            ((mX<0)?0x10:0) +
            ((mY<0)?0x20:0)
            ;                
        ps2c_write(&ps2_ms, m);
        ps2c_write(&ps2_ms, mX & 0xFF);
        ps2c_write(&ps2_ms, mY & 0xFF);
        last_b = b;
    }

	return 0;
}