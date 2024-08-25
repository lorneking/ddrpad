#ifndef HX711_H
#define HX711_H

#include "driver/gpio.h"

void hx711_init(void);
long hx711_read(gpio_num_t data_pin);

#endif // HX711_H