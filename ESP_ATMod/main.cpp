#include "Arduino.h"
#include <ESPAsyncWebServer.h>
#include "fauxmoESP.h"

#include <AsyncTimer.h>

#include "credentials.h"
#include "ESP_ATMod.h"

#include <PubSubClient.h>

#define STATE_TOPIC ("switch/" DEVICE_NAME "/get")
#define COMMAND_TOPIC ("switch/" DEVICE_NAME "/set")

bool pinState = false;
unsigned short pinDebounce = 0;
AsyncTimer t;
fauxmoESP fauxmo;
AsyncWebServer server(80);

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

void sendStatus(AsyncWebServerRequest *request)
{
    time_t now = time(nullptr);
    struct tm *info = localtime((const time_t *)&now);

    char response[80];
    char parsedTime[32];

    strftime(parsedTime, sizeof(parsedTime), "%Y-%m-%dT%T.000%z", info);

    sprintf(response, "{\"time\":\"%s\",\"pinState\":\"%s\"}", parsedTime, pinState ? "On" : "Off");

    request->send(200, F("application/json"), response);
}

void handleState(bool state)
{
    pinState = state;
    Serial.print(F("New State : "));
    Serial.println(pinState);
    digitalWrite(2, pinState == true ? HIGH : LOW);
    pubSubClient.publish(STATE_TOPIC, pinState ? "1" : "0", true);
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

void handleSub(char *topic, byte *payload, unsigned int length)
{
    handleState((char)payload[0] == '1');
}

void serverSetup()
{
    Serial.println(F("Starting Server"));

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
        String body = (request->hasParam(F("body"), true)) ? request->getParam(F("body"), true)->value() : String();
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) {
            return;
        } else {
            request->send(404, F("text/plain"), F("Not found")); } });

    server.begin();
}

void fauxmoSetup()
{
    Serial.println(F("Starting Fauxmo"));

    fauxmo.createServer(false);
    fauxmo.setPort(80);
    fauxmo.enable(true);
    fauxmo.addDevice(DEVICE_NAME);

    fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                      { 
                          pinState=state;
                          t.setTimeout([]()
                                     { handleState(pinState); },
                                     666); });
}

void pubSubHandle()
{
    while (!pubSubClient.connected())
    {
        if (!pubSubClient.connect(DEVICE_NAME))
        {
            Serial.print(F("."));
            delay(1000);
        }
        else
        {
            pubSubClient.subscribe(COMMAND_TOPIC);
        }
    }
    pubSubClient.loop();
}

void pubSubSetup()
{
    Serial.println(F("Starting PubSub"));

    pubSubClient.setServer(MQTT_SERVER, 1883);
    pubSubClient.setCallback(handleSub);
    pubSubHandle();
}

void setup()
{
    pinMode(1, INPUT);
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    ATMod_setup();

    WiFi.disconnect();
    Serial.println(F(""));
    Serial.println(F("Starting default connection."));

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (!WiFi.isConnected())
    {
        Serial.print(F("."));
        delay(1000);
    }
    Serial.println(F(""));

    serverSetup();
    fauxmoSetup();
    pubSubSetup();
    handleState(false);
}

void loop()
{
    handleButtom();

    ATMod_loop();

    fauxmo.handle();
    pubSubHandle();

    t.handle();
}