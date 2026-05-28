#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// GPIO pin for LED
#define LED_PIN 4

int main() {
    // Initialize stdio for USB serial
    stdio_init_all();
    
    // Initialize the LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    printf("==============================================\n");
    printf("RP2350 Pico SDK Template\n");
    printf("==============================================\n");
    printf("System initialized successfully!\n\n");
    
    // Main loop: blink LED and print message
    uint32_t counter = 0;
    while (true) {
        // Toggle LED
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
        
        // Print status every 10 blinks (every 10 seconds)
        counter++;
        if (counter % 10 == 0) {
            printf("Status: Running... (blinks: %ld)\n", counter);
        }
    }
    
    return 0;
}
