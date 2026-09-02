#include "temperature_sensor.h"

#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// DS18B20 1-Wire commands and timing constants:
#define DS18B20_CMD_SKIP_ROM          0xCC
#define DS18B20_CMD_CONVERT_T         0x44
#define DS18B20_CMD_READ_SCRATCHPAD   0xBE
#define DS18B20_CONVERSION_TIME_MS    750

static gpio_num_t data_gpio = GPIO_NUM_NC; // GPIO connected to the DS18B20 data line

// Release the 1-Wire bus to the pull-up level:
static void one_wire_release(void) {
    gpio_set_direction(data_gpio, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(data_gpio, 1);
}

// Drive the 1-Wire bus low:
static void one_wire_drive_low(void) {
    gpio_set_direction(data_gpio, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(data_gpio, 0);
}

// Reset the 1-Wire bus and detect sensor presence:
static esp_err_t one_wire_reset(void) {
    int presence;

    // Send the reset pulse and sample the presence pulse window:
    one_wire_drive_low();
    esp_rom_delay_us(480);
    one_wire_release();
    esp_rom_delay_us(70);
    presence = gpio_get_level(data_gpio);
    esp_rom_delay_us(410);

    if(presence == 0) return ESP_OK;
    return ESP_ERR_NOT_FOUND;
}

// Write one bit to the 1-Wire bus:
static void one_wire_write_bit(int bit) {
    one_wire_drive_low();

    if(bit) {
        esp_rom_delay_us(6);
        one_wire_release();
        esp_rom_delay_us(64);
    }
    else {
        esp_rom_delay_us(60);
        one_wire_release();
        esp_rom_delay_us(10);
    }
}

// Read one bit from the 1-Wire bus:
static int one_wire_read_bit(void) {
    int bit;

    // Open a read time slot and sample after the sensor has time to respond:
    one_wire_drive_low();
    esp_rom_delay_us(6);
    one_wire_release();
    esp_rom_delay_us(9);
    bit = gpio_get_level(data_gpio);
    esp_rom_delay_us(55);

    return bit;
}

// Write one byte to the 1-Wire bus:
static void one_wire_write_byte(uint8_t value) {
    for(int bit = 0; bit < 8; bit++) {
        one_wire_write_bit(value & 0x01);
        value >>= 1;
    }
}

// Read one byte from the 1-Wire bus:
static uint8_t one_wire_read_byte(void) {
    uint8_t value = 0;

    for(int bit = 0; bit < 8; bit++) {
        value |= (one_wire_read_bit() << bit);
    }

    return value;
}

// Initialize the temperature sensor:
esp_err_t temperature_sensor_init(const temperature_sensor_config_t *config) {
    if(config == NULL) return ESP_ERR_INVALID_ARG;

    data_gpio = config->data_gpio;
    one_wire_release();

    return ESP_OK;
}

// Read the temperature converted to degrees Celsius:
esp_err_t temperature_sensor_read_celsius(float *temperature_c) {
    uint8_t scratchpad[9];
    int16_t raw_temperature;

    if(temperature_c == NULL) return ESP_ERR_INVALID_ARG;
    if(data_gpio == GPIO_NUM_NC) return ESP_ERR_INVALID_STATE;

    // Start a conversion before reading the DS18B20 scratchpad:
    ESP_RETURN_ON_ERROR(one_wire_reset(), "temperature_sensor", "sensor ausente");
    one_wire_write_byte(DS18B20_CMD_SKIP_ROM);
    one_wire_write_byte(DS18B20_CMD_CONVERT_T);
    vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERSION_TIME_MS));

    // Read the scratchpad bytes that contain the raw temperature result:
    ESP_RETURN_ON_ERROR(one_wire_reset(), "temperature_sensor", "sensor ausente");
    one_wire_write_byte(DS18B20_CMD_SKIP_ROM);
    one_wire_write_byte(DS18B20_CMD_READ_SCRATCHPAD);

    for(int index = 0; index < 9; index++) {
        scratchpad[index] = one_wire_read_byte();
    }

    raw_temperature = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);
    *temperature_c = (float)raw_temperature / 16.0f;

    return ESP_OK;
}