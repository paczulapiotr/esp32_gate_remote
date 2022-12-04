#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "driver/gpio.h"
#include "credentials.hpp"

#define MQTT_OPEN_MSG "ESP32_GATE/OPEN"
#define MQTT_CLOSE_MSG "ESP32_GATE/CLOSE"
#define GATE_PIN_OPEN 22
#define GATE_PIN_CLOSE 23
#define PIN_LOW_TIME 1500

WiFiClient espClient;
PubSubClient client(espClient);

void wifi_setup();
void wifi_waitfForConnection();
void gpio_setup();
void mqtt_consume();
void mqtt_setup();
void mqtt_callback(char *topic, byte *message, unsigned int length);
void gate_open();
void gate_close();
void retry(std::function<void()> func, std::function<bool()> check, std::function<void()> onFailed);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(921600);
  Serial.println("Hello start");
  gpio_setup();
  wifi_setup();
  mqtt_setup();
}

void loop()
{
  wifi_waitfForConnection();
  mqtt_consume();
}
void mqtt_setup()
{
  client.setCallback(mqtt_callback);
  client.setServer(MQTT_SERVER, 1883);
}

void mqtt_callback(char *topic, byte *message, unsigned int length)
{
  Serial.printf("Message arrived on topic: %s\n", topic);
  String topicStr = String(topic);

  digitalWrite(LED_BUILTIN, HIGH);
  if (topicStr == MQTT_OPEN_MSG)
  {
    gate_open();
  }
  else if (topicStr == MQTT_CLOSE_MSG)
  {
    gate_close();
  }
  digitalWrite(LED_BUILTIN, LOW);
}

void mqtt_consume()
{
  retry([]()
        {
                Serial.println("Attempting MQTT connection...");
                if (client.connect("ESP32_Client"))
                {
                  Serial.println("MQTT connected");
                  client.subscribe(MQTT_OPEN_MSG);
                  client.subscribe(MQTT_CLOSE_MSG);
                } },
        []()
        { return client.connected(); },
        []()
        {
          Serial.printf("MQTT connection failed, rc=%d\n", client.state());
        });

  client.loop();
}

// WiFi setup
void wifi_setup()
{
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  // We start by connecting to a WiFi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifi_waitfForConnection();
  Serial.printf("Connected. IP: %s\n", WiFi.localIP().toString());
}

void wifi_waitfForConnection()
{
  retry([]() {},
        []()
        { return WiFi.status() == WL_CONNECTED; },
        []()
        {
          Serial.printf("Failed to connect to: %s\n", WIFI_SSID);
        });
}

void gpio_setup()
{
  gpio_config_t configPinOpen{
      GPIO_SEL_22,
      GPIO_MODE_OUTPUT,
      GPIO_PULLUP_DISABLE,
      GPIO_PULLDOWN_DISABLE,
      GPIO_INTR_DISABLE};

  gpio_config_t configPinClose{
      GPIO_SEL_23,
      GPIO_MODE_OUTPUT,
      GPIO_PULLUP_DISABLE,
      GPIO_PULLDOWN_DISABLE,
      GPIO_INTR_DISABLE};
  gpio_config(&configPinClose);
  gpio_config(&configPinOpen);
}

void gate_open()
{
  Serial.println("STARTING: Opening gate...");
  gpio_set_level(GPIO_NUM_22, LOW);
  delay(PIN_LOW_TIME);
  gpio_set_level(GPIO_NUM_22, HIGH);
  Serial.println("FINISHED: Opening gate...");
}

void gate_close()
{
  Serial.println("STARTING: Closing gate...");
  gpio_set_level(GPIO_NUM_23, LOW);
  delay(PIN_LOW_TIME);
  gpio_set_level(GPIO_NUM_23, HIGH);
  Serial.println("FINISHED: Closing gate...");
}

void retry(std::function<void()> func, std::function<bool()> check, std::function<void()> onFailed)
{
  int trial = 0;

  while (!check())
  {
    func();

    if (!check())
    {
      trial = trial + 1;

      if (trial > 0)
      {
        onFailed();
      }

      if (trial >= 60)
      {
        Serial.println("Restarting ESP32...");
        ESP.restart();
      }

      delay(500);
    }
  }
}