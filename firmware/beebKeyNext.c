#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include "ps2keys.h"
#include "ps2comm.h"



//PS2 keyboard
//PA7-out on KeyJam
#define GPIO_PS2KB_DAT_PIN  22
//CA2-out on KeyJam
#define GPIO_PS2KB_CLK_PIN  16

#define GPIO_CA2_IN_PIN     4
#define GPIO_KB_EN_PIN      13
#define GPIO_1MHZ_PIN       14
#define GPIO_PA7_IN_PIN     28

#define GPIO_PA3_IN_PIN     6
#define GPIO_PA4_IN_PIN     12
#define GPIO_PA5_IN_PIN     11
#define GPIO_PA6_IN_PIN     10
#define GPIO_PA0_IN_PIN     9
#define GPIO_PA1_IN_PIN     8
#define GPIO_PA2_IN_PIN     7

#define GPIO_LED1_PIN       3
#define GPIO_LED2_PIN       2
#define GPIO_LED3_PIN       5

#define GPIO_RST_IN_SPEC    21

//Keyjam board unusused
#define GPIO_RELAY_PIN      17
#define GPIO_RST_PIN        15


//the set row, set col functions assume 1MHzE is low on entry
void key_setrow(uint8_t r) {
    gpio_put(GPIO_PA4_IN_PIN, r & 1);
    gpio_put(GPIO_PA5_IN_PIN, r & 2);
    gpio_put(GPIO_PA6_IN_PIN, r & 4);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 1);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 0);
}

void key_setcol(uint8_t c) {
    gpio_put(GPIO_PA0_IN_PIN, c & 1);
    gpio_put(GPIO_PA1_IN_PIN, c & 2);
    gpio_put(GPIO_PA2_IN_PIN, c & 4);
    gpio_put(GPIO_PA3_IN_PIN, c & 8);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 1);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 0);
}


bool keysdown[81]; //81st key is reset

void key_init() {
    memset((void *)keysdown, 0, 80);
}

void key_check(int ix, bool x) {
    uint8_t ox = keysdown[ix];
    if (x != ox) {
        uint16_t ps2 = getps2(ix);
        printf("%d => %02X\n", ix, (int)ps2);
        if (ps2) {
            int r;
            if (ox) {
                r = ps2c_keyup(ps2);
                printf("F0%04X\n", ps2);
            } else {
                r = ps2c_keydown(ps2);
                printf("%04X\n", ps2);
            }
            if (r) {
                printf("ERR %d\n", r);
            }
        }
    }

    keysdown[ix] = x;
}

void key_scan(void) {

    ps2c_tick();  // check for commands back from host

    int ix = 0;
    for (int r = 0; r <= 7; r++) {
        key_setrow(r);        
        for (int c = 0; c <= 9; c++) {
            key_setcol(c);
            sleep_us(10);
            key_check(ix++, gpio_get(GPIO_PA7_IN_PIN));
        }
    }

    // handle reset separately
    key_check(ix++, !gpio_get(GPIO_RST_IN_SPEC));

}

int main()
{
    stdio_init_all();

    // Pico LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN,GPIO_OUT);
    
    // Keyboard LEDs
    gpio_init(GPIO_LED1_PIN);
    gpio_set_dir(GPIO_LED1_PIN, GPIO_OUT);
    gpio_init(GPIO_LED2_PIN);
    gpio_set_dir(GPIO_LED2_PIN, GPIO_OUT);
    gpio_init(GPIO_LED3_PIN);
    gpio_set_dir(GPIO_LED3_PIN, GPIO_OUT);

    //row/col pins
    gpio_init(GPIO_PA3_IN_PIN);
    gpio_set_dir(GPIO_PA3_IN_PIN, GPIO_OUT);
    gpio_init(GPIO_PA4_IN_PIN);
    gpio_set_dir(GPIO_PA4_IN_PIN, GPIO_OUT);
    gpio_init(GPIO_PA5_IN_PIN);
    gpio_set_dir(GPIO_PA5_IN_PIN, GPIO_OUT);
    gpio_init(GPIO_PA6_IN_PIN);
    gpio_set_dir(GPIO_PA6_IN_PIN, GPIO_OUT);
    gpio_init(GPIO_PA0_IN_PIN);
    gpio_set_dir(GPIO_PA0_IN_PIN, GPIO_OUT);
    gpio_init(GPIO_PA1_IN_PIN);
    gpio_set_dir(GPIO_PA1_IN_PIN, GPIO_OUT);
    gpio_init(GPIO_PA2_IN_PIN);
    gpio_set_dir(GPIO_PA2_IN_PIN, GPIO_OUT);

    //control / sense pins
    gpio_init(GPIO_CA2_IN_PIN);
    gpio_set_dir(GPIO_CA2_IN_PIN, GPIO_IN);
    gpio_init(GPIO_PA7_IN_PIN);
    gpio_set_dir(GPIO_PA7_IN_PIN, GPIO_IN);
    gpio_init(GPIO_KB_EN_PIN);
    gpio_set_dir(GPIO_KB_EN_PIN, GPIO_OUT);
    gpio_init(GPIO_1MHZ_PIN);
    gpio_set_dir(GPIO_1MHZ_PIN, GPIO_OUT);

    gpio_put(GPIO_KB_EN_PIN, 0);

    gpio_init(GPIO_RST_IN_SPEC);
    gpio_set_pulls(GPIO_RST_IN_SPEC, 1, 0);


    //extras to for KeyJam board
    gpio_init(GPIO_RELAY_PIN);
    gpio_set_dir(GPIO_RELAY_PIN, GPIO_OUT);
    gpio_put(GPIO_RELAY_PIN,0);
    gpio_init(GPIO_RST_PIN);
    gpio_set_dir(GPIO_RST_PIN, GPIO_OUT);
    gpio_put(GPIO_RST_PIN,0);

    puts("init...");

    key_init();

    ps2c_init(GPIO_PS2KB_CLK_PIN, GPIO_PS2KB_DAT_PIN);

    while (true) {
        sleep_ms(1);

        key_scan();

        uint8_t i = ps2c_getleds();
           
        gpio_put(GPIO_LED1_PIN, !(i & 1));
        gpio_put(GPIO_LED2_PIN, !(i & 2));
        gpio_put(GPIO_LED3_PIN, !(i & 4));

    }

    return 0;
}

