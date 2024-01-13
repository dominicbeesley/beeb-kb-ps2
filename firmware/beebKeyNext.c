#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/multicore.h"

#include "beeb-keyboard.h"
#include "beeb-mouse.h"
#include "pins.h"

int main()
{

    stdio_init_all();

    puts("init...");

    key_init();
    mouse_init();

    puts("scan...");

    while (true) {
        sleep_ms(1);

        key_scan();

        mouse_scan();
           

    }

    return 0;
}

