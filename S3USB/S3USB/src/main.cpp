#include <Arduino.h>
#include <USB.h>
#include <USBMSC.h>
#include <SD_MMC.h>

USBMSC msc;

// Pins
#define SDMMC_CLK 14
#define SDMMC_CMD 15
#define SDMMC_D0 16
#define SDMMC_D1 17
#define SDMMC_D2 18
#define SDMMC_D3 21
#define SD_POWER_PIN 13

// Buffer for sector transfer
uint8_t sector_buf[512] __attribute__((aligned(4)));

// Simple read function
int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  File f = SD_MMC.open("/usb.img", FILE_READ);
  if (!f) return 0;
  f.seek(lba * 512 + offset);
  int bytes = f.read((uint8_t *)buffer, bufsize);
  f.close();
  return bytes;
}

// Write not supported (read-only)
int32_t onWrite(uint32_t, uint32_t, uint8_t *, uint32_t) {
  return 0; // read-only
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(SD_POWER_PIN, OUTPUT);
  digitalWrite(SD_POWER_PIN, HIGH);
  delay(100);

  // Set SDMMC pins and begin
  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
  if (!SD_MMC.begin("/sdcard", true, true)) {
    Serial.println("SD_MMC mount failed!");
    while (1);
  }

  // USB MSC setup
  msc.vendorID("ESP32");
  msc.productID("SDCardMSC");
  msc.productRevision("1.0");
  msc.onRead(onRead);
  msc.onWrite(onWrite);  // Set to NULL if read-only
  msc.mediaPresent(true);
  msc.begin(SD_MMC.cardSize() / (512 / 512), 512); // total blocks, block size

  USB.begin();
  Serial.println("USB MSC ready!");
}

void loop() {
  delay(100);
}