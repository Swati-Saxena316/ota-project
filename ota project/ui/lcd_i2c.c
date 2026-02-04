#include "lcd_i2c.h"
#include "config/pin_config.h"

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

static void lcd_i2c_write(uint8_t data)
{
    i2c_master_write_to_device(
        I2C_MASTER_NUM,
        LCD_I2C_ADDR,
        &data,
        1,
        pdMS_TO_TICKS(100)
    );
}

static void lcd_pulse_enable(uint8_t data)
{
    lcd_i2c_write(data | LCD_ENABLE | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_i2c_write((data & ~LCD_ENABLE) | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_send_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble << 4) | rs | LCD_BACKLIGHT;
    lcd_pulse_enable(data);
}

static void lcd_send_byte(uint8_t byte, uint8_t rs)
{
    lcd_send_nibble(byte >> 4, rs);
    lcd_send_nibble(byte & 0x0F, rs);
}

static void lcd_cmd(uint8_t cmd)
{
    lcd_send_byte(cmd, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void lcd_data(uint8_t data)
{
    lcd_send_byte(data, LCD_RS);
}

void lcd_hw_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(50));

    lcd_send_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x02, 0);  // 4-bit mode

    lcd_cmd(0x28); // 4-bit, 2-line
    lcd_cmd(0x0C); // Display ON
    lcd_cmd(0x06); // Entry mode
    lcd_cmd(0x01); // Clear
}

void lcd_hw_clear(void)
{
    lcd_cmd(0x01);
}

void lcd_hw_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    lcd_cmd(0x80 | (col + row_offsets[row]));
}

void lcd_hw_print(const char *str)
{
    while (*str)
    {
        lcd_data(*str++);
    }
}
