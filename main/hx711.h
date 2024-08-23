#ifndef HX711_H
#define HX711_H

#include "driver/gpio.h"

// GPIO Pins for HX711
#define HX711_DT GPIO_NUM_4
#define HX711_SCK GPIO_NUM_5

// Function to initialize HX711
void hx711_init(void);

// Function to read data from HX711
long hx711_read(void);

#endif // HX711_H