#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include "ps2keys.h"
#include "ps2comm.h"


//PS/2 connector - keyboard
#define GPIO_PS2KB_DAT_PIN  0
#define GPIO_PS2KB_CLK_PIN  1
//PS/2 connector - mouse
#define GPIO_PS2MS_DAT_PIN  21
#define GPIO_PS2MS_CLK_PIN  22

//AMX connector
#define GPIO_AMX_BTN_L      18
#define GPIO_AMX_BTN_M      17
#define GPIO_AMX_BTN_R      16
#define GPIO_AMX_X1         27
#define GPIO_AMX_X2         20
#define GPIO_AMX_Y1         26
#define GPIO_AMX_Y2         19


#define GPIO_CA2_IN_PIN     4
#define GPIO_KB_EN_PIN      13
#define GPIO_1MHZ_PIN       14
#define GPIO_PA7_IN_PIN     28

#define GPIO_PA0_OUT_PIN    9
#define GPIO_PA1_OUT_PIN    8
#define GPIO_PA2_OUT_PIN    7
#define GPIO_PA3_OUT_PIN    6
#define GPIO_PA4_OUT_PIN    12
#define GPIO_PA5_OUT_PIN    11
#define GPIO_PA6_OUT_PIN    10

#define GPIO_LED1_PIN       3
#define GPIO_LED2_PIN       2
#define GPIO_LED3_PIN       5

#define GPIO_RST_IN         15

struct ps2c ps2_kb;
uint8_t leds;

//the set row, set col functions assume 1MHzE is low on entry
void key_setrow(uint8_t r) {
    gpio_put(GPIO_PA4_OUT_PIN, r & 1);
    gpio_put(GPIO_PA5_OUT_PIN, r & 2);
    gpio_put(GPIO_PA6_OUT_PIN, r & 4);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 1);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 0);
}

void key_setcol(uint8_t c) {
    gpio_put(GPIO_PA0_OUT_PIN, c & 1);
    gpio_put(GPIO_PA1_OUT_PIN, c & 2);
    gpio_put(GPIO_PA2_OUT_PIN, c & 4);
    gpio_put(GPIO_PA3_OUT_PIN, c & 8);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 1);
    sleep_us(1);
    gpio_put(GPIO_1MHZ_PIN, 0);
}


bool keysdown[81]; //81st key is reset

void key_init() {
    memset((void *)keysdown, 0, 80);
}

uint32_t last_code;

int keydown(uint16_t code) {    
    last_code = code;
    if (code & 0xFF00) {
        int r = ps2c_write(&ps2_kb, code >> 8);
        if (r)
            return r;
    }
    return ps2c_write(&ps2_kb, code);

}

int keyup(uint16_t code) {
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


int key_check(int ix, bool x) {
    int r = 0;
    uint8_t ox = keysdown[ix];
    if (x != ox) {
        uint16_t ps2 = getps2(ix);
        printf("%d => %02X\n", ix, (int)ps2);
        if (ps2) {
            if (ox) {
                r = keyup(ps2);
                printf("F0%04X\n", ps2);
            } else {
                r = keydown(ps2);
                printf("%04X\n", ps2);
            }
        }
        if (!r) {
            keysdown[ix] = x;
        }
    }
    if (r) 
        printf("ERR %d\n", r);

    return r;

}

#define READ_KB(x) \
    r = ps2c_read(&ps2_kb, &x); \
    if (r) { \
        printf("HRDER:%d %02X\n", r, (int)x); \
        return r; \
    }


int key_scan(void) {

    int r;
    uint8_t cmd;
    if (ps2c_tick(&ps2_kb, &cmd) == PS2C_ERR_REQ) {
        // commands back from host in cmd
        printf("HOST:REQ\n");

        printf("HOST:%02X\n", (int)cmd);

        uint8_t d;
        switch (cmd) {
            case 0xFF: //reset
                ps2c_write(&ps2_kb, 0xFA);    //ack
                ps2c_write(&ps2_kb, 0xAA);    //ack
                leds = 0;
                printf("RESET\n");
                break;
            case 0xFE: //resend last
                if (last_code & 0xFF0000)
                    keyup(last_code);
                else
                    keydown(last_code);
                printf("RESEND: %08X\n", (int)last_code);
                break;
            case 0xF5: //disable
                ps2c_write(&ps2_kb, 0xFA);    //ack
                printf("DISABLE:\n");
                break;
            case 0xF4: //enable
                ps2c_write(&ps2_kb, 0xFA);    //ack
                printf("ENABLE:\n");
                break;
            case 0xF3: //repeat rate - do nothing with this
                ps2c_write(&ps2_kb, 0xFA);    //acknowledge reset
                READ_KB(d)
                printf("TRATE: %d\n", (int)d);
                break;
            case 0xF2: //identify
                ps2c_write(&ps2_kb, 0xFA);    //acknowledge reset
                ps2c_write(&ps2_kb, 0xAB);
                ps2c_write(&ps2_kb, 0x83);
                printf("ID\n");
                break;
            case 0xF0: //read / set scan code set
                ps2c_write(&ps2_kb, 0xFA);    //ack
                READ_KB(d)
                printf("SCAN: %d\n", (int)d);
                if (!d)
                    ps2c_write(&ps2_kb, 1);
                break;
            case 0xEE: //echo
                ps2c_write(&ps2_kb, 0xEE);
                printf("ECHO\n");
                break;
            case 0xED: // set LEDs
                printf("LEDS:enter..wait\n");
                ps2c_write(&ps2_kb, 0xFA); // ack
                READ_KB(d)
                printf("LEDS: %02x\n", (int)d);
                leds = d;
                break;
            default:
                printf("UK:%02X\n", (int)cmd);
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
                return r;
        }
    }

    // handle reset separately
    key_check(ix++, !gpio_get(GPIO_RST_IN));
    return 0;
}


volatile int mouseX, mouseY;

void gpio_callback(uint gpio, uint32_t event_mask)
{
    if (gpio == GPIO_AMX_X1) {
        mouseX += gpio_get(GPIO_AMX_X2)?-1:1;
    } else if (gpio == GPIO_AMX_Y1) {
        mouseY += gpio_get(GPIO_AMX_Y2)?-1:1;
    }

}


int main()
{
    mouseX = 0;
    mouseY = 0;
    leds = 0xFF;

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

    puts("init...");

    key_init();

    ps2c_init(&ps2_kb, GPIO_PS2KB_CLK_PIN, GPIO_PS2KB_DAT_PIN);

    uint8_t c;
    while (true) {
        sleep_ms(1);

        key_scan();
           
        // translate between incoming byte order (CAPS = 2, SHIFT=1, MOTOR=0)
        gpio_put(GPIO_LED1_PIN, (leds & 2)?1:0);
        gpio_put(GPIO_LED2_PIN, (leds & 4)?1:0);
        gpio_put(GPIO_LED3_PIN, (leds & 1)?1:0);

        c--;
        if (c == 0) {
            uint8_t m = 
                (gpio_get(GPIO_AMX_BTN_L)?1:0) +
                (gpio_get(GPIO_AMX_BTN_M)?2:0) +
                (gpio_get(GPIO_AMX_BTN_R)?4:0) +
                (gpio_get(GPIO_AMX_X1)?8:0) +
                (gpio_get(GPIO_AMX_X2)?16:0) +
                (gpio_get(GPIO_AMX_Y1)?32:0) +
                (gpio_get(GPIO_AMX_Y2)?64:0);

            int mX, mY;
            uint32_t is = save_and_disable_interrupts();
            mX = mouseX;
            mouseX = 0;
            mY = mouseY;
            mouseY = 0;
            restore_interrupts(is);

            printf("MOUSE %02X %d x %d\n", (int)m, mX, mY);
        }

    }

    return 0;
}

