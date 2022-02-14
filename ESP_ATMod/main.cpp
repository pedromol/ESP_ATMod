#include "Arduino.h"
#include <ESPAsyncWebServer.h>
#include "fauxmoESP.h"

#include <AsyncTimer.h>

#include "credentials.h"
#include "ESP_ATMod.h"

bool pinState = false;
unsigned short pinDebounce = 0;
AsyncTimer t;
fauxmoESP fauxmo;
AsyncWebServer server(80);

void sendStatus(AsyncWebServerRequest *request)
{
    time_t now = time(nullptr);
    struct tm *info = localtime((const time_t *)&now);

    char response[80];
    char parsedTime[32];

    strftime(parsedTime, sizeof(parsedTime), "%Y-%m-%dT%T.000%z", info);

    sprintf(response, "{\"time\":\"%s\",\"pinState\":\"%s\"}", parsedTime, pinState ? "On" : "Off");

    request->send(200, "application/json", response);
}

void handleState(bool state)
{
    pinState = state;
    digitalWrite(2, pinState == true ? HIGH : LOW);
    fauxmo.setState(DEVICE_NAME, pinState, pinState == true ? 255 : 0);
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
                    handleState(!pinState);
					pinDebounce = 0; },
                                       666);
        }
    }
}

void serverSetup()
{
    Serial.println("Starting server");

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStatus(request); });

    server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                handleState(true);
                sendStatus(request); });

    server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                handleState(false);
                sendStatus(request); });

    server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
              {   
                handleState(!pinState);
                sendStatus(request); });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                         {
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return; });
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return; });

    server.begin();
}

void setupFauxmo()
{
    Serial.println("Starting Fauxmo");

    fauxmo.createServer(false);
    fauxmo.setPort(80);
    fauxmo.enable(true);
    fauxmo.addDevice(DEVICE_NAME);

    fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                      { handleState(!state); });
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

    serverSetup();
    setupFauxmo();
}

void loop()
{
    t.handle();
    fauxmo.handle();

    handleButtom();

    ATMod_loop();
}