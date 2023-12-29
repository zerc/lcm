#include <aWOT.h>
#include <SPI.h>
#include <Ethernet.h>
#include "index.cpp"

#include <AceButton.h>
using namespace ace_button;

// Ethernet setup
bool ethernetActive = true;
char requestOrigin[50];
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer server(80);
Application app;
void changeOutput(Request &req, Response &res);
void changeAll(Request &req, Response &res);
void getInfo(Request &req, Response &res);
void getIndex(Request &req, Response &res);
// void cors(Request &req, Response &res);
void headers(Request &req, Response &res);
// End Ethernet setup

// AceButton configuration
const uint8_t TOTAL_NUM_BUTTONS = 16;
const uint8_t HARD_NUM_BUTTONS = 13; // number of buttons available via hardware switches

struct Info
{
    const uint8_t inputPin;
    const uint8_t outputPin;
    bool outputState;
    bool isToggle; // can be turned off by repeated activation
};

Info INFOS[TOTAL_NUM_BUTTONS] = {
    {A0, 1, HIGH, true}, // 1
    {A1, 8, HIGH, true}, // 2
    {A2, 2, HIGH, true}, // 3
    {A3, 15, HIGH},      // 4
    {A4, 3, HIGH},       // 5
    {A5, 14, HIGH},      // 6
    {3, 4, HIGH},        // 7
    {4, 7, HIGH},        // 8
    {5, 5, HIGH},        // 9
    {6, 6, HIGH},        // 10
    {7, 12, HIGH},       // 11
    {8, 13, HIGH},       // 12
    {9, 11, HIGH},       // 13
    // Reserved by the Ethernet shield
    // https://www.arduino.cc/reference/en/libraries/ethernet/
    {10, 10, HIGH}, // 14
    {11, 0, HIGH},  // 15
    {12, 9, HIGH},  // 16
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

    registerSend();

    ButtonConfig *buttonConfig = ButtonConfig::getSystemButtonConfig();
    buttonConfig->setEventHandler(handleEvent);
    buttonConfig->setFeature(ButtonConfig::kFeatureClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
    buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);
    // End AceButton setup

    // WebServer setup
    while (Ethernet.begin(mac, 30000UL, 10000UL) == 0)
    {
        if (Ethernet.linkStatus() != LinkON)
        {
            ethernetActive = false;
            break;
        }
    }

    if (ethernetActive)
    {
        app.header("Origin", requestOrigin, 50);
        app.use(&headers);
        app.get("/", &getIndex);
        app.options("/", &cors);
        app.get("/api/info", &getInfo);
        app.post("/api/button/all/:state", &changeAll);
        app.post("/api/button/:number/:state", &changeOutput);
        server.begin();
    }
    // End WebServer setup
}

void loop()
{
    // Should be called every 4-5ms or faster, for the default debouncing time
    // of ~20ms.
    for (uint8_t i = 0; i < HARD_NUM_BUTTONS; i++)
    {
        buttons[i].check();
    }

    if (ethernetActive)
    {
        Ethernet.maintain();

        EthernetClient client = server.available();

        if (client.connected())
        {
            app.process(&client);
            client.stop();
        }
    }
}

// The event handler for the button.
void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
    uint8_t id = button->getId();
    uint8_t pin = INFOS[id].outputPin;

    switch (eventType)
    {
    case AceButton::kEventPressed:
        INFOS[id].outputState = !INFOS[id].outputState;
        registerWriteSend(pin, INFOS[id].outputState);
        break;
    case AceButton::kEventReleased:
        if (!INFOS[id].isToggle)
        {
            registerWriteSend(pin, LOW);
            INFOS[id].outputState = LOW;
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

    res.println("ok");
}

void changeAll(Request &req, Response &res)
{
    char state[5];
    req.route("state", state, 5);

    for (int i = 0; i < TOTAL_NUM_BUTTONS; i++)
    {
        int pin = INFOS[i].outputPin;
        int desiredState = 1 - atoi(state);
        registerWrite(pin, desiredState);
        INFOS[i].outputState = desiredState;
    }

    registerSend();

    res.println("ok");
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
        res.print("\"label\": \"button");
        res.print(i);
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

void cors(Request &req, Response &res)
{
    res.sendStatus(204);
}