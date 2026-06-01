#ifndef dOLED_H
#define dOLED_H

#include <string.h>
#include <pico/types.h>

class dSSD1306 {
    
public:
    // Standard SSD1306 configurations
    static constexpr uint8_t WIDTH = 128;
    static constexpr uint8_t HEIGHT = 64;
    static constexpr uint8_t I2C_ADDRESS = 0x3C; // Default SSD1306 I2C address

    dSSD1306();

    // Public API
    void Init();
    
    // Core Display Functions
    void Clear();
    void Update();
    void Draw_Pixel(int16_t x, int16_t y, bool is_on);
    void Draw_Character(char character, uint8_t starting_x, uint8_t row);
    void Draw_Text(uint8_t row, const char message[21]); // valid row values: 0-7, only 21 characters will fit on a row
    void Clear_Row(uint8_t row);
    const char * Number_to_String(uint32_t number);

    // Utility Features
    void Set_Contrast(uint8_t contrast);
    void Invert_Display(bool invert);

private:
    uint8_t display_buffer[1025];
    static const uint8_t control_byte = 0x00;
    static const uint8_t character_fonts[475]; // I believe it is size 95
    
    // Internal I2C helpers
    void Send_Command(const uint8_t *commands, size_t number_of_commands);
    void Send_Command(uint8_t command); // single command wrapper for the Send_Command above
    
    // Prevent copying
    dSSD1306(const dSSD1306&);
    void operator=(const dSSD1306&);
};

// Global instance
extern dSSD1306 g_SSD1306;

#endif  // dOLED_H