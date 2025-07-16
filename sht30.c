#include "sht30.h"

static const char *TAG = "sht30";

#define DELAY_MS           20
#define I2C_TIMEOUT_MS     1000
#define SHT30_CMD_MEAS     0x2400
#define SHT30_CMD_RESET    0x30A2
#define SHT30_CMD_HEATER_ON  0x306D
#define SHT30_CMD_HEATER_OFF 0x3066

//---------------------------------------------------------------
// Função interna para verificar o CRC dos dados
static uint8_t sht30_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

esp_err_t sht30_crc_check(const uint8_t *data, uint8_t expected_crc) {
    return (sht30_crc8(data, 2) == expected_crc) ? ESP_OK : ESP_ERR_INVALID_CRC;
}

//---------------------------------------------------------------
// Reset do sensor (comando 0x30A2)
esp_err_t sht30_reset(i2c_master_dev_handle_t dev) {
    uint8_t cmd[2] = { 0x30, 0xA2 };
    esp_err_t err = i2c_master_transmit(dev, cmd, 2, I2C_TIMEOUT_MS);
    vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
    return err;
}

//---------------------------------------------------------------
// Leitura de temperatura e umidade (6 bytes + CRC)
/**
 * Performs a temperature and humidity measurement using the SHT30 sensor.
 * 
 * This function sends the command 0x2400, which requests a measurement with
 * high repeatability and no clock stretching. After issuing the command,
 * it waits ~30 ms (maximum conversion time) to ensure the sensor has completed
 * the internal measurement process.
 * 
 * Then, it reads 6 bytes from the sensor:
 * - 2 bytes: raw temperature,
 * - 1 byte: CRC of temperature,
 * - 2 bytes: raw humidity,
 * - 1 byte: CRC of humidity.
 * 
 * The function verifies both CRCs to ensure data integrity. If the CRCs pass,
 * it converts the raw values into:
 * - Temperature in Celsius using the formula: -45 + 175 * (raw_temp / 65535),
 * - Relative humidity (%) using the formula: 100 * (raw_hum / 65535).
 * 
 * Note: Because clock stretching is disabled, the master (ESP32) must wait
 * manually (vTaskDelay) for the sensor to finish processing. This avoids 
 * reading incomplete data.
 * 
 * Returns:
 *  - ESP_OK on success,
 *  - ESP_ERR_INVALID_CRC if CRC check fails,
 *  - or error codes from i2c_master_transmit / i2c_master_receive.
 */
esp_err_t sht30_measure(i2c_master_dev_handle_t dev, float *temperature, float *humidity) {
    // uint8_t cmd[2] = { 0x24, 0x00 }; // high repeatability, no clock stretching
    uint8_t cmd[2] = { 0x2C, 0x06};
    esp_err_t err = i2c_master_transmit(dev, cmd, 2, I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;
    // vTaskDelay(pdMS_TO_TICKS(15)); // tempo de conversão
    vTaskDelay(pdMS_TO_TICKS(30)); // aguarda o tempo máximo de conversão

    uint8_t buf[6];
    err = i2c_master_receive(dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    if (sht30_crc_check(&buf[0], buf[2]) != ESP_OK) return ESP_ERR_INVALID_CRC;
    if (sht30_crc_check(&buf[3], buf[5]) != ESP_OK) return ESP_ERR_INVALID_CRC;

    uint16_t raw_temp = (buf[0] << 8) | buf[1];
    uint16_t raw_hum  = (buf[3] << 8) | buf[4];

    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity    = 100.0f * ((float)raw_hum / 65535.0f);

    return ESP_OK;
}

//---------------------------------------------------------------
// Liga/desliga aquecedor interno
esp_err_t sht30_set_heater(i2c_master_dev_handle_t dev, bool enable) {
    uint8_t cmd[2];
    if (enable) {
        cmd[0] = 0x30;
        cmd[1] = 0x6D;
    } else {
        cmd[0] = 0x30;
        cmd[1] = 0x66;
    }
    return i2c_master_transmit(dev, cmd, 2, I2C_TIMEOUT_MS);
}

//---------------------------------------------------------------
// Leitura de registrador de status
esp_err_t sht30_read_status(i2c_master_dev_handle_t dev, uint16_t *status) {
    uint8_t cmd[2] = { 0xF3, 0x2D };
    uint8_t buf[3];
    esp_err_t err = i2c_master_transmit(dev, cmd, 2, I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;
    err = i2c_master_receive(dev, buf, 3, I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    if (sht30_crc_check(&buf[0], buf[2]) != ESP_OK) return ESP_ERR_INVALID_CRC;

    *status = (buf[0] << 8) | buf[1];
    return ESP_OK;
}

//---------------------------------------------------------------
// Inicializa o dispositivo
esp_err_t sht30_init(uint8_t i2c_port,
                     gpio_num_t sda,
                     gpio_num_t scl,
                     uint32_t freq_hz,
                     i2c_master_bus_handle_t *bus_handle_out,
                     i2c_master_dev_handle_t *dev_handle_out) {
    if (!bus_handle_out || !dev_handle_out) return ESP_ERR_INVALID_ARG;

    esp_err_t err;

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    err = i2c_new_master_bus(&bus_cfg, bus_handle_out);
    if (err != ESP_OK) return err;

    i2c_device_config_t dev_cfg = {
        .device_address = SHT30_I2C_ADDR,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = freq_hz,
    };

    err = i2c_master_bus_add_device(*bus_handle_out, &dev_cfg, dev_handle_out);
    if (err != ESP_OK) return err;

    err = i2c_master_probe(*bus_handle_out, SHT30_I2C_ADDR, I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "SHT30 detectado com sucesso");

    err = sht30_reset(*dev_handle_out);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Reset falhou: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}
