//Do An cuoi ki
#include <WiFi.h>
#include <string>
#include "PubSubClient.h"
#include "DHTesp.h"
#include "RTClib.h"

RTC_DS1307 rtc;
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqttServer = "broker.mqtt-dashboard.com";
int port = 1883;
const float GAMMA = 0.7;
const float RL10 = 50;
const int pin_led = 13;
const int pin_move = 2;
const int pin_light = 34;
const int pin_loa = 12;
const int pin_bom_nuoc = 14;
const int pin_quat = 27;

int temp_ = 0;
int humi_ = 0;
bool move_ = 0;
int light_ = 0;
bool user_control = 0;

DHTesp dhtSensor;
WiFiClient espClient;
PubSubClient client(espClient);

void wifiConnect() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!!!");
}

void mqttReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection....");
    if (client.connect("n13/id")) {
      Serial.println("connected");
      client.subscribe("n13/led");
      client.subscribe("n13/loa");
      client.subscribe("n13/bom_nuoc");
      client.subscribe("n13/quat");
      client.subscribe("n13/user_control");
    } else {
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void on_off (char* type) {
  if (strcmp(type, "on_led") == 0) {
    digitalWrite(pin_led, 1);
  } 
  if (strcmp(type, "off_led") == 0) {
    digitalWrite(pin_led, 0);
  } 
  if (strcmp(type, "on_loa") == 0) {
    //digitalWrite(pin_loa, 1);
    tone(pin_loa, 500);
  } 
  if (strcmp(type, "off_loa") == 0) {
    //digitalWrite(pin_loa, 0);
    noTone(pin_loa);
  } 
  if (strcmp(type, "on_bom_nuoc") == 0) {
    digitalWrite(pin_bom_nuoc, 1);
  } 
  if (strcmp(type, "off_bom_nuoc") == 0) {
    digitalWrite(pin_bom_nuoc, 0);
  } 
  if (strcmp(type, "on_quat") == 0) {
    digitalWrite(pin_quat, 1);
  } 
  if (strcmp(type, "off_quat") == 0) {
    digitalWrite(pin_quat, 0);
  } 
}


void callback (char* topic, byte* message, unsigned int length) {
  Serial.println(topic);
  String stMessage;
  for (int i =0; i < length; i++) {
    stMessage += (char)message[i];
  }
  char * mess = (char*)stMessage.c_str();
  if (strcmp(mess, "true") == 0 && strcmp(topic, "n13/user_control") == 0) {
    if (!user_control) {
      off_all();
    }
    user_control = 1;
  } else if (strcmp(mess, "true") != 0 && strcmp(topic, "n13/user_control") == 0) {
    user_control = 0;
  } else {
    on_off(mess);
  }
  
  Serial.println((stMessage));
}

void setup() {
  Serial.begin(115200);

#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  Serial.println("Connecting to WiFi");
  pinMode(pin_led, OUTPUT);
  pinMode(pin_loa, OUTPUT);
  pinMode(pin_bom_nuoc, OUTPUT);
  pinMode(pin_quat, OUTPUT);
  pinMode(pin_move, INPUT);
  ledcAttachPin(pin_loa, 1);
  ledcSetup(1, 500, 500);
  pinMode(pin_light, INPUT);
  wifiConnect();

  client.setServer(mqttServer, port);
  client.setCallback(callback);

  dhtSensor.setup(15, DHTesp::DHT22);
  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void temp () {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  int t = data.temperature;
  temp_ = t;
  char buffer[50];
  sprintf(buffer, "%d", t);
  client.publish("n13/temp", buffer);
}

void humi () {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  int h = data.humidity;
  humi_ = h;
  char buffer[50];
  sprintf(buffer, "%d", h);
  client.publish("n13/humi", buffer);
}

void move () {
  int data = digitalRead(pin_move);
  move_ = data;
  char buffer[50];
  sprintf(buffer, "%d", data);
  client.publish("n13/move", buffer);
}

void light () {
  int analogValue = analogRead(pin_light);
  float voltage = (float(analogValue) / 4) / 1024. * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  int lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));
  light_ = lux;
  char buffer[50];
  sprintf(buffer, "%d", lux);
  client.publish("n13/light", buffer);
}

void process (int temp, int humi, int light) {
  // Humi < Humi -> Tuoi cay
  // Move  == 1 -> Loa
  // Anh sang < light -> led
  // Temp < temp -> quat
  if (humi_ < humi) {
    on_off("on_bom_nuoc");
  } else {
    on_off("off_bom_nuoc");
  }
  if (move_ == 1) {
    on_off("on_loa");
  } else {
    on_off("off_loa");
  }
  if (light_ < light) {
    on_off("on_led");
  } else {
    on_off("off_led");
  }
  if (temp_ > temp) {
    on_off("on_quat");
  } else {
    on_off("off_quat");
  }
}

void off_all() {
  on_off("off_bom_nuoc");
  on_off("off_loa");
  on_off("off_led");
  on_off("off_quat");
}

int time() {
  DateTime now = rtc.now();
  return now.hour();
}

void loop() {
  
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();

  temp();
  humi();
  move();
  light();
  if (!user_control) {
    if (time() >= 18) // Tối
      process(20, 80, 5000);
    if (time() <= 5)  // Sáng sớm
      process(18, 80, 5000);
    else              // Sáng - Chiều
      process(15, 80, 5000);
  }

  delay(5000);
}
