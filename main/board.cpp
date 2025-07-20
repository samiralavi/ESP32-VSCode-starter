#include "board.h"

#include <driver/gpio.h>

void Board::configure() {
    /* Configure LEDs */
    gpio_reset_pin(Board::PIN_LED_RED);
    gpio_reset_pin(Board::PIN_LED_GREEN);
    gpio_reset_pin(Board::PIN_LED_BLUE);
    gpio_set_direction(Board::PIN_LED_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(Board::PIN_LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(Board::PIN_LED_BLUE, GPIO_MODE_OUTPUT);
}
