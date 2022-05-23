#include <DS3231.h>
#include <Wire.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// MAX7219
// Define hardware type, size, and output pins:
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 3

// LDR
#define LDR_PIN A0

// LM35
#define LM35_PIN A1

// Create a new instance of the MD_Parola class with hardware SPI connection:
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);


DS3231 myRTC;

byte year;
byte month;
byte date;
byte dow;
byte hour;
byte minute;
byte second;

unsigned long timeAlive = 0;

bool century = false;
bool h12Flag = false;
bool pmFlag = false;

String DASH = "-";
String SPACE = " ";
String COLON = ":";
String EMPTY_STR = "";

char *NAMA = "Aaron Christopher";
char *NRP = "07211940000055";

String temperature = EMPTY_STR;

char buf[100];

enum State { nama, waktu };
State state = waktu;

void setup() {
    // Start the serial port
    Serial.begin(9600);

    // Start the I2C interface
    Wire.begin();

    myRTC.setYear(0);
    myRTC.setMonth(0);
    myRTC.setDate(0);
    myRTC.setDoW(0);
    myRTC.setHour(0);
    myRTC.setMinute(0);
    myRTC.setSecond(0);

    // Initialize the object:
    myDisplay.begin();
    // Set the intensity (brightness) of the display (0-15):
    myDisplay.setIntensity(0);
    // Clear the display:
    myDisplay.displayClear();

    setTime();

    // myDisplay.displayScroll(buf, PA_CENTER, PA_SCROLL_LEFT, 60);

    Serial.println("Setup Complete!");
}

void loop() {
    switch(state) {
        case nama:
            setTemp();
            break;
        case waktu:
            setTime();
            break;
    }

    myDisplay.setIntensity(ledIntensitySelect(analogRead(LDR_PIN)));

    String toBePrinted = EMPTY_STR;

    byte curSecond = myRTC.getSecond();

    // Serial.println(getTemp());

    if ((curSecond >= 10 && curSecond <= 15) || (curSecond >= 40 && curSecond <= 45)) {
        toBePrinted += getTemp();
    } else {
        toBePrinted += getTime();
    }

    setTemp();

    myDisplay.print(toBePrinted);
}

String getTemp() {
    unsigned long timeNow = millis();

    if (timeAlive == 0 || timeNow - timeAlive >= 1000) {
        temperature = String((float) analogRead(LM35_PIN) / 2.0479);
        timeAlive = timeNow;
    }
    return temperature.substring(0, 4);
}

void setTemp() {
    String t = EMPTY_STR;
    t += getTemp();
    Serial.println(t);
    t.toCharArray(buf, 100);
}

void setNama() {
    String n = NAMA;

    n.toCharArray(buf, 100);
}

String getTime() {
    char buf[30];
    int hour = myRTC.getHour(h12Flag, pmFlag);
    sprintf(buf, "%02d", hour);
    String time = EMPTY_STR + buf;
    time += COLON;
    int minute = myRTC.getMinute();
    sprintf(buf, "%02d", minute);
    time += buf;

    return time;
}

void setTime() {
    String time = getTime();

    time.toCharArray(buf, 100);
}

byte ledIntensitySelect(int light) {
    int a = abs(light - 1023) * 16;
    a /= 1023;
    --a;
    return abs(a);
}
