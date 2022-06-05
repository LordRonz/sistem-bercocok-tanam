#include <DS3231.h>
#include <LowPower.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <PS2Keyboard.h>
#include <SPI.h>
#include <Wire.h>

// BAUD RATE
#define BAUD_RATE 9600

// MAX7219
// Define hardware type, size, and output pins:
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 10
#define SCROLL_SPEED 60

// LDR
#define LDR_PIN A0

// LM35
#define LM35_PIN A1
#define TEMP_DUR 3

// Keyboard
#define KB_DATA_PIN 2
#define KB_IRQ_PIN 3

// Delay
#define WAIT 69
#define TIMEOUT 60000 * 5  // 5 mins timeour

// string constants
#define DASH String("-")
#define SPACE String(" ")
#define COLON String(":")
#define EMPTY_STR String("")
#define NAME String("Aaron Christopher Tanhar")
#define NRP String("07211940000055")
#define LODASH String("_")
#define DOUBLE_LODASH String("__")
#define ELLIPSIS String("..")

// num constants
#define TIMER_DONE_DUR 3000
#define MAX_INPUT5 100

// Create a new instance of the MD_Parola class with hardware SPI connection:
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

PS2Keyboard keyboard;

DS3231 myRTC;

byte ledIntensity;
uint16_t ledDiscoDelay;
uint16_t ledDiscoSet;

unsigned long timeAlive;
unsigned long intensityThrottle;
unsigned long lastInteraction;

float temperature;

struct Alarm {
    bool active;
    byte hour;
    byte minute;
    byte dur;  // duration in seconds
};

Alarm alarms[] = {
    {false},
    {false},
    {false},
    {false},
    {false}};

enum class STATE {
    TIME,
    MENU,
    SET_TIMER,
    SET_TIME,
    SELECT_ALARM,
    SET_ALARM,
    SET_DUR,
    ALARM_ACTIVE,
    TIMER_ACTIVE,
    TIMER_DONE,
    SET_ALARM5,
    PRE_SLEEP,
    SLEEP,
    SHALLOW_SLEEP,
    SETTING,
    TIME_MODE,
    LED_MODE,
};
STATE programState;

enum class T_MODE {
    NO_SEC,
    SEC
};
T_MODE timeMode;

enum class LED_MODE {
    NORMAL,
    DISCO
};
LED_MODE ledMode;

enum class M_STATE {
    JAM,
    ALARM,
    TIMER,
    SETTING,
};
M_STATE menuState;

enum class A_STATE {
    A1,
    A2,
    A3,
    A4,
    A5
};
A_STATE alarmState;

enum class S_STATE {
    TIME_MODE,
    LED_MODE,
};
S_STATE settingState;

String inputAlarmHours = "__";
String inputAlarmMinutes = "__";
byte inputAlarmDuration;
byte inputtedAlarm;

String alarm5Input = "";

byte activeAlarm;
unsigned long alarmStartTime;
unsigned long timerStartTime;
unsigned long timerDoneStartTime;

String inputTimerMinutes = "__";
String inputTimerSeconds = "__";
byte inputtedTimer;

String inputClockHours = "__";
String inputClockMinutes = "__";
byte inputtedClock;
int8_t setHour = -1;
int8_t setMinute = -1;

void setup() {
    // Start the serial port
    Serial.begin(BAUD_RATE);

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

    attachInterrupt(digitalPinToInterrupt(KB_DATA_PIN), keyboardHandler, FALLING);
}

void loop() {
    if (programState == STATE::SLEEP) {
        LowPower.idle(SLEEP_FOREVER, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
        return;
    }

    // Try to sleep on idle
    if (programState == STATE::TIME && millis() - lastInteraction > TIMEOUT) {
        programState = STATE::SHALLOW_SLEEP;
        myDisplay.displayClear();
    }

    if (ledMode == LED_MODE::NORMAL) {
        ledIntensitySelect(LDR_PIN);
    } else {
        ledIntensityDisco(LDR_PIN);
    }
    myDisplay.setIntensity(ledIntensity);
    myDisplay.setTextAlignment(PA_CENTER);

    if (setHour != -1 && setMinute != -1) {
        myRTC.setHour(setHour);
        myRTC.setMinute(setMinute);
        setHour = -1;
        setMinute = -1;
    }

    {
        String output;
        bool isScrolling = false;
        byte scrollSpeed = SCROLL_SPEED;
        switch (programState) {
            case STATE::TIME: {
                bool h12Flag = false;
                bool pmFlag = false;
                byte hour = myRTC.getHour(h12Flag, pmFlag);
                byte minute = myRTC.getMinute();

                for (byte j = 0; j < 5; ++j) {
                    if (alarms[j].hour == hour && alarms[j].minute == minute && alarms[j].active) {
                        programState = STATE::ALARM_ACTIVE;
                        activeAlarm = j;
                        alarmStartTime = millis();
                    }
                }

                byte curSecond = myRTC.getSecond();
                if ((curSecond >= 10 && curSecond < 10 + TEMP_DUR) || (curSecond >= 40 && curSecond < 40 + TEMP_DUR)) {
                    output = getTemp();
                } else {
                    if (timeMode == T_MODE::NO_SEC) {
                        output = getTime();
                    } else {
                        output = getTime(true);
                        isScrolling = true;
                    }
                }
                break;
            }
            case STATE::MENU: {
                switch (menuState) {
                    case M_STATE::JAM:
                        output = "Jam";
                        break;
                    case M_STATE::ALARM:
                        output = "Alarm";
                        break;
                    case M_STATE::TIMER:
                        output = "Timer";
                        break;
                    case M_STATE::SETTING:
                        output = "Setting";
                        break;
                }
                break;
            }
            case STATE::SET_TIMER: {
                output = inputTimerMinutes + COLON + inputTimerSeconds;
                if (inputtedTimer < 4) {
                    output.setCharAt(inputtedTimer < 2 ? inputtedTimer : inputtedTimer + 1, getBlinkingString("|", 500, 1)[0]);
                }
                break;
            }
            case STATE::SET_TIME: {
                output = inputClockHours + COLON + inputClockMinutes;
                if (inputtedClock < 4) {
                    output.setCharAt(inputtedClock < 2 ? inputtedClock : inputtedClock + 1, getBlinkingString("|", 500, 1)[0]);
                }
                break;
            }
            case STATE::SELECT_ALARM: {
                switch (alarmState) {
                    case A_STATE::A1:
                        output = "1";
                        break;
                    case A_STATE::A2:
                        output = "2";
                        break;
                    case A_STATE::A3:
                        output = "3";
                        break;
                    case A_STATE::A4:
                        output = "4";
                        break;
                    case A_STATE::A5:
                        output = "5";
                        break;
                }
                break;
            }
            case STATE::SET_ALARM5: {
                output = alarm5Input + getBlinkingString("|", 500);
                myDisplay.setTextAlignment(PA_LEFT);
                byte width = MAX_DEVICES * 8;
                uint16_t textWidth = myDisplay.getTextColumns(output.c_str());

                if (textWidth > width) {
                    String temp;
                    do {
                        output.remove(0, 1);
                        temp = ELLIPSIS + output;
                    } while (myDisplay.getTextColumns(temp.c_str()) > width);
                    output = temp;
                }
                break;
            }
            case STATE::SET_ALARM: {
                output = inputAlarmHours + COLON + inputAlarmMinutes;
                if (inputtedAlarm < 4) {
                    output.setCharAt(inputtedAlarm < 2 ? inputtedAlarm : inputtedAlarm + 1, getBlinkingString("|", 500, 1)[0]);
                }
                break;
            }
            case STATE::SET_DUR: {
                output = String(inputAlarmDuration) + " s";
                break;
            }
            case STATE::TIMER_ACTIVE: {
                unsigned long seconds = inputTimerMinutes.toInt() * 60 + inputTimerSeconds.toInt();
                unsigned long timeDiff = millis() - timerStartTime;
                if (timeDiff > (seconds * 1000)) {
                    programState = STATE::TIMER_DONE;
                    timerDoneStartTime = millis();
                }
                uint16_t secsLeft = seconds - (timeDiff / 1000);
                uint16_t minsLeft = secsLeft / 60;
                secsLeft -= minsLeft * 60;
                String minsOutput = String(minsLeft);
                String secsOutput = String(secsLeft);
                if (minsOutput.length() < 2) {
                    minsOutput = "0" + minsOutput;
                }
                if (secsOutput.length() < 2) {
                    secsOutput = "0" + secsOutput;
                }
                output = minsOutput + getBlinkingString(COLON, 500) + secsOutput;
                break;
            }
            case STATE::TIMER_DONE: {
                unsigned long timePassed = millis() - timerDoneStartTime;
                if (timePassed > TIMER_DONE_DUR) {
                    programState = STATE::TIME;
                }

                output = (timePassed / 500) & 1 ? EMPTY_STR : "DONE";
                break;
            }
            case STATE::ALARM_ACTIVE: {
                isScrolling = true;
                if (millis() - alarmStartTime > alarms[activeAlarm].dur * 1000) {
                    alarms[activeAlarm].active = false;
                    programState = STATE::TIME;
                    break;
                }
                switch (activeAlarm) {
                    case 0: {
                        output = NAME;
                        break;
                    }
                    case 1: {
                        output = NRP;
                        break;
                    }
                    case 2: {
                        output = NAME + SPACE + NRP;
                        break;
                    }
                    case 3: {
                        output = String(alarms[activeAlarm].dur - ((millis() - alarmStartTime) / 1000)) + " s";
                        isScrolling = false;
                        break;
                    }
                    case 4: {
                        output = alarm5Input;
                        break;
                    }
                }
                break;
            }
            case STATE::SETTING: {
                switch (settingState) {
                    case S_STATE::TIME_MODE: {
                        output = "Mode Jam";
                        scrollSpeed = 40;
                        isScrolling = true;
                        break;
                    }
                    case S_STATE::LED_MODE: {
                        output = "Mode LED";
                        scrollSpeed = 40;
                        isScrolling = true;
                        break;
                    }
                }
                break;
            }
            case STATE::TIME_MODE: {
                isScrolling = true;
                scrollSpeed = 40;
                switch (timeMode) {
                    case T_MODE::NO_SEC: {
                        output = "Tanpa detik";
                        break;
                    }
                    case T_MODE::SEC: {
                        output = "Dengan detik";
                        break;
                    }
                }
                break;
            }
            case STATE::LED_MODE: {
                switch (ledMode) {
                    case LED_MODE::NORMAL: {
                        output = "Normal";
                        break;
                    }
                    case LED_MODE::DISCO: {
                        output = "Disco";
                        break;
                    }
                }
                break;
            }
            case STATE::SHALLOW_SLEEP: {
                bool h12Flag = false;
                bool pmFlag = false;
                byte hour = myRTC.getHour(h12Flag, pmFlag);
                byte minute = myRTC.getMinute();

                for (byte j = 0; j < 5; ++j) {
                    if (alarms[j].hour == hour && alarms[j].minute == minute && alarms[j].active) {
                        programState = STATE::ALARM_ACTIVE;
                        activeAlarm = j;
                        alarmStartTime = millis();
                        lastInteraction = alarmStartTime;
                    }
                }
                break;
            }
        }

        if (programState != STATE::SHALLOW_SLEEP) {
            if (!isScrolling) {
                myDisplay.print(output);
            } else {
                if (myDisplay.displayAnimate()) {
                    myDisplay.displayScroll(output.c_str(), PA_LEFT, PA_SCROLL_LEFT, scrollSpeed);
                }
            }
        }
    }

    if (Serial.available() > 0) {
        String readStr = Serial.readString();
        adjustClock(readStr);
    }
    delay(WAIT);
}

bool isStateChanged(STATE pState, T_MODE tState, M_STATE mState, A_STATE aState, S_STATE sState) {
    return pState != programState || tState != timeMode || mState != menuState || aState != alarmState || sState != settingState;
}

void clearResetDisplay() {
    myDisplay.displayClear();
    myDisplay.displayReset();
}

void adjustClock(String& data) {
    byte hour = data.substring(0, 2).toInt();
    byte min = data.substring(3, 5).toInt();
    byte sec = data.substring(6, 8).toInt();

    // Example: >> 22:44:35

    myRTC.setHour(hour);
    myRTC.setMinute(min);
    myRTC.setSecond(sec);
}

String getTemp() {
    unsigned long timeNow = millis();
    if (!temperature || timeNow - timeAlive >= 2000) {
        do {
            temperature = (float)analogRead(LM35_PIN) / 2.0479;
        } while (temperature < 1);
        timeAlive = timeNow;
    }
    String res = String(temperature);
    res = res.substring(0, 4);
    res += " C";
    return res;
}

String getBlinkingString(String initial, uint16_t delay) {
    return (millis() / delay) & 1 ? initial : SPACE;
}

String getBlinkingString(String initial, uint16_t delay, int spaceLength) {
    String spaces;
    for (byte i = 0; i < spaceLength; ++i) {
        spaces += SPACE;
    }
    return (millis() / delay) & 1 ? initial : spaces;
}

String getTime() {
    char buf[8];
    bool h12Flag = false;
    bool pmFlag = false;

    byte hour = myRTC.getHour(h12Flag, pmFlag);
    sprintf(buf, "%02d", hour);
    String time = EMPTY_STR + buf;

    time += getBlinkingString(COLON, 500);
    byte minute = myRTC.getMinute();
    sprintf(buf, "%02d", minute);
    time += buf;

    return time;
}

String getTimeNoBlink() {
    char buf[8];
    bool h12Flag = false;
    bool pmFlag = false;

    byte hour = myRTC.getHour(h12Flag, pmFlag);
    sprintf(buf, "%02d", hour);
    String time = EMPTY_STR + buf;

    time += COLON;
    byte minute = myRTC.getMinute();
    sprintf(buf, "%02d", minute);
    time += buf;

    return time;
}

String getTime(bool withSecond) {
    String time;
    if (withSecond) {
        time = getTimeNoBlink();
        char buf[8];
        time += COLON;
        byte second = myRTC.getSecond();
        sprintf(buf, "%02d", second);
        time += buf;
    } else {
        time = getTime();
        char buf[8];
        time += getBlinkingString(COLON, 500);
        byte second = myRTC.getSecond();
        sprintf(buf, "%02d", second);
        time += buf;
    }
    return time;
}

byte getLedIntensity(const uint16_t& light) {
    uint16_t a = (light * 16) / 1023;
    return max(a, 0);
}

void ledIntensitySelect(const byte& ldrPin) {
    unsigned long timeNow = millis();

    if (timeNow - intensityThrottle >= 1000) {
        uint16_t light = analogRead(ldrPin);
        ledIntensity = getLedIntensity(light);
        intensityThrottle = timeNow;
    }
}

void ledIntensityDisco(const byte& ldrPin) {
    unsigned long timeNow = millis();
    if (timeNow - ledDiscoSet >= ledDiscoDelay) {
        ledIntensity = ledIntensity == 15 ? 0 : 15;
        ledDiscoDelay = random(50, 1000);
        ledDiscoSet = timeNow;
    }
}

void resetAlarmInput() {
    inputtedAlarm = 0;
    inputAlarmHours = "__";
    inputAlarmMinutes = "__";
    inputAlarmDuration = 0;
}

void keyboardHandler() {
    if (!keyboard.available()) {
        return;
    }
    char key = keyboard.read();
    lastInteraction = millis();
    switch (programState) {
        case STATE::TIME: {
            if (key == PS2_ENTER) {
                programState = STATE::MENU;
            } else if (key == PS2_BACKSPACE) {
                programState = STATE::SLEEP;
                myDisplay.displayClear();
            }
            break;
        }
        case STATE::MENU: {
            if (key == PS2_LEFTARROW) {
                switch (menuState) {
                    case M_STATE::JAM:
                        menuState = M_STATE::SETTING;
                        break;
                    case M_STATE::ALARM:
                        menuState = M_STATE::JAM;
                        break;
                    case M_STATE::TIMER:
                        menuState = M_STATE::ALARM;
                        break;
                    case M_STATE::SETTING:
                        menuState = M_STATE::TIMER;
                        break;
                }
            } else if (key == PS2_RIGHTARROW) {
                switch (menuState) {
                    case M_STATE::JAM:
                        menuState = M_STATE::ALARM;
                        break;
                    case M_STATE::ALARM:
                        menuState = M_STATE::TIMER;
                        break;
                    case M_STATE::TIMER:
                        menuState = M_STATE::SETTING;
                        break;
                    case M_STATE::SETTING:
                        menuState = M_STATE::JAM;
                        break;
                }
            } else if (key == PS2_ESC) {
                menuState = M_STATE::JAM;
                programState = STATE::TIME;
            } else if (key == PS2_ENTER) {
                switch (menuState) {
                    case M_STATE::JAM: {
                        programState = STATE::SET_TIME;
                        break;
                    }
                    case M_STATE::ALARM: {
                        programState = STATE::SELECT_ALARM;
                        break;
                    }
                    case M_STATE::TIMER: {
                        programState = STATE::SET_TIMER;
                        break;
                    }
                    case M_STATE::SETTING: {
                        programState = STATE::SETTING;
                        clearResetDisplay();
                        break;
                    }
                }
            }
            break;
        }
        case STATE::SET_TIME: {
            if (key == PS2_ESC) {
                programState = STATE::MENU;
            } else if (key >= '0' && key <= '9') {
                switch (inputtedClock) {
                    case 0:
                        if (key > '2') {
                            break;
                        }
                        inputClockHours = String(key) + String(inputClockHours[1]);
                        ++inputtedClock;
                        break;
                    case 1:
                        if (inputClockHours[0] > '1' && key > '3') {
                            break;
                        }
                        inputClockHours = String(inputClockHours[0]) + String(key);
                        ++inputtedClock;
                        break;
                    case 2:
                        if (key > '5') {
                            break;
                        }
                        inputClockMinutes = String(key) + String(inputClockMinutes[1]);
                        ++inputtedClock;
                        break;
                    case 3:
                        inputClockMinutes = String(inputClockMinutes[0]) + String(key);
                        ++inputtedClock;
                        break;
                }
            } else if (key == PS2_BACKSPACE) {
                switch (inputtedClock) {
                    case 1:
                        inputClockHours = "__";
                        --inputtedClock;
                        break;
                    case 2:
                        inputClockHours = String(inputClockHours[0]) + "_";
                        --inputtedClock;
                        break;
                    case 3:
                        inputClockMinutes = "__";
                        --inputtedClock;
                        break;
                    case 4:
                        inputClockMinutes = String(inputClockMinutes[0]) + "_";
                        --inputtedClock;
                        break;
                }
            } else if (key == PS2_ENTER) {
                if (inputtedClock >= 4) {
                    setHour = inputClockHours.toInt();
                    setMinute = inputClockMinutes.toInt();

                    programState = STATE::TIME;
                }
            }
            break;
        }
        case STATE::SELECT_ALARM: {
            if (key == PS2_ESC) {
                programState = STATE::MENU;
            } else if (key == PS2_LEFTARROW) {
                switch (alarmState) {
                    case A_STATE::A1:
                        alarmState = A_STATE::A5;
                        break;
                    case A_STATE::A2:
                        alarmState = A_STATE::A1;
                        break;
                    case A_STATE::A3:
                        alarmState = A_STATE::A2;
                        break;
                    case A_STATE::A4:
                        alarmState = A_STATE::A3;
                        break;
                    case A_STATE::A5:
                        alarmState = A_STATE::A4;
                        break;
                }
            } else if (key == PS2_RIGHTARROW) {
                switch (alarmState) {
                    case A_STATE::A1:
                        alarmState = A_STATE::A2;
                        break;
                    case A_STATE::A2:
                        alarmState = A_STATE::A3;
                        break;
                    case A_STATE::A3:
                        alarmState = A_STATE::A4;
                        break;
                    case A_STATE::A4:
                        alarmState = A_STATE::A5;
                        break;
                    case A_STATE::A5:
                        alarmState = A_STATE::A1;
                        break;
                }
            } else if (key == PS2_ENTER) {
                if (alarmState == A_STATE::A5) {
                    programState = STATE::SET_ALARM5;
                } else {
                    programState = STATE::SET_ALARM;
                }
            }
            break;
        }
        case STATE::SET_ALARM5: {
            uint16_t inputLen = alarm5Input.length();
            if (key == PS2_ENTER && inputLen > 0) {
                programState = STATE::SET_ALARM;
            } else if (key == PS2_ESC) {
                programState = STATE::SELECT_ALARM;
            } else if (key == PS2_BACKSPACE) {
                if (inputLen > 0) {
                    alarm5Input.remove(inputLen - 1, 1);
                }
            } else {
                if (inputLen < 100) {
                    alarm5Input += String(key);
                }
            }
            break;
        }
        case STATE::SET_ALARM: {
            if (key == PS2_ESC) {
                programState = alarmState != A_STATE::A5 ? STATE::SELECT_ALARM : STATE::SET_ALARM5;
                resetAlarmInput();
            } else if (key >= '0' && key <= '9') {
                switch (inputtedAlarm) {
                    case 0:
                        if (key > '2') {
                            break;
                        }
                        inputAlarmHours = String(key) + String(inputAlarmHours[1]);
                        ++inputtedAlarm;
                        break;
                    case 1:
                        if (inputAlarmHours[0] > '1' && key > '3') {
                            break;
                        }
                        inputAlarmHours = String(inputAlarmHours[0]) + String(key);
                        ++inputtedAlarm;
                        break;
                    case 2:
                        if (key > '5') {
                            break;
                        }
                        inputAlarmMinutes = String(key) + String(inputAlarmMinutes[1]);
                        ++inputtedAlarm;
                        break;
                    case 3:
                        inputAlarmMinutes = String(inputAlarmMinutes[0]) + String(key);
                        ++inputtedAlarm;
                        break;
                }
            } else if (key == PS2_BACKSPACE) {
                switch (inputtedAlarm) {
                    case 1:
                        inputAlarmHours = DOUBLE_LODASH;
                        --inputtedAlarm;
                        break;
                    case 2:
                        inputAlarmHours = String(inputAlarmHours[0]) + LODASH;
                        --inputtedAlarm;
                        break;
                    case 3:
                        inputAlarmMinutes = DOUBLE_LODASH;
                        --inputtedAlarm;
                        break;
                    case 4:
                        inputAlarmMinutes = String(inputAlarmMinutes[0]) + LODASH;
                        --inputtedAlarm;
                        break;
                }
            } else if (key == PS2_ENTER) {
                if (inputtedAlarm >= 4) {
                    programState = STATE::SET_DUR;
                }
            }
            break;
        }
        case STATE::SET_DUR: {
            if (key == PS2_ESC) {
                programState = STATE::SET_ALARM;
            } else if (key == PS2_UPARROW) {
                ++inputAlarmDuration;
            } else if (key == PS2_DOWNARROW) {
                --inputAlarmDuration;
            } else if (key == PS2_ENTER && inputAlarmDuration) {
                if (inputtedAlarm >= 4) {
                    programState = STATE::SET_DUR;
                    byte index = 0;
                    switch (alarmState) {
                        case A_STATE::A1:
                            index = 0;
                            break;
                        case A_STATE::A2:
                            index = 1;
                            break;
                        case A_STATE::A3:
                            index = 2;
                            break;
                        case A_STATE::A4:
                            index = 3;
                            break;
                        case A_STATE::A5:
                            index = 4;
                            break;
                    }
                    alarms[index].active = true;
                    alarms[index].hour = inputAlarmHours.toInt();
                    alarms[index].minute = inputAlarmMinutes.toInt();
                    alarms[index].dur = inputAlarmDuration;
                    resetAlarmInput();
                    programState = STATE::TIME;
                }
            }
            break;
        }
        case STATE::ALARM_ACTIVE: {
            if (key == PS2_ESC) {
                programState = STATE::TIME;
                alarms[activeAlarm].active = false;
            }
            break;
        }
        case STATE::SET_TIMER: {
            if (key == PS2_ESC) {
                programState = STATE::MENU;
            } else if (key >= '0' && key <= '9') {
                switch (inputtedTimer) {
                    case 0:
                        inputTimerMinutes = String(key) + String(inputTimerMinutes[1]);
                        ++inputtedTimer;
                        break;
                    case 1:
                        inputTimerMinutes = String(inputTimerMinutes[0]) + String(key);
                        ++inputtedTimer;
                        break;
                    case 2:
                        if (inputTimerMinutes == String("99") && key > '5') {
                            break;
                        }
                        inputTimerSeconds = String(key) + String(inputTimerSeconds[1]);
                        ++inputtedTimer;
                        break;
                    case 3:
                        inputTimerSeconds = String(inputTimerSeconds[0]) + String(key);
                        ++inputtedTimer;
                        break;
                }
            } else if (key == PS2_BACKSPACE) {
                switch (inputtedTimer) {
                    case 1:
                        inputTimerMinutes = DOUBLE_LODASH;
                        --inputtedTimer;
                        break;
                    case 2:
                        inputTimerMinutes = String(inputTimerMinutes[0]) + LODASH;
                        --inputtedTimer;
                        break;
                    case 3:
                        inputTimerSeconds = DOUBLE_LODASH;
                        --inputtedTimer;
                        break;
                    case 4:
                        inputTimerSeconds = String(inputTimerSeconds[0]) + LODASH;
                        --inputtedTimer;
                        break;
                }
            } else if (key == PS2_ENTER) {
                if (inputtedTimer >= 4) {
                    programState = STATE::TIMER_ACTIVE;
                    timerStartTime = millis();
                }
            }
            break;
        }
        case STATE::TIMER_ACTIVE: {
            if (key == PS2_ESC) {
                programState = STATE::TIME;
            }
            break;
        }
        case STATE::SLEEP: {
            programState = STATE::TIME;
            break;
        }
        case STATE::SHALLOW_SLEEP: {
            programState = STATE::TIME;
            break;
        }
        case STATE::SETTING: {
            if (key == PS2_LEFTARROW) {
                switch (settingState) {
                    case S_STATE::TIME_MODE: {
                        settingState = S_STATE::LED_MODE;
                        break;
                    }
                    case S_STATE::LED_MODE: {
                        settingState = S_STATE::TIME_MODE;
                        break;
                    }
                }
                clearResetDisplay();
            } else if (key == PS2_RIGHTARROW) {
                switch (settingState) {
                    case S_STATE::TIME_MODE: {
                        settingState = S_STATE::LED_MODE;
                        break;
                    }
                    case S_STATE::LED_MODE: {
                        settingState = S_STATE::TIME_MODE;
                        break;
                    }
                }
                clearResetDisplay();
            } else if (key == PS2_ESC) {
                programState = STATE::MENU;
            } else if (key == PS2_ENTER) {
                if (settingState == S_STATE::TIME_MODE) {
                    programState = STATE::TIME_MODE;
                } else {
                    programState = STATE::LED_MODE;
                }
                clearResetDisplay();
            }
            break;
        }
        case STATE::TIME_MODE: {
            if (key == PS2_LEFTARROW) {
                switch (timeMode) {
                    case T_MODE::NO_SEC: {
                        timeMode = T_MODE::SEC;
                        break;
                    }
                    case T_MODE::SEC: {
                        timeMode = T_MODE::NO_SEC;
                        break;
                    }
                }
                clearResetDisplay();
            } else if (key == PS2_RIGHTARROW) {
                switch (timeMode) {
                    case T_MODE::NO_SEC: {
                        timeMode = T_MODE::SEC;
                        break;
                    }
                    case T_MODE::SEC: {
                        timeMode = T_MODE::NO_SEC;
                        break;
                    }
                }
                clearResetDisplay();
            } else if (key == PS2_ESC) {
                programState = STATE::SETTING;
                clearResetDisplay();
            } else if (key == PS2_ENTER) {
                programState = STATE::TIME;
            }
            break;
        }
        case STATE::LED_MODE: {
            if (key == PS2_LEFTARROW) {
                switch (ledMode) {
                    case LED_MODE::NORMAL: {
                        ledMode = LED_MODE::DISCO;
                        break;
                    }
                    case LED_MODE::DISCO: {
                        ledMode = LED_MODE::NORMAL;
                        break;
                    }
                }
            } else if (key == PS2_RIGHTARROW) {
                switch (ledMode) {
                    case LED_MODE::NORMAL: {
                        ledMode = LED_MODE::DISCO;
                        break;
                    }
                    case LED_MODE::DISCO: {
                        ledMode = LED_MODE::NORMAL;
                        break;
                    }
                }
            } else if (key == PS2_ESC) {
                programState = STATE::SETTING;
                clearResetDisplay();
            } else if (key == PS2_ENTER) {
                programState = STATE::TIME;
            }
            break;
        }
    }
}
