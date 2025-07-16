#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_timer.h>
#include <esp_err.h>
#include "esp_rom_sys.h"  // for esp_rom_delay_us

#define SHT30_I2C_ADDR          0x44  // default; can be 0x45 if ADDR=1
#define SHT30_CMD_MEAS_HIGHREP  0x2400
#define SHT30_CMD_SOFT_RESET    0x30A2
#define SHT30_CMD_READ_STATUS   0xF32D
#define SHT30_CMD_HEATER_ON     0x306D
#define SHT30_CMD_HEATER_OFF    0x3066

/** Checks the CRC of received SHT30 data (polynomial 0x31) */
esp_err_t sht30_crc_check(const uint8_t *data, uint8_t expected_crc);

/** Starts a temperature and humidity measurement and returns the values */
esp_err_t sht30_measure(i2c_master_dev_handle_t dev, float *temp_c, float *rh_percent);

/** Performs a soft reset on the sensor */
esp_err_t sht30_reset(i2c_master_dev_handle_t dev);

/** Reads the 16-bit status register */
esp_err_t sht30_read_status(i2c_master_dev_handle_t dev, uint16_t *status);

/** Enables or disables the internal heater */
esp_err_t sht30_set_heater(i2c_master_dev_handle_t dev, bool enable);

/** Initializes the I2C bus and the device handle */
esp_err_t sht30_init(uint8_t i2c_port,
                     gpio_num_t sda,
                     gpio_num_t scl,
                     uint32_t freq_hz,
                     i2c_master_bus_handle_t *bus_handle_out,
                     i2c_master_dev_handle_t *dev_handle_out);
