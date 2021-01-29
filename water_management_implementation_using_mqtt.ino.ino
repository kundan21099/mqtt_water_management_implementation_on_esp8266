#include <BearSSLHelpers.h>
#include <CertStoreBearSSL.h>
#include <ESP8266WiFi.h>

#include <SPI.h>
#include <Wire.h>
#include <PubSubClient.h>
// #include <WiFi.h>

// Replace the next variables with your SSID/Password combination
const char *ssid = "realme 5";
const char *pass = "12345678";

// Add your MQTT Broker IP address, example:
const char *mqtt_server = "test.mosquitto.org";
#define LED_BUILTIN 16

#define SENSOR 2
#define trig 5
#define echo 4

// #define UDP_TX_PACKET_MAX_SIZE 24;
//  #define UDP_TX_PACKET_MAX_SIZE 8192;

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
boolean ledState = LOW;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned long flowMilliLitres;
unsigned int totalMilliLitres;
float flowLitres;
float totalLitres;
long duration, inches;
int cm;
float level;
int set = 100;
int setpoint = 10;

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

WiFiClient client;

// WiFiClient espClient;
PubSubClient mqttclient(client);
long lastMsg = 0;
char msg[50];
int value = 0;

long microsecondsToCentimeters(long microseconds);
long microsecondsToInches(long microseconds);
void reconnect();
void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSOR, INPUT_PULLUP);
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;

  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED)
  {
    status = WiFi.begin(ssid, pass); // Connect to WPA/WPA2 Wi-Fi network.
    delay(5000);
  }

  Serial.println("Connected to wifi");

  // setup_wifi();
  mqttclient.setServer(mqtt_server, 1883);
  mqttclient.setCallback(callback);
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
}

// separate function to connect with wifi
// void setup_wifi() {
//  delay(10);
//  // We start by connecting to a WiFi network
//  Serial.println();
//  Serial.print("Connecting to ");
//  Serial.println(ssid);
//
//  WiFi.begin(ssid, password);
//
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    Serial.print(".");
//  }
//
//  Serial.println("");
//  Serial.println("WiFi connected");
//  Serial.println("IP address: ");
//  Serial.println(WiFi.localIP());
//}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageflow;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageflow += (char)message[i];
  }
  Serial.println();
}

void reconnect()
{
  // Loop until we're reconnected
  while (!mqttclient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttclient.connect("ESP8266Client99"))
    {
      Serial.println("connected");
      // Subscribe
      mqttclient.subscribe("esp/water");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop()
{
  if (!mqttclient.connected())
  {
    reconnect();
  }
  mqttclient.loop();

  long now = millis();
  if (now - lastMsg > 5000)
  {
    lastMsg = now;

    currentMillis = millis();
    if (currentMillis - previousMillis > interval)
    {

      pulse1Sec = pulseCount;
      pulseCount = 0;

      flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
      previousMillis = millis();
      flowMilliLitres = (flowRate / 60) * 1000;
      flowLitres = (flowRate / 60);

      // Add the millilitres passed in this second to the cumulative total
      totalMilliLitres += flowMilliLitres;
      totalLitres += flowLitres;
      // Print the flow rate for this second in litres / minute
      Serial.print("Flow rate: ");
      Serial.print(float(flowRate)); // Print the integer part of the variable
      Serial.print("L/min");
      Serial.print("\t"); // Print tab space

      // Print the cumulative total of litres flowed since starting
      Serial.print("Output Liquid Quantity: ");
      Serial.print(totalMilliLitres);
      Serial.print("mL / ");
      Serial.print(totalLitres);
      Serial.println("L");
    }

    pinMode(trig, OUTPUT);
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
    pinMode(echo, INPUT);
    duration = pulseIn(echo, HIGH);
    inches = microsecondsToInches(duration);
    cm = microsecondsToCentimeters(duration);
    level = set - cm;
    Serial.print(inches);
    Serial.print("in, ");
    Serial.print(level);
    Serial.print("cm");
    Serial.println();
    delay(100);
    if (cm <= setpoint)
    {
      Serial.println("Tank is full");
      digitalWrite(LED_BUILTIN, LOW);
    }
    else
    {
      Serial.println("Tank is not full");
      digitalWrite(LED_BUILTIN, HIGH);
    }
    // Convert the value to a char array
    char flowString[8];
    dtostrf(flowRate, 1, 2, flowString);
    mqttclient.publish("esp/flow", flowString);

    // Convert the value to a char array
    char volString[8];
    dtostrf(totalLitres, 1, 2, volString);
    mqttclient.publish("esp/volume", volString);

    char levelString[8];
    dtostrf(level, 1, 2, levelString);
    mqttclient.publish("esp/level", levelString);
  }
}

long microsecondsToInches(long microseconds)
{
  return microseconds / 74 / 2;
}

long microsecondsToCentimeters(long microseconds)
{
  return microseconds / 29 / 2;
}
