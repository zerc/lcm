#include "Arduino_LED_Matrix.h"
#include <aWOT.h>
#include <SPI.h>
#include <WiFiS3.h>
#include <AceButton.h>
using namespace ace_button;

#include "secrets.h"
#include "index.cpp"

#ifndef SSID
#error "SSID is not defined. Please define it in secrets.h"
#endif

#ifndef PASS
#error "PASS is not defined. Please define it in secrets.h"
#endif

// Matrix setup
ArduinoLEDMatrix matrix;
// End Matrix setup

// WiFi setup
WiFiServer server(80);
char ssid[] = SSID;
char pass[] = PASS;
int status = WL_IDLE_STATUS;
Application app;
void changeOutput(Request &req, Response &res);
void changeAll(Request &req, Response &res);
void getInfo(Request &req, Response &res);
void getIndex(Request &req, Response &res);
void headers(Request &req, Response &res);
// End WiFi setup

// AceButton configuration
const uint8_t TOTAL_NUM_BUTTONS = 16;
const uint8_t HARD_NUM_BUTTONS = 16; // number of buttons available via hardware switches

struct Info
{
    const uint8_t inputPin;
    const uint8_t outputPin;
    bool outputState;
    bool isToggle; // can be turned on/off by repeated activation
    char *label;
};

Info INFOS[TOTAL_NUM_BUTTONS] = {
    {A0, 2, HIGH, true, "hallway"},           // 3  the same as A2 - two switches should connect to the same output
    {A1, 8, HIGH, true, "bed 2"},             // 2
    {A2, 2, HIGH, true, "hallway"},           // 3
    {8, 15, HIGH, false, "kitchen main"},     // 12
    {A4, 3, HIGH, false, "bed 1 main"},       // 5
    {A5, 14, HIGH, false, "bed 1 cup"},       // 6
    {10, 4, HIGH, false, "kitchen cabinets"}, // 14
    {7, 13, HIGH, false, "living main"},      // 11
    {5, 5, HIGH, false, "living dining"},     // 9
    {6, 12, HIGH, false, "living walls"},     // 10
    {4, 6, HIGH, false, "button 11"},         // 8
    {A3, 11, HIGH, false, "hallway 2"},       // 4
    {9, 7, HIGH, false, "kitchen counter"},   // 13
    {3, 10, HIGH, false, "button 14"},        // 7
    {11, 0, HIGH, false, "button 15"},        // 15
    {12, 9, HIGH, false, "button 16"},        // 16
};

AceButton buttons[HARD_NUM_BUTTONS];

void handleEvent(AceButton *, uint8_t, uint8_t);
// End AceButton configuration

const int CLOCK_PIN = 0; // Pin connected to clock pin (SH_CP) of 74HC595
const int LATCH_PIN = 1; // Pin connected to latch pin (ST_CP) of 74HC595
const int DATA_PIN = 2;  // Pin connected to Data in (DS) of 74HC595

// the bits you want to send to the register
byte BITSTOSEND = 0xFF;
byte BITSTOSEND2 = 0xFF;
bool WARMUP = true;

void setup()
{
    delay(1000); // some microcontrollers reboot twice

    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);

    pinMode(LATCH_PIN, OUTPUT);
    pinMode(DATA_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);

    // AceButton setup
    for (uint8_t i = 0; i < HARD_NUM_BUTTONS; i++)
    {
        pinMode(INFOS[i].outputPin, OUTPUT);
        pinMode(INFOS[i].inputPin, INPUT);
        buttons[i].init(INFOS[i].inputPin, INFOS[i].outputState, i);
    }

    ButtonConfig *buttonConfig = ButtonConfig::getSystemButtonConfig();
    buttonConfig->setEventHandler(handleEvent);
    buttonConfig->setClickDelay(uint16_t clickDelay)
        // End AceButton setup

        _changeAll(LOW);

    // WebServer setup
    while (status != WL_CONNECTED)
    {
        status = WiFi.begin(ssid, pass);
        delay(5000);
    }
    server.begin();

    app.get("/", &getIndex);
    app.get("/api/info", &getInfo);
    app.post("/api/button/all/:state", &changeAll);
    app.post("/api/button/:number/:state", &changeOutput);
    // End WebServer setup

    // Matrix
    // matrix.begin();
    // End Matrix
}

void loop()
{
    if (WARMUP)
    {
        _changeAll(HIGH);
        WARMUP = false;
    }

    // Should be called every 4-5ms or faster, for the default debouncing time
    // of ~20ms.
    for (uint8_t i = 0; i < HARD_NUM_BUTTONS; i++)
    {
        buttons[i].check();
    }

    WiFiClient client = server.available();

    if (client.connected())
    {
        app.process(&client);
        client.stop();
    }
}

// The event handler for the button.
void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
    uint8_t id = button->getId();

    if (id == 2)
    {
        // edge case: buttons 0 and 2 should change the same output.
        id = 0;
    }

    uint8_t pin = INFOS[id].outputPin;

    switch (eventType)
    {
    case AceButton::kEventPressed:
        if (INFOS[id].isToggle)
        {
            INFOS[id].outputState = !INFOS[id].outputState;
        }
        else
        {
            INFOS[id].outputState = HIGH;
        }
        registerWriteSend(pin, INFOS[id].outputState);
        break;
    case AceButton::kEventReleased:
        if (!INFOS[id].isToggle)
        {
            INFOS[id].outputState = LOW;
            registerWriteSend(pin, INFOS[id].outputState);
        }
        break;
    default:
        break;
    }
}

// Sends bits to the shift registers
void registerSend()
{
    // turn off the output so the pins don't light up
    // while you're shifting bits:
    digitalWrite(LATCH_PIN, LOW);

    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, BITSTOSEND2);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, BITSTOSEND);

    // turn on the output so the LEDs can light up:
    digitalWrite(LATCH_PIN, HIGH);
}

void registerWrite(int pin, int state)
{
    if (pin <= 7)
    {
        bitWrite(BITSTOSEND, pin, state);
    }
    else
    {
        bitWrite(BITSTOSEND2, pin - 8, state);
    }
}

void registerWriteSend(int pin, int state)
{
    registerWrite(pin, state);
    registerSend();
}

void changeOutput(Request &req, Response &res)
{
    char number[5];
    char state[5];

    req.route("number", number, 5);
    req.route("state", state, 5);

    int id = atoi(number);
    int pin = INFOS[id].outputPin;
    int desiredState = 1 - atoi(state);

    registerWriteSend(pin, desiredState);
    INFOS[id].outputState = desiredState;

    res.println(number);
    res.println("ok");
}

void changeAll(Request &req, Response &res)
{
    char state[5];
    req.route("state", state, 5);
    int desiredState = 1 - atoi(state);

    _changeAll(desiredState);

    res.println("ok all");
}

void _changeAll(int state)
{
    if (state)
    {
        BITSTOSEND = 0xFF;
        BITSTOSEND2 = 0xFF;
    }
    else
    {
        BITSTOSEND = 0x00;
        BITSTOSEND2 = 0x00;
    }
    for (int i = 0; i < TOTAL_NUM_BUTTONS; i++)
    {
        int pin = INFOS[i].outputPin;
        INFOS[i].outputState = state;
    }

    registerSend();
}

void getInfo(Request &req, Response &res)
{
    res.set("Content-Type", "application/json");

    res.println("{\"buttons\": [");

    for (int i = 0; i < TOTAL_NUM_BUTTONS; i++)
    {
        Info info = INFOS[i];
        res.print("{");
        res.print("\"id\": ");
        res.print(i);
        res.print(", ");
        res.print("\"state\": ");
        res.print(!info.outputState);
        res.print(", ");
        res.print("\"label\": \"");
        res.print(info.label);
        res.print("\"");
        res.print("}");

        if (i + 1 < TOTAL_NUM_BUTTONS)
        {
            res.print(",");
        }
    }

    res.println("]}");
}

void getIndex(Request &req, Response &res)
{
    res.set("Content-Type", "text/html");
    res.printP(index);
}

void headers(Request &req, Response &res)
{
    char *origin = req.get("Origin");
    res.set("Access-Control-Allow-Origin", origin);
    res.set("Access-Control-Allow-Methods", "GET, HEAD, POST");
    res.set("Access-Control-Allow-Headers", "*");
}
