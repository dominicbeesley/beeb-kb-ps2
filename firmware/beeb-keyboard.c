#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "debug.h"
#include "ps2keys.h"
#include "ps2comm.h"
#include "beeb-keyboard.h"
#include "pins.h"

struct ps2c ps2_kb;
uint8_t leds;
uint32_t last_code;
bool keysdown[81]; //81st key is reset

static void key_reset(void);

//the set row, set col functions assume 1MHzE is low on entry
static void key_setrow(uint8_t r) {
    gpio_put(GPIO_PA4_OUT_PIN, r & 1);
    gpio_put(GPIO_PA5_OUT_PIN, r & 2);
    gpio_put(GPIO_PA6_OUT_PIN, r & 4);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 1);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 0);
}

static void key_setcol(uint8_t c) {
    gpio_put(GPIO_PA0_OUT_PIN, c & 1);
    gpio_put(GPIO_PA1_OUT_PIN, c & 2);
    gpio_put(GPIO_PA2_OUT_PIN, c & 4);
    gpio_put(GPIO_PA3_OUT_PIN, c & 8);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 1);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 0);
}



static void key_show_leds(void) {
    // translate between incoming byte order (CAPS = 2, SHIFT=1, MOTOR=0)
    gpio_put(GPIO_LED1_PIN, (leds & 2)?1:0);
    gpio_put(GPIO_LED2_PIN, (leds & 4)?1:0);
    gpio_put(GPIO_LED3_PIN, (leds & 1)?1:0);

}

void key_init() {

    // Keyboard LEDs
    gpio_init(GPIO_LED1_PIN);
    gpio_set_dir(GPIO_LED1_PIN, GPIO_OUT);
    gpio_init(GPIO_LED2_PIN);
    gpio_set_dir(GPIO_LED2_PIN, GPIO_OUT);
    gpio_init(GPIO_LED3_PIN);
    gpio_set_dir(GPIO_LED3_PIN, GPIO_OUT);

    //row/col pins
    gpio_init(GPIO_PA3_OUT_PIN);
    gpio_set_dir(GPIO_PA3_OUT_PIN, GPIO_OUT);
    gpio_init(GPIO_PA4_OUT_PIN);
    gpio_set_dir(GPIO_PA4_OUT_PIN, GPIO_OUT);
    gpio_init(GPIO_PA5_OUT_PIN);
    gpio_set_dir(GPIO_PA5_OUT_PIN, GPIO_OUT);
    gpio_init(GPIO_PA6_OUT_PIN);
    gpio_set_dir(GPIO_PA6_OUT_PIN, GPIO_OUT);
    gpio_init(GPIO_PA0_OUT_PIN);
    gpio_set_dir(GPIO_PA0_OUT_PIN, GPIO_OUT);
    gpio_init(GPIO_PA1_OUT_PIN);
    gpio_set_dir(GPIO_PA1_OUT_PIN, GPIO_OUT);
    gpio_init(GPIO_PA2_OUT_PIN);
    gpio_set_dir(GPIO_PA2_OUT_PIN, GPIO_OUT);

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

    gpio_init(GPIO_RST_IN);
    gpio_set_pulls(GPIO_RST_IN, 1, 0);

    ps2c_init(&ps2_kb, GPIO_PS2KB_CLK_PIN, GPIO_PS2KB_DAT_PIN);

}


static int keydown(uint16_t code) {    
    last_code = code;
    if (code & 0xFF00) {
        int r = ps2c_write(&ps2_kb, code >> 8);
        if (r)
            return r;
    }
    return ps2c_write(&ps2_kb, code);

}

static int keyup(uint16_t code) {
    last_code = code | 0xFF0000;
    if (code & 0xFF00) {
        int r = ps2c_write(&ps2_kb, code >> 8);
        if (r)
            return r;
    }
    int r = ps2c_write(&ps2_kb, 0xF0);
    if (r) return r;
    return ps2c_write(&ps2_kb, code);
}


static int key_check(int ix, bool x) {
    int r = 0;
    uint8_t ox = keysdown[ix];
    if (x != ox) {
        uint16_t ps2 = getps2(ix);
        DBUG(printf("%d => %02X\n", ix, (int)ps2));
        if (ps2) {
            if (ox) {
                r = keyup(ps2);
                DBUG(printf("F0%04X\n", ps2));
            } else {
                r = keydown(ps2);
                DBUG(printf("%04X\n", ps2));
            }
        }
        if (!r) {
            keysdown[ix] = x;
        }
    }
    if (r) 
        DBUG(printf("ERR %d\n", r));

    return r;

}

#define READ_KB(x) \
    r = ps2c_read(&ps2_kb, &x); \
    if (r) { \
        DBUG(printf("HRDER:%d %02X\n", r, (int)x)); \
        return r; \
    }

static void key_reset(void) {

    memset((void *)keysdown, 0, 80);

    leds = 0xFF;
    key_show_leds();

    sleep_ms(500);

    ps2c_write(&ps2_kb, 0xAA);    //ack
    leds = 0x00;
    key_show_leds();
    DBUG(printf("RESET\n"));

}

int key_scan(void) {

    int r;
    uint8_t cmd;
    if (ps2c_tick(&ps2_kb, &cmd) == PS2C_ERR_REQ) {
        // commands back from host in cmd
        DBUG(printf("HOST:REQ\n"))

        DBUG(printf("HOST:%02X\n", (int)cmd));

        uint8_t d;
        switch (cmd) {
            case 0xFF: //reset
                ps2c_write(&ps2_kb, 0xFA);    //ack
                key_reset();
                break;
            case 0xFE: //resend last
//                if (last_code & 0xFF0000)
//                    keyup(last_code);
//                else
//                    keydown(last_code);
                DBUG(printf("RESEND: %08X\n", (int)last_code));
                break;
            case 0xF5: //disable
                ps2c_write(&ps2_kb, 0xFA);    //ack
                DBUG(printf("DISABLE:\n"));
                break;
            case 0xF4: //enable
                ps2c_write(&ps2_kb, 0xFA);    //ack
                DBUG(printf("ENABLE:\n"));
                break;
            case 0xF3: //repeat rate - do nothing with this
                ps2c_write(&ps2_kb, 0xFA);    //acknowledge reset
                READ_KB(d)
                DBUG(printf("TRATE: %d\n", (int)d));
                break;
            case 0xF2: //identify
                ps2c_write(&ps2_kb, 0xFA);    //acknowledge reset
                ps2c_write(&ps2_kb, 0xAB);
                ps2c_write(&ps2_kb, 0x83);
                DBUG(printf("ID\n"));
                break;
            case 0xF0: //read / set scan code set
                ps2c_write(&ps2_kb, 0xFA);    //ack
                READ_KB(d)
                DBUG(printf("SCAN: %d\n", (int)d));
                if (!d)
                    ps2c_write(&ps2_kb, 1);
                break;
            case 0xEE: //echo
                ps2c_write(&ps2_kb, 0xEE);
                DBUG(printf("ECHO\n"));
                break;
            case 0xED: // set LEDs
                DBUG(printf("LEDS:enter..wait\n"));
                ps2c_write(&ps2_kb, 0xFA); // ack
                READ_KB(d)
                DBUG(printf("LEDS: %02x\n", (int)d));
                leds = d;

                key_show_leds();

                break;
            default:
                ps2c_write(&ps2_kb, 0xFE); //resend please
                DBUG(printf("UK:%02X\n", (int)cmd));
                break;
        }
    } 

    int ix = 0;
    for (int row = 0; row <= 7; row++) {
        key_setrow(row);        
        for (int col = 0; col <= 9; col++) {
            key_setcol(col);
            sleep_us(10);
            r = key_check(ix++, gpio_get(GPIO_PA7_IN_PIN));
            if (r)
                goto end;
        }
    }

    // handle reset separately
    r = key_check(ix++, !gpio_get(GPIO_RST_IN));

end:

    if (r) {
        DBUG(printf("SCAN:END:ERR:%d\n", r));
    }

    return r;
}
