#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <AsyncTimer.h>

#include "credentials.h"
#include "ESP_ATMod.h"

bool pinState = false;
unsigned short pinDebounce = 0;
AsyncTimer t;

ESP8266WebServer server(80);

void handleIndex()
{
    time_t now = time(nullptr);
    struct tm *info = localtime((const time_t *)&now);

    char response[80];
    char parsedTime[32];

    strftime(parsedTime, sizeof(parsedTime), "%Y-%m-%dT%T.000%z", info);

    sprintf(response, "{\"time\":\"%s\",\"pinState\":\"%s\"}", parsedTime, pinState ? "On" : "Off");

    server.send(200, "application/json", response);
}

void handleToggle()
{
    pinState = !pinState;
    digitalWrite(2, pinState == true ? LOW : HIGH);
    handleIndex();
}

void handleOn()
{
    pinState = true;
    digitalWrite(2, HIGH);
    handleIndex();
}

void handleOff()
{
    pinState = false;
    digitalWrite(2, LOW);
    handleIndex();
}

void handleButtom()
{
    if (digitalRead(0) == LOW)
    {
        if (pinDebounce != 0)
        {
            t.reset(pinDebounce);
        }
        else
        {
            pinDebounce = t.setTimeout([]()
                                       {
					pinState = !pinState;
					digitalWrite(2, pinState == true ? LOW : HIGH);
					pinDebounce = 0; },
                                       666);
        }
    }
}

void setup()
{
    pinMode(1, INPUT);
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    ATMod_setup();

    WiFi.disconnect();
    Serial.println("");
    Serial.println("Starting default connection.");

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (!WiFi.isConnected())
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.print("");

    Serial.println("Starting server");

    server.on("/", handleIndex); // Handle Index page
    server.on("/on", handleOn);
    server.on("/off", handleOff);
    server.on("/toggle", handleToggle);

    server.begin(); // Start the server
}

void loop()
{
    t.handle();
    server.handleClient(); // Handling of incoming client requests

    handleButtom();

    ATMod_loop();
}