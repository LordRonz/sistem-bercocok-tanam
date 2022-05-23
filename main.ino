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

// Create a new instance of the MD_Parola class with hardware SPI connection:
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

PS2Keyboard keyboard;

DS3231 myRTC;

byte year;
byte month;
byte date;
byte dow;
byte hour;
byte minute;
byte second;

bool century = false;
bool h12Flag = false;
bool pmFlag = false;

const byte WAIT = 60;

const String DASH = "-";
const String SPACE = " ";
const String COLON = ":";
const String EMPTY_STR = "";

const char *NAMA = "Aaron Christopher";
const char *NRP = "07211940000055";

unsigned long timeAlive = 0;

float temperature;

char buf[100];

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

    setTime();

    Serial.println("Setup Complete!");
}

void loop() {
    myDisplay.setIntensity(ledIntensitySelect(analogRead(LDR_PIN)));

    String toBePrinted = EMPTY_STR;

    byte curSecond = myRTC.getSecond();

    if ((curSecond >= 10 && curSecond <= 15) || (curSecond >= 40 && curSecond <= 45)) {
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
