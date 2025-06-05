#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/multicore.h"

#include "debug.h"
#include "beeb-keyboard.h"
#include "beeb-mouse.h"
#include "pins.h"

//second core for dedicated mouse action
void core_mouse(void) {

    mouse_init();
    while (true) {
        sleep_ms(1);
        mouse_scan();
    }

}

int main()
{

    stdio_init_all();

    DBUG(puts("init..."));

    key_init();

    multicore_launch_core1(&core_mouse);

    DBUG(puts("scan..."));

    while (true) {
        sleep_ms(1);

        key_scan();       
    }

    return 0;
}

