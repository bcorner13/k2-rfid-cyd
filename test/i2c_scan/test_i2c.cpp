#include <Arduino.h>
#include <Wire.h>

TwoWire I2C_CAN = TwoWire(1);

#define PN532_RESET 2
#define SDA_PIN 8
#define SCL_PIN 9

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // 1. Physical Reset to wake the PN532
    pinMode(PN532_RESET, OUTPUT);
    digitalWrite(PN532_RESET, LOW);
    delay(50);
    digitalWrite(PN532_RESET, HIGH);
    delay(50);

    // 2. Start I2C on the CAN pins (GPIO 16/15)
    I2C_CAN.begin(SDA_PIN, SCL_PIN, 50000);
    Serial.println("--- Scanner Ready ---");
}

void loop() {
    byte error, address;
    int nDevices = 0;
    Serial.println("Scanning...");

    for (address = 1; address < 127; address++) {
        I2C_CAN.beginTransmission(address);
        error = I2C_CAN.endTransmission();

        if (error == 0) {
            Serial.print("Found at 0x");
            Serial.print(address, HEX);
            if (address == 0x24) Serial.println(" (PN532!)");
            else Serial.println();
            nDevices++;
        }
        else if (error == 4) {
            Serial.print("Bus Error at 0x");
            Serial.println(address, HEX);
        }
    }

    if (nDevices == 0) Serial.println("No devices found. Swap L/H.");
    delay(5000);
}