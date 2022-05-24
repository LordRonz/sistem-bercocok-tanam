#include <DS3231.h>
#include <Wire.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <PS2Keyboard.h>
#include <SPI.h>

// MAX7219
// Define hardware type, size, and output pins:
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 10

// LDR
#define LDR_PIN A0

// LM35
#define LM35_PIN A1

// Keyboard
#define KB_DATA_PIN 2
#define KB_IRQ_PIN 3

// Delay
#define WAIT 69

// Create a new instance of the MD_Parola class with hardware SPI connection:
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

PS2Keyboard keyboard;

DS3231 myRTC;

bool h12Flag = false;
bool pmFlag = false;

byte ledIntensity = 0;

const String DASH = "-";
const String SPACE = " ";
const String COLON = ":";
const String EMPTY_STR = "";

const char NAMA[] PROGMEM = "Aaron Christopher";
const char NRP[] PROGMEM = "07211940000055";

unsigned long timeAlive = 0;
unsigned long intensityThrottle = 0;
unsigned long colonDelay = 0;

enum State { nama, waktu };
State state = waktu;

void setup() {
    // Start the serial port
    Serial.begin(9600);

    keyboard.begin(KB_DATA_PIN, KB_IRQ_PIN);

    // Start the I2C interface
    Wire.begin();

    // Initialize the object:
    myDisplay.begin();
    // Set the intensity (brightness) of the display (0-15):
    myDisplay.setIntensity(0);
    // Clear the display:
    myDisplay.displayClear();

    myDisplay.setTextAlignment(PA_CENTER);

    Serial.println(F("Setup Complete!"));
}

void loop() {
    ledIntensitySelect(LDR_PIN);
    myDisplay.setIntensity(ledIntensity);

    String toBePrinted = EMPTY_STR;

    byte curSecond = myRTC.getSecond();

    if ((curSecond >= 10 && curSecond < 15) || (curSecond >= 40 && curSecond < 45)) {
        toBePrinted += getTemp();
    } else {
        toBePrinted += getTime();
    }

    myDisplay.print(toBePrinted);

    if (Serial.available() > 0) {
        adjustClock(Serial.readString());
    }

    delay(WAIT);
}

String progmemCharsToString(const char* s) {
    String res;
    char buf[2];
    for (byte k = 0; k < strlen_P(s); k++) {
        buf[0] = pgm_read_byte_near(s + k);
        buf[1] = '\0';
        res.concat(buf);
    }
    return res;
}

void adjustClock(String data) {
    byte hour = data.substring(0,2).toInt();
    byte min = data.substring(3,5).toInt();
    byte sec = data.substring(6,8).toInt();

    // Example: >> 22:44:35

    myRTC.setHour(hour);
    myRTC.setMinute(min);
    myRTC.setSecond(sec);

    Serial.println(F(">> Datetime successfully set!"));
}

String getTemp() {
    unsigned long timeNow = millis();
    float temperature;
    if (timeAlive == 0 || timeNow - timeAlive >= 1000) {
        temperature = (float)analogRead(LM35_PIN) / 2.0479;
        timeAlive = timeNow;
    }
    String res;
    res.concat(temperature);
    res = res.substring(0, 4);
    res.concat(" ");
    res.concat("C");
    return res;
}

String getTime() {
    char buf[30];
    byte hour = myRTC.getHour(h12Flag, pmFlag);
    sprintf(buf, "%02d", hour);
    String time = EMPTY_STR + buf;
    unsigned long timeNow = millis();
    if (timeNow - colonDelay >= 0 && timeNow - colonDelay <= 1000) {
        time += COLON;
    } else if (timeNow - colonDelay > 1000) {
        time += SPACE;
        if (timeNow - colonDelay >= 1500) {
            colonDelay = timeNow;
        }
    }
    byte minute = myRTC.getMinute();
    sprintf(buf, "%02d", minute);
    time += buf;

    return time;
}

byte getLedIntensity(int light) {
    int a = abs(light - 1023) * 16;
    a /= 1023;
    --a;
    return abs(a);
}

void ledIntensitySelect(uint8_t ldrPin) {
    unsigned long timeNow = millis();

    if (timeNow - intensityThrottle >= 1000) {
        int light = analogRead(ldrPin);
        ledIntensity = getLedIntensity(light);
        intensityThrottle = timeNow;
    }
}
