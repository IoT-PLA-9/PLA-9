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

WiFiClient net;
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
char fileName[20] = "boiler.csv";
char fileSave[20] = "NotSend.csv";
const int logsToRemove = 60 / (TIME4 / 60); //?

const int trigger = logsToRemove * 24;
unsigned long Time = TIME2 * 1000;
unsigned long prevTime = 0;

String msgField[7];

struct Data { 
     char *epch; 
     char *temp;
     char *r1set; 
     char *r1stt;
     char *tsl;
     char *tsh ; 
     char *aut;
} ; 

struct Data dTofile;

float temperature;

String relay1Setteable = "false";
// String relay2Setteable = "false";
unsigned long epoch;
String relaystatus = "false";
float tempsetpointH = 27.00;
float tempsetpointL = 26.00;
String automatic = "true";

// String tempsetpointH_D;
// String tempsetpointL_D;
  // Mensaje de LastWill
  const char willTopic[] = "homie/boiler_xxx/will";
  String willPayload = "Broken !!!";
  bool willRetain = true;
  int willQos = 1;

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
  Serial.println(epoch);

  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("relay1 = ");
  Serial.println(relay1);

  Serial.print("relay1/$setteable = ");
  Serial.println(relay1Setteable);

  Serial.print("tempsetpointL = ");
  Serial.println(tempsetpointL);

  Serial.print("tempsetpointH = ");
  Serial.println(tempsetpointH);

  Serial.print("automatic = ");
  Serial.println(automatic);
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

  // Y este whie ?
  while (!mqttClient.connect(mqttClientId, mqttUser, mqttPass) && (mqttnow < mqttTime + 5000))
  {
    Serial.print(".");
    mqttnow = millis();
    delay(1000);
  }

  if (mqttClient.connected())
  {
    Serial.println("\nconnected!");

    mqttClient.publish(willTopic, willPayload, willRetain, willQos); // Last Will

    mqttClient.subscribe("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointH");
    mqttClient.subscribe("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointL");
    mqttClient.subscribe("homie/boiler_xxx/mkr1000/mkrenv/automatic");
    //    mqttClient.subscribe("/hello");
    mqttClient.subscribe("homie/boiler_xxx/mkr1000/relayshd/relay1/set");
    mqttClient.subscribe("homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable");
    mqttClient.subscribe("homie/boiler_xxx/mkr1000/relayshd/relay1");
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

  if ((topic == "homie/boiler_xxx/mkr1000/mkrenv/automatic")) //&& (payload == "true"))
  {
    automatic = payload;
    Serial.println("automatic: " + automatic);
 //   mqttClient.publish("homie/boiler_xxx/mkr1000/mkenv/automatic",automatic,RETAIN,0);

  }
  /*
  if ((topic == "homie/boiler_xxx/mkr1000/mkrenv/automatic") && (payload == "false"))
  {
    automatic = "false";
    Serial.println("automatic: " + automatic);
  }
 */
  if ((topic == "homie/boiler_xxx/mkr1000/relayshd/relay1"))
  {
    mqttClient.unsubscribe("homie/boiler_xxx/mkr1000/relayshd/relay1");
    digitalWrite(RELAY1, payload == "true");
    relaystatus = payload; 
 //   mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1",relaystatus,RETAIN,0);  
//    if (payload == "true")
//    {
      Serial.print("==> Relay1 \t");
      Serial.print(payload+"\t");
      Serial.println(digitalRead(RELAY1));
      
//      digitalWrite(RELAY1, payload == "true");
//      relaystatus = payload;
//      if (digitalRead(RELAY1))
//        mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", "true");
//      else 
//        mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", "false");  
    
    /*
    if (payload == "false")
    {
      digitalWrite(RELAY1, LOW);
      relaystatus = "false";
      if (!digitalRead(RELAY1))
        mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", "false");
    }
   */
  }

  if ((topic == "homie/boiler_xxx/mkr1000/relayshd/relay1/set") && (payload == "true") && (relay1Setteable != "false") && (automatic != "true"))
  {
    Serial.print("==> Relay1 set \t");
    Serial.print(payload+"\t");
    Serial.println(digitalRead(RELAY1));

    digitalWrite(RELAY1, HIGH);
    relaystatus = "true";
//    if (digitalRead(RELAY1))
//      mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", "true" );
  }

  //else 
  if ((topic == "homie/boiler_xxx/mkr1000/relayshd/relay1/set") && (payload == "false") && (relay1Setteable != "false") && (automatic != "true"))
  {
    digitalWrite(RELAY1, LOW);
    relaystatus = "false";
//    if (!digitalRead(RELAY1))
//      mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", "false" );
  }

  if ((topic == "homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable")) //&& (payload == "true"))
  {
   // relay1Setteable = "true";
      Serial.print("==> Relay1 setteable\t");
      Serial.print(payload+"\t");
      Serial.println(relay1Setteable);
           
     if (relay1Setteable != payload)
      {
       relay1Setteable = payload;
//       mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable", relay1Setteable );
      }

  }
/*
  else if ((topic == "homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable") && (payload == "false"))
  {
    relay1Setteable = "false";
  }
*/
  if ((topic == "homie/boiler_xxx/mkr1000/mkrenv/tempsetpointH"))
  {
    tempsetpointH = payload.toFloat();
    Serial.print("\ntempsetpointH = ");
    Serial.println(tempsetpointH);
    //------------------------
    if (temperature >= tempsetpointH)
    {
      relaystatus = "false";
      digitalWrite(RELAY1,LOW);
      mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/set", "false",RETAIN,0);     
    }
    //------------------------
  }

  if ((topic == "homie/boiler_xxx/mkr1000/mkrenv/tempsetpointL"))
  {
    tempsetpointL = payload.toFloat();
    Serial.print("\ntempsetpointL = ");
    Serial.println(tempsetpointL);
    //------------------------
    if (temperature <= tempsetpointL)
    {
      relaystatus = "true";
      digitalWrite(RELAY1,HIGH);
      mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/set", "true",RETAIN,0);     
    }
    //------------------------
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
void WriteToFile(char *nameofFile)
{
  File myxFile;
  Watchdog.reset();
  if (myxFile = SD.open(nameofFile, FILE_WRITE))
  {
    Serial.print("Writing to ");
    Serial.print(nameofFile);
    Serial.println(" .....");

    myxFile.print(epoch);   //msgField[0]
    myxFile.print(",");

    myxFile.print(temperature);  //msgField[1]
    myxFile.print(",");

    myxFile.print(tempsetpointH);  //msgField[2]
    myxFile.print(",");
    //myFile.print(" tempsetpointH ");
    myxFile.print(tempsetpointL);   //msgField[3]
    //myFile.print(" tempsetpointL ");
    myxFile.print(",");

    myxFile.print(relaystatus);    //msgField[4]
    myxFile.print(",");
    //myFile.print(" relay1 ");

    myxFile.print(relay1Setteable);  //msgField[5]
    //myFile.print(" relay1/$setteable ");
    myxFile.print(",");

    myxFile.print(automatic);   //msgField[6]
     //+++++++ Añadido
    myxFile.println();
    myxFile.close();
  
    Serial.println("done.");
  }
  else
  {
    Serial.print("error opening ");
    Serial.println(fileName);
  }
}

void fileInfo(char *nameofFile)
{
  Serial.print(nameofFile);
  Serial.print(" contains ");
  int num = NumberOfLogs(nameofFile);
  Serial.print(num);
  Serial.println(" logs.\n 1=>Showing contents...");
  PrintFile(nameofFile);
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
    //    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/epoch", String(epoch), RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/temperature", (String(temperature) + " / " + String(epoch)), RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", (relaystatus + " / " + String(epoch)), RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointH", (String(tempsetpointH) + " / " + String(epoch)), RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointL", (String(tempsetpointL) + " / " + String(epoch)), RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/relay1shd/relay1/$setteable", (relay1Setteable + " / " + String(epoch)), RETAIN, 0); // ++++ Añadido
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkenviron/automatic", (automatic + " / " + String(epoch)), RETAIN, 0);
    //   tempsetpointL_D = tempsetpointL;
    //    tempsetpointH_D = tempsetpointH;
    Serial.print("Saving to ");
    Serial.println(fileName);
    //delay(1000);
    WriteToFile(fileName);
    SaveDate("SenDate.txt", String(epoch));
  }

  if (!(mqttClient.connected()))
  {
    Watchdog.reset();

    getENVValues();

    printReading(temperature);
    Serial.print("Saving to ");
    Serial.println(fileSave);
    // delay(1000);
    WriteToFile(fileSave);
    SaveDate("NoSDate.txt", String(epoch));
    cleanFile(fileSave);
  }

  rtc.setAlarmSeconds((rtc.getAlarmSeconds() + TIME4) % 60);
}

void getCSVfields(String fullMsg)
{
  byte sentencePos =0;
  int commaCount=0;
  msgField[commaCount]="";
  while (sentencePos < fullMsg.length())
  {
    if(fullMsg.charAt(sentencePos) == ',')
    {
      commaCount++;
      msgField[commaCount]="";
      sentencePos++;
    }
    else
    {
      msgField[commaCount] += fullMsg.charAt(sentencePos);
      sentencePos++;
    }
  }
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
    mqttClient.publish("homie/boiler_xxx/mkr1000/$homie", "4.0.0", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/$name", "mkr1000", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/$state", "", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/$nodes", "mkrenv,relayshd", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/$extensions", "", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/$implementation", "mkr1000", RETAIN, 0);

    //NODE 1
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/$name", "mkrenv", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/$type", "", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/$properties", "temperature,tempstpointL,tempsetpointH,automatic", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/temperature/$datatype", "float", RETAIN, 0);     //temperature value
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetL/$datatype", "float", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetH/$datatype", "float", RETAIN, 0);
   // mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/epoch/$datatype", "String", RETAIN, 0);          //time value
                                                                                                         //    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/temperature_D/$datatype", "float", RETAIN, 0); //temperature value deferred
                                                                                                         //    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/epoch_D/$datatype", "String", RETAIN, 0);      //time value deferred
                                                                                                         //    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/$attributes", "$tempsetpointH,$tempsetpointL,automatic,$tempsetpointH_D,$tempsetpointL_D", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointH/$attributes", "float", RETAIN, 0); //temperature setpoint high
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointL/$attributes", "float", RETAIN, 0); //temperature setpoint low
                                                                                                         //    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/$tempsetpointH_D/$attributes", "float", RETAIN, 0); //temperature setpoint high deferred
                                                                                                         //    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/$tempsetpointL_D/$attributes", "float", RETAIN, 0); //temperature setpoint low deferred
    mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/automatic/$attributes", "boolean", RETAIN, 0);   //automatic control setting

    //NODE 2
    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/$name", "relayshd", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/$type", "", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/$properties", "relay1,relay1/set", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/set/$datatype", "boolean", RETAIN, 0); //status relay1 setting
                                                                                                        //    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$datatype", "boolean", RETAIN, 0);     //status relay1
                                                                                                        //    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1_D/$datatype", "boolean", RETAIN, 0);   //status relay1 deferred
 //   mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable", "boolean", RETAIN, 0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable", "true", RETAIN,0);
    mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$attributes", "boolean", RETAIN, 0); // relay1 setteable setting
  }

  Watchdog.reset();

  if (!(mqttClient.connected()))
  {

    Watchdog.disable();
    Serial.print(fileSave);
    Serial.print(" contains in last log ");
    argument = NumberOfLogs(fileName);
    Serial.print(argument.toInt());
    Serial.println("\n 2=> Showing content...");
    Serial.print(fileSave);
    Serial.print("\t"); 
    Serial.println(argument);
    
    LastFilelog = ReadLine(fileSave, argument.toInt());
    Serial.println(LastFilelog);
/*
    LastFilelogEpoch = LastFilelog.substring(0, 10);
    LastFilelogTemp = LastFilelog.substring(13, 18);
    LastFilelogR1 = LastFilelog.substring(62, 68);
    LastFilelogR1Set = LastFilelog.substring(75, 81);
    LastFilelogtempsetpointH = LastFilelog.substring(22, 27);
    LastFilelogtempsetpointL = LastFilelog.substring(42, 47);
  
*/    
    //Serial.println(LastFilelogEpoch + LastFilelogTemp + LastFilelogR1 + LastFilelogR1Set + LastFilelogtempsetpointH + LastFilelogtempsetpointL);

    Serial.println("\nInit relay1 with last SD log...");
    if (LastFilelogR1.indexOf("true") >= 0)
   // {
      digitalWrite(RELAY1, HIGH);
   // }
   // if (LastFilelogR1.indexOf("false") >= 0)
  //  {
    else
      digitalWrite(RELAY1, LOW);
  //  }

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
      Serial.println("tempsetpointH: " + String(tempsetpointH));
      Serial.println("tempsetpointL: " + String(tempsetpointL));
      Serial.println("automatic: " + automatic);
      Serial.print("relay1: ");
      Serial.println(digitalRead(RELAY1));
      Serial.println("relay1setteable: " + relay1Setteable);
      Serial.println();

      String relaystatus_D = relaystatus;
      if (temperature <= tempsetpointL)
      {
        digitalWrite(RELAY1, HIGH);
        relaystatus = "true";
      }
      if (temperature >= tempsetpointH)
      {
        digitalWrite(RELAY1, LOW);
        relaystatus = "false";
      }

      if ((relaystatus_D != relaystatus) && (mqttClient.connected()))
      {

        if (digitalRead(RELAY1))
  //      {
          mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", String(true) + " / " + String(epoch),0,0);
  //      }
  //      if (!digitalRead(RELAY1))
  //      {
          else
          mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", String(false) + " / " + String(epoch),0,0);
  //      }
      }
    }
    lastMillis2 = millis();
  }

  Watchdog.reset();

  if ((prevTime + Time) < millis())
  {

    if (NumberOfLogs(fileSave) >= trigger)
    {
      Serial.print("Deleting oldest logs.....");
      Watchdog.disable();
      RemoveOldLogs(fileSave, trigger, logsToRemove);
      Serial.println("done.");
      Watchdog.enable(180000);
      fileInfo(fileSave);
    }

    if ((NumberOfLogs(fileSave) > 0) && (mqttClient.connected()))
    {

      Watchdog.disable();
      Serial.print(fileSave);
      Serial.print(" contains in last log ");
      argument = NumberOfLogs(fileSave);
      Serial.println(fileSave);
      Serial.print(argument.toInt());
      Serial.println("\n 3=>Showing content...");
      
      LastFilelog = ReadLine(fileSave, argument.toInt());

      Serial.println(LastFilelog);
      getCSVfields(LastFilelog);
/*
      LastFilelogEpoch = LastFilelog.substring(0, 10); 
      LastFilelogTemp = LastFilelog.substring(13, 18);
      LastFilelogR1 = LastFilelog.substring(62, 68);
      LastFilelogR1Set = LastFilelog.substring(75, 81);
      LastFilelogtempsetpointH = LastFilelog.substring(22, 27);
      LastFilelogtempsetpointL = LastFilelog.substring(42, 47);
*/
     

      //Serial.println(LastFilelogEpoch + LastFilelogTemp + LastFilelogR1 + LastFilelogR1Set + LastFilelogtempsetpointH + LastFilelogtempsetpointL);
      Serial.println("\nSending last log to broker...");
      
      if (!mqttClient.connected())
        connectMqttServer();
      if (mqttClient.connected())
      {
//------------------------------------------------------------------------------
       for (int i=1; i <= argument.toInt(); i++ )
       {
         
         LastFilelog = ReadLine(fileSave, argument.toInt());
         Serial.println(LastFilelog);
         getCSVfields(LastFilelog);
        //        mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/epoch_D", LastFilelogEpoch, RETAIN, 0);
         mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/temperature", (msgField[1] + " / " + msgField[0]), RETAIN, 0);
         mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointH", (msgField[2] + " / " + msgField[0]), RETAIN, 0);
         mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/tempsetpointL", (msgField[3] + " / " + msgField[0]), RETAIN, 0);

        // if (LastFilelogR1.indexOf("true") >= 0)
         mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", (msgField[4] + " / " + msgField[0]), RETAIN, 0);
       // else
       //   mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", "false", RETAIN, 0);
         mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable",(msgField[5]+  " / " + msgField[0]), RETAIN, 0);   //+++
         mqttClient.publish("homie/boiler_xxx/mkr1000/mkrenv/automatic",(msgField[6] + " / " + msgField[0]), RETAIN, 0);   //+++
        //EraseLastLog(fileSave);
        //fileInfo(fileSave);
        SD.remove(fileSave);     
       } 
//----------------------------------------------------------------------------------
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
      fileInfo(fileSave);
    }

    else if (s1.indexOf("read") >= 0)
    {
      Watchdog.reset();
      Serial.print(fileName);
      Serial.print(" contains in log ");
      argument = s1.substring(s1.indexOf("d") + 1);
      Serial.print(argument.toInt());
      Serial.println("\n 4=>Showing content...");
      Serial.println(ReadLine(fileSave, argument.toInt()));
    }

    else if (s1.indexOf("save") >= 0)
    {
      Watchdog.disable();
      Serial.print(fileSave);
      Serial.print(" remove logs, last logs saved: ");
      argument = s1.substring(s1.indexOf("e") + 1);
      Serial.print(argument.toInt());
      Serial.println("\nShowing trigger log:");
      Serial.println(ReadLine(fileSave, 1 + NumberOfLogs(fileSave) - argument.toInt()));
      Serial.println("\nDeleting logs.....");
      SaveLogs(fileSave, argument.toInt());
      Serial.println("done.");
      fileInfo(fileSave);
      Watchdog.enable(180000);
    }

    else if ((s1.indexOf("true") >= 0))
    {
      digitalWrite(RELAY1, HIGH);
      if (mqttClient.connected())
        mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", String(true) + " / " + String(epoch), RETAIN, 0);
      Serial.println("relay1 = true");
    }
    else if ((s1.indexOf("false") >= 0))
    {
      digitalWrite(RELAY1, LOW);
      if (mqttClient.connected())
        mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1", String(false) + " / " + String(epoch) , RETAIN, 0);

    }
    else if (s1.indexOf("relay1/$setteable1") >= 0)
    {
      if (mqttClient.connected())
        mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable",(String(true) + " / " + String(epoch)), RETAIN, 0);
      relay1Setteable = "true";
    }
    else if (s1.indexOf("relay1/$setteable0") >= 0)
    {
      if (mqttClient.connected())
        mqttClient.publish("homie/boiler_xxx/mkr1000/relayshd/relay1/$setteable", String(false) +" / " + String(epoch), RETAIN, 0);
      relay1Setteable = "false";
    }

    else
    {
      Serial.println("Invalid Data");
    }
  }
    // WriteToFile(fileName);
    Watchdog.reset();
}
void SaveDate(char * nameofFile, String epch)
{
  File myxFile;
  if  (!SD.exists(nameofFile))
  {
    myxFile = SD.open(nameofFile, FILE_WRITE);
    myxFile.print(epch);
    myxFile.close();
  }
}
void cleanFile(char *nameofFile)
{
  File myxFileO;
  File myxFileD;
 // SdFat sd;
  
  int ctdor = 1;
  String line;
  float epch;

  epch = WiFi.getTime();
  if (SD.exists(nameofFile))
  {
    myxFileD = SD.open("buffer.csv", FILE_WRITE);
    myxFileO = SD.open(nameofFile, FILE_READ) ;
    myxFileD.close();

    String msgField[7];
    while (myxFileO.available())
    {
      line = myxFileO.readStringUntil('\n');
      getCSVfields(line);
//      Serial.print("Linea: ");
//      Serial.println(ctdor); 
      Serial.print("Msg: ");
      Serial.println(msgField[0]);
      Serial.print("Epoch -24: ");
      Serial.println(epch -(24*60*3600));
      if (msgField[0].toFloat() >= epch -(24*60*3600)) // 24h en segundos
      {
        while (myxFileO.available())
        {
          Serial.print("Linea: ");
          Serial.println(ctdor);
          line = myxFileO.readStringUntil('\n');
          WriteLine("buffer.csv", line);
        } 
        myxFileO.close();
        CopyFile("buffer.csv",nameofFile);
        Serial.print("buffer.csv ->");
        Serial.print(nameofFile);

        SD.remove("SenDate.txt");
        SaveDate("SenDate.txt", String(epch));
      }
      ctdor++;
        myxFileO.close();
    SD.remove(nameofFile);
    }
 }
}
