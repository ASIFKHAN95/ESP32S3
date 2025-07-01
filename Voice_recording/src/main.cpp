#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "I2S_REC"
#define MOUNT_POINT "/sdcard"

// I2S Configuration
#define I2S_PORT           I2S_NUM_0
#define SAMPLE_RATE        16000
#define BITS_PER_SAMPLE    I2S_BITS_PER_SAMPLE_16BIT
#define I2S_DMA_BUF_LEN    1024
#define I2S_READ_LEN       1024
#define RECORD_DURATION_S  10  // seconds
#define CHANNELS           1

// SD Card SPI pins
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 20
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

// I2S Mic pins
#define I2S_SCK 2
#define I2S_WS  15
#define I2S_SD  13

void i2s_init() {
    i2s_config_t config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = I2S_DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_PORT, &config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
}

void sdcard_init() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };

    sdmmc_card_t* card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        while (1) vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "SD card mounted.");
}

void write_wav_header(FILE *f, uint32_t sample_rate, uint32_t data_size) {
    uint8_t header[44] = {0};

    uint32_t file_size = data_size + 36;
    uint32_t byte_rate = sample_rate * CHANNELS * 16 / 8;
    uint16_t block_align = CHANNELS * 16 / 8;

    memcpy(header, "RIFF", 4);
    header[4] = (uint8_t)(file_size & 0xff);
    header[5] = (uint8_t)((file_size >> 8) & 0xff);
    header[6] = (uint8_t)((file_size >> 16) & 0xff);
    header[7] = (uint8_t)((file_size >> 24) & 0xff);
    memcpy(header + 8, "WAVEfmt ", 8);
    header[16] = 16;
    header[20] = 1; // PCM
    header[22] = CHANNELS;
    header[24] = (uint8_t)(sample_rate & 0xff);
    header[25] = (uint8_t)((sample_rate >> 8) & 0xff);
    header[26] = (uint8_t)((sample_rate >> 16) & 0xff);
    header[27] = (uint8_t)((sample_rate >> 24) & 0xff);
    header[28] = (uint8_t)(byte_rate & 0xff);
    header[29] = (uint8_t)((byte_rate >> 8) & 0xff);
    header[30] = (uint8_t)((byte_rate >> 16) & 0xff);
    header[31] = (uint8_t)((byte_rate >> 24) & 0xff);
    header[32] = block_align;
    header[34] = 16; // bits per sample
    memcpy(header + 36, "data", 4);
    header[40] = (uint8_t)(data_size & 0xff);
    header[41] = (uint8_t)((data_size >> 8) & 0xff);
    header[42] = (uint8_t)((data_size >> 16) & 0xff);
    header[43] = (uint8_t)((data_size >> 24) & 0xff);

    fseek(f, 0, SEEK_SET);
    fwrite(header, 1, 44, f);
}

void record_task(void *arg) {
    FILE *f = fopen(MOUNT_POINT"/record.wav", "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        vTaskDelete(NULL);
    }

    // Reserve header space
    fseek(f, 44, SEEK_SET);

    uint8_t *buf = malloc(I2S_READ_LEN);
    size_t total_written = 0;
    size_t bytes_read;

    int loops = (SAMPLE_RATE * RECORD_DURATION_S * 2) / I2S_READ_LEN;

    for (int i = 0; i < loops; i++) {
        if (i2s_read(I2S_PORT, buf, I2S_READ_LEN, &bytes_read, portMAX_DELAY) == ESP_OK) {
            fwrite(buf, 1, bytes_read, f);
            total_written += bytes_read;
        }
    }

    write_wav_header(f, SAMPLE_RATE, total_written);
    fclose(f);
    free(buf);

    ESP_LOGI(TAG, "Recording complete. File saved.");
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting...");
    sdcard_init();
    i2s_init();

    xTaskCreate(record_task, "record_task", 8192, NULL, 5, NULL);
}
