#include <Adafruit_NeoPixel.h>

#define LED_PIN       48  // WS2812B LED
#define NUM_LEDS      1   // Single LED
#define BATTERY_PIN   7   // ADC pin for battery voltage
#define CHARGING_PIN  6   // TP4056 CHRG Pin (LOW = Charging)
#define FULL_PIN      5   // TP4056 DONE Pin (LOW = Full)

Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    pinMode(CHARGING_PIN, INPUT_PULLUP);
    pinMode(FULL_PIN, INPUT_PULLUP);
    led.begin();
    led.show();
}

float readBatteryVoltage() {
    int raw = analogRead(BATTERY_PIN);
    return (raw / 4095.0) * 3.3 * ((100.0 + 47.0) / 47.0);  // Convert ADC to real voltage
}

bool isCharging() {
    return digitalRead(CHARGING_PIN) == LOW;
}

bool isBatteryFull() {
    return digitalRead(FULL_PIN) == LOW;
}

void setLED(int r, int g, int b, bool blink) {
    if (blink) {
        led.setPixelColor(0, led.Color(r, g, b));
        led.show();
        delay(500);
        led.setPixelColor(0, led.Color(0, 0, 0));  // Turn off
        led.show();
        delay(500);
    } else {
        led.setPixelColor(0, led.Color(r, g, b));
        led.show();
    }
}

void loop() {
    float batteryVoltage = readBatteryVoltage();
    bool charging = isCharging();
    bool full = isBatteryFull();

    Serial.printf("Battery Voltage: %.2fV\n", batteryVoltage);

    if (charging) {
        Serial.println("Battery Charging...");
        setLED(255, 0, 0, true);  // Blinking Red
    } else if (full) {
        Serial.println("Battery Full!");
        setLED(0, 255, 0, false);  // Solid Green
    } else if (batteryVoltage < 3.0) {
        Serial.println("Low Battery Warning!");
        setLED(255, 255, 0, true);  // Blinking Yellow
    } else {
        Serial.println("Battery OK, System Running...");
        setLED(0, 0, 255, false);  // Solid Blue
    }

    delay(1000);
}
