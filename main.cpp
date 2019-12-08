#include <Arduino.h>
#include <Arduino_MKRENV.h>
#include <SPI.h>
#include <SD.h>
#include "RoundRobinbyJR.h"
#include <WiFi101.h>
#include <RTCZero.h>
#include <FlashAsEEPROM.h>
#include <FlashStorage.h>
#include <Adafruit_SleepyDog.h>
#include <MQTT.h>
#include <MQTTClient.h>
#include "arduino_secrets.h"
 
#define RELAY1 1
#define RETAIN true
 
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int keyIndex = 0;
int status = WL_IDLE_STATUS;
char mqttHost[] = SECRET_MQTT_HOST;
char mqttClientId[] = SECRET_MQTT_CLIENT_ID;
char mqttUser[] = SECRET_MQTT_USER;
char mqttPass[] = SECRET_MQTT_PASS;
 
unsigned long lastMillis = 0;
unsigned long lastMillis2 = 0;
 
WiFiSSLClient net;
MQTTClient mqttClient;
RTCZero rtc;
 
const int GMT = 1;
 
const int SD_CHIP_SELECT = 4;
 
const int relay1 = 1;
 
String s1 = "";
String argument = "";
 
String LastFilelog = "";
String LastFilelogEpoch = "";
String LastFilelogTemp = "";
String LastFilelogR1 = "";
String LastFilelogR1Set = "";
String LastFilelogtempsetpointH = "";
String LastFilelogtempsetpointL = "";
 
 
unsigned int seconds = TIME1;
 
File myFile;
char fileName[20] = "reg24h.txt";
const int logsToRemove = 60 / (TIME4/60);
 
const int trigger = logsToRemove * 24;
unsigned long Time = TIME2 * 1000;
unsigned long prevTime = 0;
 
float temperature;
 
String relay1Setteable = "false";
String relay2Setteable = "false";
unsigned long epoch;
String relaystatus;
String tempsetpointH = "30.00";
String tempsetpointL = "29.50";
String automatic = "true";
String tempsetpointH_D;
String tempsetpointL_D;
 
void setRTCwithNTP()
{
 
  int numberOfTries = 0, maxTries = 6;
  Serial.println("getting time...");
  do
  {
    Serial.print("getting time...");
    epoch = WiFi.getTime();
    delay(1000);
    Serial.println("done");
    numberOfTries++;
  } while ((epoch == 0) || (numberOfTries < maxTries));
 
  if (numberOfTries > maxTries)
  {
    Serial.print("NTP unreachable!!");
    WiFi.disconnect();
  }
  else
  {
    Serial.print("Epoch received: ");
    Serial.println(epoch);
    rtc.setEpoch(epoch);
 
    Serial.println();
  }
  rtc.setHours(rtc.getHours() + GMT);
}
 
void print2digits(int number)
{
  if (number < 10)
  {
    Serial.print("0");
  }
  Serial.print(number);
}
 
void printTime()
{
 
  print2digits(rtc.getHours());
  Serial.print(":");
  print2digits(rtc.getMinutes());
  Serial.print(":");
  print2digits(rtc.getSeconds());
  Serial.println();
}
 
void printDate()
{
  print2digits(rtc.getDay());
  Serial.print("/");
  print2digits(rtc.getMonth());
  Serial.print("/");
  print2digits(rtc.getYear());
 
  Serial.println("");
}
 
void printWiFiStatus()
{
 
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
 
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
 
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
 
void printReading(float temperature)
{
 
  printDate();
 
  printTime();
 
  Serial.print("epoch = ");
  Serial.print(epoch);
  Serial.println(" ");
 
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" °C");
 
  Serial.print("relay1 = ");
  Serial.print(relaystatus);
  Serial.println(" ");
 
  Serial.print("relay1/$setteable = ");
  Serial.print(String(relay1Setteable));
  Serial.println(" ");
 
  Serial.print("tempsetpointL = ");
  Serial.print(String(tempsetpointL));
  Serial.println(" ");
 
  Serial.print("tempsetpointH = ");
  Serial.print(String(tempsetpointH));
  Serial.println(" ");
 
  Serial.println();
}
 
void connectMqttServer()
{
 
  Watchdog.disable();
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
 
  mqttClient.begin(mqttHost, SSL_PORT, net);
  Serial.print("\nconnecting to MQTT server...");
 
  unsigned long mqttTime = millis();
  unsigned long mqttnow = millis();
 
  while (!mqttClient.connect(mqttClientId, mqttUser, mqttPass) && (mqttnow < mqttTime + 5000))
  {
    Serial.print(".");
    mqttnow = millis();
    delay(1000);
  }
 
  if (mqttClient.connected())
  {
    Serial.println("\nconnected!");
    mqttClient.subscribe("/homie/mkr1000/mkrenv/$tempsetpointH");
    mqttClient.subscribe("/homie/mkr1000/mkrenv/$tempsetpointL");
    mqttClient.subscribe("/homie/mkr1000/mkrenv/$automatic");
    mqttClient.subscribe("/hello");
    mqttClient.subscribe("/homie/mkr1000/relayshd/relay1/set");
    mqttClient.subscribe("/homie/mkr1000/relayshd/relay1/$setteable");
    mqttClient.subscribe("/homie/mkr1000/relayshd/relay1");
  }
  else
  {
    Serial.println("\nnot connected!");
  }
  Watchdog.enable(180000);
}
 
void messageReceived(String &topic, String &payload)
{
 
  Serial.println("incoming: " + topic + " - " + payload);
 
  if ((topic == "/homie/mkr1000/mkrenv/$automatic") && (payload == "true"))
  {
    automatic = "true";
    Serial.println("automatic: " + automatic);
  }
 
  if ((topic == "/homie/mkr1000/mkrenv/$automatic") && (payload == "false"))
  {
    automatic = "false";
    Serial.println("automatic: " + automatic);
  }
 
  if ((topic == "/homie/mkr1000/relayshd/relay1"))
  {
    mqttClient.unsubscribe("/homie/mkr1000/relayshd/relay1");
    if (payload == "true")
    {
      digitalWrite(RELAY1, HIGH);
      relaystatus = "true";
      if (digitalRead(RELAY1))
        mqttClient.publish("/homie/mkr1000/relayshd/relay1", "true");
    }
    if (payload == "false")
    {
      digitalWrite(RELAY1, LOW);
      relaystatus = "false";
      if (!digitalRead(RELAY1))
        mqttClient.publish("/homie/mkr1000/relayshd/relay1", "false");
    }
  }
 
  if ((topic == "/homie/mkr1000/relayshd/relay1/set") && (payload == "true") && (relay1Setteable != "false") && (automatic != "true"))
  {
    digitalWrite(RELAY1, HIGH);
    relaystatus = "true";
    if (digitalRead(RELAY1))
      mqttClient.publish("/homie/mkr1000/relayshd/relay1", "true");
  }
 
  else if ((topic == "/homie/mkr1000/relayshd/relay1/set") && (payload == "false") && (relay1Setteable != "false") && (automatic != "true"))
  {
    digitalWrite(RELAY1, LOW);
    relaystatus = "false";
    if (!digitalRead(RELAY1))
      mqttClient.publish("/homie/mkr1000/relayshd/relay1", "false");
  }
 
  if ((topic == "/homie/mkr1000/relayshd/relay1/$setteable") && (payload == "true"))
  {
    relay1Setteable = "true";
  }
 
  else if ((topic == "/homie/mkr1000/relayshd/relay1/$setteable") && (payload == "false"))
  {
    relay1Setteable = "false";
  }
 
  if ((topic == "/homie/mkr1000/mkrenv/$tempsetpointH"))
  {
    tempsetpointH = payload;
    Serial.print("\ntempsetpointH = ");
    Serial.println(tempsetpointH);
  }
 
  if ((topic == "/homie/mkr1000/mkrenv/$tempsetpointL"))
  {
    tempsetpointL = payload;
    Serial.print("\ntempsetpointL = ");
    Serial.println(tempsetpointL);
  }
 
  Serial.println(" ");
}
 
void getENVValues()
{
 
  if (digitalRead(RELAY1))
    relaystatus = "true";
  else
    relaystatus = "false";
 
  temperature = ENV.readTemperature();
 
  epoch = rtc.getEpoch() - GMT * 3600;
}
void WriteToFile()
{
  Watchdog.reset();
  if (myFile = SD.open(fileName, FILE_WRITE))
  {
    Serial.print("Writing to ");
    Serial.print(fileName);
    Serial.println(" .....");
 
    myFile.print(epoch);
    myFile.print("   ");
 
    myFile.print(temperature);
    myFile.print(" °C ");
 
    myFile.print(tempsetpointH);
    myFile.print(" tempsetpointH ");
    myFile.print(tempsetpointL);
    myFile.print(" tempsetpointL ");
 
    myFile.print(relaystatus);
    myFile.print(" relay1 ");
 
    myFile.print(relay1Setteable);
    myFile.print(" relay1/$setteable ");
 
    myFile.println();
    myFile.close();
    Serial.println("done.");
  }
  else
  {
    Serial.println("error opening test.txt");
  }
}
 
void fileInfo()
{
  Serial.print(fileName);
  Serial.print(" contains ");
  int num = NumberOfLogs(fileName);
  Serial.print(num);
  Serial.println(" logs.\nShowing contents...");
  PrintFile(fileName);
  Serial.println();
}
 
void alarmMatch()
{
 
  Serial.println("Alarm match!");
 
  if ((mqttClient.connected()))
  {
    Watchdog.reset();
 
    getENVValues();
 
    printReading(temperature);
 
    lastMillis = millis();
    mqttClient.publish("/homie/mkr1000/mkrenv/epoch", String(epoch), RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/mkrenv/temperature", String(temperature), RETAIN, 0);
 
    mqttClient.publish("/homie/mkr1000/relayshd/relay1", relaystatus, RETAIN, 0);
 
    mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointH", tempsetpointH, RETAIN, 0);
 
    mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointL", tempsetpointL, RETAIN, 0);
    tempsetpointL_D = tempsetpointL;
    tempsetpointH_D = tempsetpointH;
  }
 
  if (!(mqttClient.connected()))
  {
    Watchdog.reset();
 
    getENVValues();
 
    printReading(temperature);
 
    WriteToFile();
  }
 
  rtc.setAlarmSeconds((rtc.getAlarmSeconds() + TIME4) % 60);
}
 
void setup()
{
  pinMode(RELAY1, OUTPUT);
 
  int watch_dog = Watchdog.enable(170000);
  Serial.begin(9600);
 
  delay(5000);
  Serial.print("Enabled the watchdog with max countdown of ");
  Serial.print(watch_dog, DEC);
  Serial.println(" milliseconds!");
 
  if (!ENV.begin())
  {
    Serial.println("Failed to initialize MKR ENV shield!");
    while (1);
  }
  Watchdog.reset();
 
  Serial.println("Initializing SD card...");
  if (!SD.begin(4))
  {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");
 
  Watchdog.reset();
 
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield not present");
    while (true);
  }
  Watchdog.reset();
 
  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
    Watchdog.reset();
  }
  printWiFiStatus();
 
  rtc.begin();
  Watchdog.reset();
  setRTCwithNTP();
  printTime();
  printDate();
 
  Watchdog.reset();
 
  mqttClient.begin(mqttHost, net);
  mqttClient.onMessage(messageReceived);
  connectMqttServer();
 
  if ((mqttClient.connected()))
  {
 
    //DEVICE
    mqttClient.publish("/homie/mkr1000/$homie", "4.0.0", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/$name", "mkr1000", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/$state", "", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/$nodes", "mkrenv,relayshd", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/$extensions", "", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/$implementation", "mkr1000", RETAIN, 0);
 
    //NODE 1
    mqttClient.publish("/homie/mkr1000/mkrenv/$name", "mkrenv", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/mkrenv/$type", "", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/mkrenv/$properties", "temperature,epoch,,temperature_D,epoch_D", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/mkrenv/temperature/$datatype", "float", RETAIN, 0);   //temperature value
    mqttClient.publish("/homie/mkr1000/mkrenv/epoch/$datatype", "String", RETAIN, 0);        //time value
    mqttClient.publish("/homie/mkr1000/mkrenv/temperature_D/$datatype", "float", RETAIN, 0); //temperature value deferred
    mqttClient.publish("/homie/mkr1000/mkrenv/epoch_D/$datatype", "String", RETAIN, 0);      //time value deferred
    mqttClient.publish("/homie/mkr1000/mkrenv/$attributes", "$tempsetpointH,$tempsetpointL,$automatic,$tempsetpointH_D,$tempsetpointL_D", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointH/$attributes", "float", RETAIN, 0);   //temperature setpoint high
    mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointL/$attributes", "float", RETAIN, 0);   //temperature setpoint low
    mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointH_D/$attributes", "float", RETAIN, 0); //temperature setpoint high deferred
    mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointL_D/$attributes", "float", RETAIN, 0); //temperature setpoint low deferred
    mqttClient.publish("/homie/mkr1000/mkrenv/$automatic/$attributes", "boolean", RETAIN, 0);     //automatic control setting
 
    //NODE 2
    mqttClient.publish("/homie/mkr1000/relayshd/$name", "relayshd", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/relayshd/$type", "", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/relayshd/$properties", "relay1,relay1_D,relay1/set", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/relayshd/relay1/set/$datatype", "boolean", RETAIN, 0); //status relay1 setting
    mqttClient.publish("/homie/mkr1000/relayshd/relay1/$datatype", "boolean", RETAIN, 0);     //status relay1
    mqttClient.publish("/homie/mkr1000/relayshd/relay1_D/$datatype", "boolean", RETAIN, 0);   //status relay1 deferred
    mqttClient.publish("/homie/mkr1000/relayshd/$attributes", "relay1/$setteable", RETAIN, 0);
    mqttClient.publish("/homie/mkr1000/relayshd/relay1/$setteable/$attributes", "boolean", RETAIN, 0); // relay1 setteable setting
  }
 
  Watchdog.reset();
 
  if (!(mqttClient.connected()))
  {
 
    Watchdog.disable();
    Serial.print(fileName);
    Serial.print(" contains in last log ");
    argument = NumberOfLogs(fileName);
    Serial.print(argument.toInt());
    Serial.println("\nShowing content...");
    LastFilelog = ReadLine(fileName, argument.toInt());
 
    LastFilelogEpoch = LastFilelog.substring(0, 10);
    LastFilelogTemp = LastFilelog.substring(13, 18);
    LastFilelogR1 = LastFilelog.substring(62, 68);
    LastFilelogR1Set = LastFilelog.substring(75, 81);
    LastFilelogtempsetpointH = LastFilelog.substring(22, 27);
    LastFilelogtempsetpointL = LastFilelog.substring(42, 47);
 
    Serial.println(LastFilelogEpoch + LastFilelogTemp + LastFilelogR1 + LastFilelogR1Set + LastFilelogtempsetpointH + LastFilelogtempsetpointL);
 
    Serial.println("\nInit relay1 with last SD log...");
    if (LastFilelogR1.indexOf("true") >= 0)
    {
      digitalWrite(RELAY1, HIGH);
    }
    if (LastFilelogR1.indexOf("false") >= 0)
    {
      digitalWrite(RELAY1, LOW);
    }
 
    Watchdog.enable(180000);
  }
 
  rtc.setAlarmTime(00, 00, 00);
  rtc.enableAlarm(rtc.MATCH_SS);
 
  rtc.attachInterrupt(alarmMatch);
 
  Watchdog.reset();
}
 
void loop()
{
 
  mqttClient.loop();
 
  if (!(mqttClient.connected()) && (millis() - lastMillis > seconds * 1000))
  {
 
    if (!mqttClient.connected())
      connectMqttServer();
 
    lastMillis = millis();
  }
 
  Watchdog.reset();
 
  if (millis() - lastMillis2 > TIME3)
  {
    if (automatic == "true")
    {
      temperature = ENV.readTemperature();
      Serial.println("\ntemperature: " + String(temperature));
      Serial.println("tempsetpointL: " + tempsetpointL);
      Serial.println("tempsetpointH: " + tempsetpointH);
      Serial.println("automatic: " + automatic);
      Serial.println();
 
      String relaystatus_D = relaystatus;
      if (temperature < tempsetpointL.toFloat())
      {
        digitalWrite(RELAY1, HIGH);
        relaystatus = "true";
      }
      if (temperature > tempsetpointH.toFloat())
      {
        digitalWrite(RELAY1, LOW);
        relaystatus = "false";
      }
 
      if ((relaystatus_D != relaystatus) && (mqttClient.connected()))
      {
 
        if (digitalRead(RELAY1))
        {
          mqttClient.publish("/homie/mkr1000/relayshd/relay1", "true");
        }
        if (!digitalRead(RELAY1))
        {
          mqttClient.publish("/homie/mkr1000/relayshd/relay1", "false");
        }
      }
    }
    lastMillis2 = millis();
  }
 
  Watchdog.reset();
 
  if ((prevTime + Time) < millis())
  {
 
    if (NumberOfLogs(fileName) >= trigger)
    {
      Serial.print("Deleting oldest logs.....");
      Watchdog.disable();
      RemoveOldLogs(fileName, trigger, logsToRemove);
      Serial.println("done.");
      Watchdog.enable(180000);
      fileInfo();
    }
 
    if ((NumberOfLogs(fileName) > 0) && (mqttClient.connected()))
    {
 
      Watchdog.disable();
      Serial.print(fileName);
      Serial.print(" contains in last log ");
      argument = NumberOfLogs(fileName);
      Serial.print(argument.toInt());
      Serial.println("\nShowing content...");
      LastFilelog = ReadLine(fileName, argument.toInt());
 
      LastFilelogEpoch = LastFilelog.substring(0, 10);
      LastFilelogTemp = LastFilelog.substring(13, 18);
      LastFilelogR1 = LastFilelog.substring(62, 68);
      LastFilelogR1Set = LastFilelog.substring(75, 81);
      LastFilelogtempsetpointH = LastFilelog.substring(22, 27);
      LastFilelogtempsetpointL = LastFilelog.substring(42, 47);
 
      Serial.println(LastFilelogEpoch + LastFilelogTemp + LastFilelogR1 + LastFilelogR1Set + LastFilelogtempsetpointH + LastFilelogtempsetpointL);
      Serial.println("\nSending last log to broker...");
      if (!mqttClient.connected())
        connectMqttServer();
      if (mqttClient.connected())
      {
        mqttClient.publish("/homie/mkr1000/mkrenv/epoch_D", LastFilelogEpoch, RETAIN, 0);
        mqttClient.publish("/homie/mkr1000/mkrenv/temperature_D", LastFilelogTemp, RETAIN, 0);
        mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointH_D", LastFilelogtempsetpointH, RETAIN, 0);
        mqttClient.publish("/homie/mkr1000/mkrenv/$tempsetpointL_D", LastFilelogtempsetpointL, RETAIN, 0);
 
        if (LastFilelogR1.indexOf("true") >= 0)
        {
          mqttClient.publish("/homie/mkr1000/relayshd/relay1_D", "true", RETAIN, 0);
        }
        if (LastFilelogR1.indexOf("false") >= 0)
        {
          mqttClient.publish("/homie/mkr1000/relayshd/relay1_D", "false", RETAIN, 0);
        }
        EraseLastLog(fileName);
        fileInfo();
      }
      if (!mqttClient.connected())
        connectMqttServer();
      Watchdog.enable(180000);
    }
 
    prevTime = millis();
  }
  Watchdog.reset();
 
  s1 = Serial.readString();
  if (s1 != "")
  {
    Serial.print("Received serial command => ");
    Serial.println(s1);
 
    if (s1.indexOf("check") >= 0)
    {
      Watchdog.reset();
      fileInfo();
    }
 
    else if (s1.indexOf("read") >= 0)
    {
      Watchdog.reset();
      Serial.print(fileName);
      Serial.print(" contains in log ");
      argument = s1.substring(s1.indexOf("d") + 1);
      Serial.print(argument.toInt());
      Serial.println("\nShowing content...");
      Serial.println(ReadLine(fileName, argument.toInt()));
    }
 
    else if (s1.indexOf("save") >= 0)
    {
      Watchdog.disable();
      Serial.print(fileName);
      Serial.print(" remove logs, last logs saved: ");
      argument = s1.substring(s1.indexOf("e") + 1);
      Serial.print(argument.toInt());
      Serial.println("\nShowing trigger log:");
      Serial.println(ReadLine(fileName, 1 + NumberOfLogs(fileName) - argument.toInt()));
      Serial.println("\nDeleting logs.....");
      SaveLogs(fileName, argument.toInt());
      Serial.println("done.");
      fileInfo();
      Watchdog.enable(180000);
    }
 
    else if ((s1.indexOf("true") >= 0))
    {
      digitalWrite(RELAY1, HIGH);
      if (mqttClient.connected())
        mqttClient.publish("/homie/mkr1000/relayshd/relay1", "true", RETAIN, 0);
      Serial.println("relay1 = true");
    }
    else if ((s1.indexOf("false") >= 0))
    {
      digitalWrite(RELAY1, LOW);
      if (mqttClient.connected())
        mqttClient.publish("/homie/mkr1000/relayshd/relay1", "false", RETAIN, 0);
      Serial.println("relay1 = false");
    }
    else if (s1.indexOf("relay1/$setteable1") >= 0)
    {
      if (mqttClient.connected())
        mqttClient.publish("/homie/mkr1000/relayshd/relay1/$setteable", "true", RETAIN, 0);
      relay1Setteable = "true";
    }
    else if (s1.indexOf("relay1/$setteable0") >= 0)
    {
      if (mqttClient.connected())
        mqttClient.publish("/homie/mkr1000/relayshd/relay1/$setteable", "false", RETAIN, 0);
      relay1Setteable = "false";
    }
 
    else
    {
      Serial.println("Invalid Data");
    }
  }
 
  Watchdog.reset();
}
