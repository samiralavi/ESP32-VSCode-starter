#ifndef BOARD_H
#define BOARD_H

#include <driver/gpio.h>

class Board {

public:
    void configure();

    static constexpr gpio_num_t PIN_LED_RED = GPIO_NUM_38;
    static constexpr gpio_num_t PIN_LED_GREEN = GPIO_NUM_21;
    static constexpr gpio_num_t PIN_LED_BLUE = GPIO_NUM_14;
};

#endif
