#define SECRET_SSID "SSSID_NAME"
#define SECRET_PASS "SSIDPWD"
#define SECRET_MQTT_CLIENT_ID "Client_ID"
#define SECRET_MQTT_HOST "HOST_IP"
#define SECRET_MQTT_USER "user"
#define SECRET_MQTT_PASS "pwd"
#define SSL_PORT 8883
#define RELAY1 1
#define RETAIN true
 
//TIME1 every try to reconect to broker
#define TIME1 10 
//TIME2 Check for number of logs if they should be erased//every n seconds is a log, so every n * 1000 miliseconds
// time is also for sending registerd logs if they exist to the broker
#define TIME2 30 
//TIME3 Automatic temperature control
#define TIME3 5000 
//TIME4 Every tme we send data to broker or if not connected register the log to SD
#define TIME4 60 
