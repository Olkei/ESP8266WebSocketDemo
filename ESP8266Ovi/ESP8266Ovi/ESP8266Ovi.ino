#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// led variables and password
int MEASUREMENT = 1000; // sekunnin välein tarkistus (valoisuus)
int KIINNI = 1000; // sekunnin välein tarkistus (ovi)
int AUKI = 10000; // 10 sek välein tarkistus (ovi)
int red = 0;
int yellow = 0;
int green = 0;
int redPin = 14;     //D5
int greenPin = 12;   //D6
int yellowPin = 13;  //D7
bool userAnswer = 0; //tarkistaa tuleeko käyttäjältä oikea salasana

const char *correctPW = "oikea"; //oven salasana
const char *ssid = "ESP"; //Wifi mihin ESP yhdistää
const char *ssidpassword = "passu1234"; // kyseisen wifin salasana
const char *hostName = "esp-async"; // ESPin muodostama wifi
const char *http_username = "admin"; // käyttäjätunnus ESPiin
const char *http_password = "admin"; // salasana ESPiin

unsigned long eventtime = 0;
unsigned long measurementEventtime = 0;

int measured_value = 0;
int temp_value = 0;

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void measure();

void setup()
{
  Serial.begin(57600);
  Serial.setDebugOutput(true);
  WiFi.hostname(hostName);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(hostName, ssidpassword);
  WiFi.begin(ssid, ssidpassword);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(1000);
    WiFi.begin(ssid, ssidpassword);
  }

  //pinMode(A0, INPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);

  digitalWrite(redPin, 0);
  digitalWrite(greenPin, 0);
  digitalWrite(yellowPin, 0);

  MDNS.begin("ESP");
  MDNS.addService("http", "tcp", 80);

  SPIFFS.begin();
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.addHandler(new SPIFFSEditor(http_username, http_password));

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("NOT_FOUND: ");

    if (request->contentLength())
    {
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for (i = 0; i < headers; i++)
    {
      AsyncWebHeader *h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for (i = 0; i < params; i++)
    {
      AsyncWebParameter *p = request->getParam(i);
    }

    request->send(404);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char *)data);
    if (index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
  server.begin();
}

void loop()
{

  if (userAnswer == 0) // pitää punaisen ledin 1sek ajan päällä
  {
    if (millis() - eventtime >= KIINNI)
    {
      digitalWrite(redPin, 0);
      digitalWrite(yellowPin, 0);
      digitalWrite(greenPin, 0);
    }
  }

  if (userAnswer == 1) // pitää "oven auki" 10 sek ajan
  {
    if (millis() - eventtime >= AUKI)
    {
      digitalWrite(redPin, 0);
      digitalWrite(yellowPin, 0);
      digitalWrite(greenPin, 0);
    }
  }

  if (millis() - measurementEventtime >= MEASUREMENT) //lähettää dataa verkkosivulle valoisuuteen liitten
  {
    measure();
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["m"] = measured_value;
    String data;
    if (temp_value != measured_value)
    {
    root.printTo(data);
    ws.textAll(data);
    }
    temp_value = measured_value;
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }

  else if (type == WS_EVT_PONG)
  {
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < info->len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      Serial.printf("%s\n", msg.c_str());
      // Serial.println(msg.c_str());
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject &root = jsonBuffer.parseObject(msg);
      const char *pword = root["p"]; //otetaan JSONista tullut salasanasyötteen

      ws.text(client->id(), "received");
      if (root["p"] == correctPW) // tarkistaa JSONista tulleen salasanasyötteen mätsäämisen oikeaan salasanaan
      {
        userAnswer = 1; //oven avaus
        Serial.printf("oikein! \n");
        analogWrite(yellowPin, 255);
        analogWrite(greenPin, 255);
      }
      else
      {
        userAnswer = 0; // ovi pysyy kiinni
        Serial.printf("väärin! \n");
        analogWrite(yellowPin, 0);
        analogWrite(greenPin, 0);
        analogWrite(redPin, 255);
      }
      eventtime = millis();
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < info->len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      Serial.printf("%s\n", msg.c_str());

      if ((info->index + len) == info->len)
      {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void measure() // mittaa valoisuutta
{
  int light;
  light = (int)analogRead(A0);
  measurementEventtime = millis();
  measured_value = light;
}
