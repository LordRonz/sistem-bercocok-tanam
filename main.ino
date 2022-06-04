#include <DS3231.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <PS2Keyboard.h>

// BAUD RATE
#define BAUD_RATE 9600

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

// string constants
#define DASH String("-")
#define SPACE String(" ")
#define COLON String(":")
#define EMPTY_STR String("")
#define NAME String("Aaron Christopher Tanhar")
#define NRP String("07211940000055")
#define LODASH String("_")
#define DOUBLE_LODASH String("__")

// Create a new instance of the MD_Parola class with hardware SPI connection:
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

PS2Keyboard keyboard;

DS3231 myRTC;

byte ledIntensity;

unsigned long timeAlive;
unsigned long intensityThrottle;
unsigned long colonDelay;

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
                   SET_ALARM5 };
STATE program_state;

enum class M_STATE { JAM,
                     ALARM,
                     TIMER };
M_STATE menu_state;

enum class A_STATE { A1,
                     A2,
                     A3,
                     A4,
                     A5 };
A_STATE alarm_state;

String inputAlarmHours = "__";
String inputAlarmMinutes = "__";
byte inputAlarmDuration = 0;
byte inputtedAlarm = 0;

String alarm5Input = "";

byte activeAlarm = 0;
unsigned long alarmStartTime = 0;
unsigned long timerStartTime = 0;

String inputTimerMinutes = "__";
String inputTimerSeconds = "__";
byte inputtedTimer = 0;

String inputClockHours = "__";
String inputClockMinutes = "__";
byte inputtedClock = 0;

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
    ledIntensitySelect(LDR_PIN);
    myDisplay.setIntensity(ledIntensity);

    byte curSecond = myRTC.getSecond();

    {
        String output;
        switch (program_state) {
            case STATE::TIME: {
                if ((curSecond >= 10 && curSecond < 15) || (curSecond >= 40 && curSecond < 45)) {
                    output = getTemp();
                } else {
                    output = getTime();
                }
                break;
            }
            case STATE::MENU: {
                switch (menu_state) {
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
            case STATE::SET_TIMER:
                output = inputTimerMinutes + ":" + inputTimerSeconds;
                break;
            case STATE::SET_TIME:
                output = inputClockHours + ":" + inputClockMinutes;
                break;
            case STATE::SELECT_ALARM:
                switch (alarm_state) {
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
            case STATE::SET_ALARM5:
                output = alarm5Input;
                break;
            case STATE::SET_ALARM:
                output = inputAlarmHours + COLON + inputAlarmMinutes;
                break;
            case STATE::SET_DUR:
                output = String(inputAlarmDuration) + " s";
                break;
            case STATE::TIMER_ACTIVE:
                unsigned int seconds = inputTimerMinutes.toInt() * 60 + inputTimerSeconds.toInt();
                unsigned long timeDiff = millis() - timerStartTime;
                if (timeDiff > (seconds * 1000)) {
                    program_state = STATE::TIME;
                }
                unsigned int secsLeft = seconds - (timeDiff / 1000);
                unsigned int minsLeft = secsLeft / 60;
                secsLeft -= minsLeft * 60;
                String minsOutput = String(minsLeft);
                String secsOutput = String(secsLeft);
                if (minsOutput.length() < 2) {
                    minsOutput = "0" + minsOutput;
                }
                if (secsOutput.length() < 2) {
                    secsOutput = "0" + secsOutput;
                }
                output = minsOutput + COLON + secsOutput;
                break;
        }
        myDisplay.print(output);
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
    float temperature;
    if (!timeAlive || timeNow - timeAlive >= 1000) {
        temperature = (float)analogRead(LM35_PIN) / 2.0479;
        timeAlive = timeNow;
    }
    String res = String(temperature);
    res = res.substring(0, 4);
    res += " C";
    return res;
}

String getTime() {
    char buf[8];
    bool h12Flag = false;
    bool pmFlag = false;

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
    unsigned char key = keyboard.read();
    switch (program_state) {
        case STATE::TIME:
        case STATE::TEMP:
            if (key == PS2_ENTER) {
                program_state = STATE::MENU;
            }
            break;
        case STATE::MENU:
            if (key == PS2_LEFTARROW) {
                switch (menu_state) {
                    case M_STATE::JAM:
                        menu_state = M_STATE::TIMER;
                        break;
                    case M_STATE::ALARM:
                        menu_state = M_STATE::JAM;
                        break;
                    case M_STATE::TIMER:
                        menu_state = M_STATE::ALARM;
                        break;
                }
            } else if (key == PS2_RIGHTARROW) {
                switch (menu_state) {
                    case M_STATE::JAM:
                        menu_state = M_STATE::ALARM;
                        break;
                    case M_STATE::ALARM:
                        menu_state = M_STATE::TIMER;
                        break;
                    case M_STATE::TIMER:
                        menu_state = M_STATE::JAM;
                        break;
                }
            } else if (key == PS2_ESC) {
                menu_state = M_STATE::JAM;
                program_state = STATE::TIME;
            } else if (key == PS2_ENTER) {
                if (menu_state == M_STATE::JAM) {
                    program_state = STATE::SET_TIME;
                } else if (menu_state == M_STATE::ALARM) {
                    program_state = STATE::SELECT_ALARM;
                } else {
                    program_state = STATE::SET_TIMER;
                }
            }
            break;
        case STATE::SET_TIME:
            if (key == PS2_ESC) {
                program_state = STATE::MENU;
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
                    program_state = STATE::TIME;
                    byte hour = inputClockHours.toInt();
                    byte min = inputClockMinutes.toInt();
                    myRTC.setHour(hour);
                    myRTC.setMinute(min);
                    program_state = STATE::TIME;
                }
            }
            break;
        case STATE::SELECT_ALARM:
            if (key == PS2_ESC) {
                program_state = STATE::MENU;
            } else if (key == PS2_LEFTARROW) {
                switch (alarm_state) {
                    case A_STATE::A1:
                        alarm_state = A_STATE::A5;
                        break;
                    case A_STATE::A2:
                        alarm_state = A_STATE::A1;
                        break;
                    case A_STATE::A3:
                        alarm_state = A_STATE::A2;
                        break;
                    case A_STATE::A4:
                        alarm_state = A_STATE::A3;
                        break;
                    case A_STATE::A5:
                        alarm_state = A_STATE::A4;
                        break;
                }
            } else if (key == PS2_RIGHTARROW) {
                switch (alarm_state) {
                    case A_STATE::A1:
                        alarm_state = A_STATE::A2;
                        break;
                    case A_STATE::A2:
                        alarm_state = A_STATE::A3;
                        break;
                    case A_STATE::A3:
                        alarm_state = A_STATE::A4;
                        break;
                    case A_STATE::A4:
                        alarm_state = A_STATE::A5;
                        break;
                    case A_STATE::A5:
                        alarm_state = A_STATE::A1;
                        break;
                }
            } else if (key == PS2_ENTER) {
                if (alarm_state == A_STATE::A5) {
                    program_state = STATE::SET_ALARM5;
                } else {
                    program_state = STATE::SET_ALARM;
                }
            }
            break;
        case STATE::SET_ALARM5:
            if (key == PS2_ENTER && alarm5Input.length() > 0) {
                program_state = STATE::SET_ALARM;
            } else if (key == PS2_ESC) {
                program_state = STATE::SELECT_ALARM;
            } else if (key == PS2_BACKSPACE && alarm5Input.length() > 0) {
                alarm5Input.remove(alarm5Input.length() - 1, 1);
            } else {
                alarm5Input += String(key);
            }
            break;
        case STATE::SET_ALARM:
            if (key == PS2_ESC) {
                program_state = STATE::SELECT_ALARM;
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
                    program_state = STATE::SET_DUR;
                }
            }
            break;
        case STATE::SET_DUR:
            if (key == PS2_ESC) {
                program_state = STATE::SET_ALARM;
            } else if (key == PS2_UPARROW) {
                ++inputAlarmDuration;
            } else if (key == PS2_DOWNARROW) {
                --inputAlarmDuration;
            } else if (key == PS2_ENTER && inputAlarmDuration) {
                if (inputtedAlarm >= 4) {
                    program_state = STATE::SET_DUR;
                    byte index = 0;
                    switch (alarm_state) {
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
                    program_state = STATE::TIME;
                }
            }
            break;
        case STATE::ALARM_ACTIVE:
            if (key == PS2_ESC) {
                program_state = STATE::TIME;
                alarms[activeAlarm].active = false;
            }
            break;
        case STATE::SET_TIMER:
            if (key == PS2_ESC) {
                program_state = STATE::MENU;
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
                    program_state = STATE::TIMER_ACTIVE;
                    timerStartTime = millis();
                }
            }
            break;
        case STATE::TIMER_ACTIVE:
            if (key == PS2_ESC) {
                program_state = STATE::TIME;
            }
            break;
    }
}
