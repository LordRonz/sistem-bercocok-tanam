#include <DS3231.h>
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

// Create a new instance of the MD_Parola class with hardware SPI connection:
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

PS2Keyboard keyboard;

DS3231 myRTC;

byte ledIntensity;

unsigned long timeAlive;
unsigned long intensityThrottle;
unsigned long colonDelay;

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

enum class STATE { TIME,
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
                   SLEEP };
STATE programState;

enum class M_STATE { JAM,
                     ALARM,
                     TIMER };
M_STATE menuState;

enum class A_STATE { A1,
                     A2,
                     A3,
                     A4,
                     A5 };
A_STATE alarmState;

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
        delay(WAIT);
        return;
    }

    ledIntensitySelect(LDR_PIN);
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
                    output = getTime();
                }
                break;
            }
            case STATE::MENU: {
                switch (menuState) {
                    case M_STATE::JAM:
                        output = "JAM";
                        break;
                    case M_STATE::ALARM:
                        output = "ALARM";
                        break;
                    case M_STATE::TIMER:
                        output = "TIMER";
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
        }

        if (!isScrolling) {
            myDisplay.print(output);
        } else {
            output += SPACE;
            if (myDisplay.displayAnimate()) {
                myDisplay.displayText(output.c_str(), PA_CENTER, SCROLL_SPEED, 0, PA_SCROLL_LEFT);
            }
        }
    }

    if (Serial.available() > 0) {
        String readStr = Serial.readString();
        adjustClock(readStr);
    }
    delay(WAIT);
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
        case STATE::MENU:
            if (key == PS2_LEFTARROW) {
                switch (menuState) {
                    case M_STATE::JAM:
                        menuState = M_STATE::TIMER;
                        break;
                    case M_STATE::ALARM:
                        menuState = M_STATE::JAM;
                        break;
                    case M_STATE::TIMER:
                        menuState = M_STATE::ALARM;
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
                        menuState = M_STATE::JAM;
                        break;
                }
            } else if (key == PS2_ESC) {
                menuState = M_STATE::JAM;
                programState = STATE::TIME;
            } else if (key == PS2_ENTER) {
                if (menuState == M_STATE::JAM) {
                    programState = STATE::SET_TIME;
                } else if (menuState == M_STATE::ALARM) {
                    programState = STATE::SELECT_ALARM;
                } else {
                    programState = STATE::SET_TIMER;
                }
            }
            break;
        case STATE::SET_TIME:
            if (key == PS2_ESC) {
                programState = STATE::MENU;
            } else if (key >= '0' && key <= '9') {
                switch (inputtedClock) {
                    case 0:
                        inputClockHours = String(key) + String(inputClockHours[1]);
                        ++inputtedClock;
                        break;
                    case 1:
                        inputClockHours = String(inputClockHours[0]) + String(key);
                        ++inputtedClock;
                        break;
                    case 2:
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
            if (key == PS2_ENTER && alarm5Input.length() > 0) {
                programState = STATE::SET_ALARM;
            } else if (key == PS2_ESC) {
                programState = STATE::SELECT_ALARM;
            } else if (key == PS2_BACKSPACE) {
                if (alarm5Input.length() > 0) {
                    alarm5Input.remove(alarm5Input.length() - 1, 1);
                }
            } else {
                alarm5Input += String(key);
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
                        inputAlarmHours = String(key) + String(inputAlarmHours[1]);
                        ++inputtedAlarm;
                        break;
                    case 1:
                        inputAlarmHours = String(inputAlarmHours[0]) + String(key);
                        ++inputtedAlarm;
                        break;
                    case 2:
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
    }
}
