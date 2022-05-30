#define UNIT_NUMBER 1

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#define SerialAT Serial1
#define SerialMon Serial

/* Todo:
 * GPS Precision
 * GPS Speed
 * General reliablity
 * General Speed
 * Disconnect detector/reset loop
 *    
 * Wishlist:
 * Multiple connection + UDP Server
 */


// variables for recieving serial
const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
boolean newData = false;

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

/*
   Tests enabled
*/
#define TINY_GSM_TEST_GPRS    true
#define TINY_GSM_TEST_GPS     false
#define TINY_GSM_POWERDOWN    true

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[]  = "hologram";     //SET TO YOUR APN
const char gprsUser[] = "";
const char gprsPass[] = "";
String DEVICE_KEYS[] = {"be75%RzU","R{#fvKLL","gJrxNYe5"};

//const char device_key[] = String(DEVICE_KEYS[UNIT_NUMBER]);
String device_key = String(DEVICE_KEYS[UNIT_NUMBER]);
//String payload_tag = "email_test";
String payload_tag = "percentage_test";
const char server[]   = "cloudsocket.hologram.io";
const char resource[] = "9999";
const char newline[] = "\r\n";

#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <Ticker.h>
//#include <SoftwareSerial.h>

#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif


#ifdef USE_SSL
TinyGsmClientSecure client(modem);
const int           port = 9999;
#else
TinyGsmClient  client(modem);
const int      port = 9999;
#endif

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60          // Time ESP32 will go to sleep (in seconds)

#define UART_BAUD   9600
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13
#define LED_PIN     12
#define SENSOR_PIN  32

int SENSOR_PINS[] = {32, 33, 34, 35, 21, 22};
int SENSOR_LEVELS[] = {10, 25, 50, 75, 100, 200};

int counter, lastIndex, numberOfPieces = 24;
String pieces[24], input;
String res = "";


void setup() {
  // Set console baud rate
  Serial.begin(115200);
  delay(10);

  for(int i = 0; i < 6; i++){ 
    pinMode(SENSOR_PINS[i], INPUT);
  }

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  // Attempt to set up SD Card
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("No SDCard Detected");
  } else {
    uint32_t cardSize = SD.cardSize() / (1024 * 1024);
    String str = "SDCard Size: " + String(cardSize) + "MB";
    Serial.println(str);
  }

  Serial.println("\nWait...");

  delay(10000);

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem, attempting to continue without restarting");
  }

  String name = modem.getModemName();
  delay(500);
  Serial.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  delay(500);
  Serial.println("Modem Info: " + modemInfo);

  // Set SIM7000G GPIO4 LOW ,turn off GPS power
  // CMD:AT+SGPIO=0,4,1,0
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,0");

  #if TINY_GSM_TEST_GPRS
    // Unlock your SIM card with a PIN if needed
    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
      modem.simUnlock(GSM_PIN);
    }
  #endif
  /*
  /*
    2 Automatic
    13 GSM only
    38 LTE only
    51 GSM and LTE only
  * * * *
  Serial.println("connecting to network");
  String res;
  do {
    res = modem.setNetworkMode(38);
    delay(500);
    Serial.println("res: " + res);
  } while (!res);
  Serial.println("connected to network");
  
  /*
    1 CAT-M
    2 NB-Iot
    3 CAT-M and NB-IoT
  * * *
  Serial.println("setting mode");
  do {
    res = modem.setPreferredMode(1);
    delay(500);
  } while (!res);
  Serial.println("mode set");
  */

    Serial.println("========INIT========");

    if (!modem.init()) {
        modemRestart();
        delay(2000);
        Serial.println("Failed to restart modem, attempting to continue without restarting");
        return;
    }

    Serial.println("========SIMCOMATI======");
    modem.sendAT("+SIMCOMATI");
    modem.waitResponse(1000L, res);
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
    res = "";
    Serial.println("=======================");

    Serial.println("=====Preferred mode selection=====");
    modem.sendAT("+CNMP?");
    if (modem.waitResponse(1000L, res) == 1) {
        res.replace(GSM_NL "OK" GSM_NL, "");
        Serial.println(res);
    }
    res = "";
    Serial.println("=======================");


    Serial.println("=====Preferred selection between CAT-M and NB-IoT=====");
    modem.sendAT("+CMNB?");
    if (modem.waitResponse(1000L, res) == 1) {
        res.replace(GSM_NL "OK" GSM_NL, "");
        Serial.println(res);
    }
    res = "";
    Serial.println("=======================");

    
    // Unlock your SIM card with a PIN if needed
    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
        modem.simUnlock(GSM_PIN);
    }

    /*
    for (int i = 0; i <= 4; i++) {
        uint8_t network[] = {
            2,  //Automatic
            13, //GSM only
            38, //LTE only
            51  //GSM and LTE only
        };
        Serial.printf("Try %d method\n", network[i]);
        
        modem.setNetworkMode(network[i]);
        delay(3000);
        bool isConnected = false;
        int tryCount = 60;
        while (tryCount--) {
            int16_t signal =  modem.getSignalQuality();
            Serial.print("Signal: ");
            Serial.print(signal);
            Serial.print(" ");
            Serial.print("isNetworkConnected: ");
            isConnected = modem.isNetworkConnected();
            Serial.println( isConnected ? "CONNECT" : "NO CONNECT");
            if (isConnected) {
                break;
            }
            delay(1000);
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
        if (isConnected) {
            break;
        }
    }
    */
        /*
      2 Automatic
      13 GSM only
      38 LTE only
      51 GSM and LTE only
    * * * */
    Serial.println("connecting to network");
    String res;
    do {
      res = modem.setNetworkMode(38);
      delay(500);
      Serial.println("res: " + res);
    } while (!res);
    Serial.println("connected to network");
    
    /*
      1 CAT-M
      2 NB-Iot
      3 CAT-M and NB-IoT
    * * */
    Serial.println("setting mode");
    do {
      res = modem.setPreferredMode(1);
      delay(500);
    } while (!res);

    
    digitalWrite(LED_PIN, HIGH);

    Serial.println();
    Serial.println("Device is connected .");
    Serial.println();

    Serial.println("=====Inquiring UE system information=====");
    modem.sendAT("+CPSI?");
    if (modem.waitResponse(1000L, res) == 1) {
        res.replace(GSM_NL "OK" GSM_NL, "");
        Serial.println(res);
    }

  getConnectionData();
  blinkLED(100);
  Serial.println("loop active");
}


void blinkLED(float delay_input){
  digitalWrite(LED_PIN, LOW);
  delay(delay_input);
  digitalWrite(LED_PIN, HIGH);
}

void connectToGPRS(){
  Serial.println("begin GPRS connect");
    SerialAT.println("AT+CGDCONT?");
    delay(500);
    if (SerialAT.available()) {
      input = SerialAT.readString();
      input = "email_test";
      for (int i = 0; i < input.length(); i++) {
        if (input.substring(i, i + 1) == "\n") {
          pieces[counter] = input.substring(lastIndex, i);
          lastIndex = i + 1;
          counter++;
        }
        if (i == input.length() - 1) {
          pieces[counter] = input.substring(lastIndex, i);
        }
      }
      // Reset for reuse
      input = "email_test_2";
      counter = 0;
      lastIndex = 0;

      for ( int y = 0; y < numberOfPieces; y++) {
        for ( int x = 0; x < pieces[y].length(); x++) {
          char c = pieces[y][x];  //gets one byte from buffer
          if (c == ',') {
            if (input.indexOf(": ") >= 0) {
              String data = input.substring((input.indexOf(": ") + 1));
              if ( data.toInt() > 0 && data.toInt() < 25) {
                modem.sendAT("+CGDCONT=" + String(data.toInt()) + ",\"IP\",\"" + String(apn) + "\",\"0.0.0.0\",0,0,0,0");
              }
              input = "";
              break;
            }
            // Reset for reuse
            input = "";
          }
          else {
            input += c;
          }
        }
      }
    } else {
      Serial.println("Failed to get PDP!");
    }


    if (modem.isNetworkConnected()) {
      Serial.println("Network connected");
    }else{
      Serial.println("\n\n\nWaiting for network...");
      // Serial.println("wait for network: " + modem.waitForNetwork());
      if (!modem.waitForNetwork()) {
        delay(10000);
        Serial.println("waited for network");
        return;
      }
    }

    if (modem.isNetworkConnected()) {
      Serial.println("Network connected");
    }

    Serial.println("\n---Starting GPRS TEST---\n");
    Serial.println("Connecting to: " + String(apn));
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      delay(10000);
      return;
    }

    Serial.print("GPRS status: ");
    if (modem.isGprsConnected()) {
      Serial.println("connected");
    } else {
      Serial.println("not connected");
    }
}


void disconnectFromGPRS(){
  modem.gprsDisconnect();
  delay(500);
  if (!modem.isGprsConnected()) {
    Serial.println("GPRS disconnected");
  } else {
    Serial.println("GPRS disconnect: Failed.");
  }
}


void enableGPS(void)
{
    // Set SIM7000G GPIO4 HIGH ,turn on GPS power
    // CMD:AT+SGPIO=0,4,1,1
    // Only in version 20200415 is there a function to control GPS power
    modem.sendAT("+SGPIO=0,4,1,1");
    if (modem.waitResponse(10000L) != 1) {
        DBG(" SGPIO=0,4,1,1 false ");
    }
    modem.enableGPS();
}


void disableGPS(void)
{
    // Set SIM7000G GPIO4 LOW ,turn off GPS power
    // CMD:AT+SGPIO=0,4,1,0
    // Only in version 20200415 is there a function to control GPS power
    modem.sendAT("+SGPIO=0,4,1,0");
    if (modem.waitResponse(10000L) != 1) {
        DBG(" SGPIO=0,4,1,0 false ");
    }
    modem.disableGPS();
}


void modemPowerOn()
{
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, LOW);
    delay(1000);    //Datasheet Ton mintues = 1S
    digitalWrite(PWR_PIN, HIGH);
}


void modemPowerOff()
{
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, LOW);
    delay(1500);    //Datasheet Ton mintues = 1.2S
    digitalWrite(PWR_PIN, HIGH);
}


void modemRestart()
{
    modemPowerOff();
    delay(1000);
    modemPowerOn();
}


String getGPSCoordinates(){
  
  Serial.println("Start positioning. Try to locate outdoors.");
  Serial.println("The blue indicator light flashes to indicate positioning.");

  enableGPS();
  
  float lat,  lon;
  while (1) {
      if (modem.getGPS(&lat, &lon)) {
          Serial.println("The location has been locked, the latitude and longitude are:");
          //Serial.print("latitude:"); Serial.println(lat);
          //Serial.print("longitude:"); Serial.println(lon);
          break;
      }
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(2000);
  }

  //get higher precision lat and long from modem.GPSraw
  String raw_GPS = modem.getGPSraw();
  Serial.println("GPSRAW "+ raw_GPS);
  int comma1 = raw_GPS.indexOf(',');
  int comma2 = raw_GPS.indexOf(',',comma1 + 1);
  int comma3 = raw_GPS.indexOf(',',comma2 + 1);
  int comma4 = raw_GPS.indexOf(',',comma3 + 1);
  int comma5 = raw_GPS.indexOf(',',comma4 + 1);
  String lat2 = raw_GPS.substring(comma3+1,comma4);
  String lon2 = raw_GPS.substring(comma4+1,comma5);
  Serial.print("latitude:"); Serial.println(lat2);
  Serial.print("longitude:"); Serial.println(lon2);
  
  disableGPS();
  digitalWrite(LED_PIN, HIGH);
  
  //{"coordinates":"38.567,-121.401"}
  //String coordinates = "\\\"coordinates\\\":\\\"" + String(lat2) + "," + String(lon2) + "\\\"";
  String coordinates = String(lat2) + "," + String(lon2);
  
  return coordinates;
}

void getConnectionData(){
    String ccid = modem.getSimCCID();
    Serial.println("CCID: " + ccid);

    String imei = modem.getIMEI();
    Serial.println("IMEI: " + imei);

    String cop = modem.getOperator();
    Serial.println("Operator: " + cop);

    IPAddress local = modem.localIP();
    Serial.println("Local IP: " + String(local));

    int csq = modem.getSignalQuality();
    Serial.println("Signal quality: " + String(csq));
}

//takes about 3 seconds
void establishTCP(){
    
    modem.sendAT("+CIPSHUT");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("cipshut " + res);
    }
    
    modem.sendAT("+CIPMUX=0");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("cipmux " + res);
    }

    modem.sendAT("+CIPSTATUS");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("cipstatus " + res);
    }

    // set APN
    modem.sendAT("+CSTT=\""+String(apn)+"\"");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("cstt " + res);
    }
    delay(500);

    // check switch connection
    modem.sendAT("+CIICR");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("CIICR" + res);
    }

    // check ip
    modem.sendAT("+CIFSR");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("CIFSR" + res);
    }

    // set to get data manually
    modem.sendAT("+CIPRXGET=1");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("ciprxget=1 " + res);
    }

    delay(500);
    
    // start tcp connection
    modem.sendAT("+CIPSTART=\"TCP\",\"cloudsocket.hologram.io\",9999");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println(res);
    }
    delay(2000);    
}

void sendData(String k, String d, String t){
    // https://support.hologram.io/hc/en-us/articles/360036559494-SIMCOM-SIM7000
    // try terminating payload with \r\n
    // send a message
    // Host: cloudsocket.hologram.io:9999
    // {"k":"[DEVICE_KEY]","d":"[MESSAGE_BODY]","t":"TOPIC_NAME"}
    // {"k":"be75%RzU","d":"{\"coordinates\":\"38.556694,-121.411225\"}","t":"email_test"}
    // {"k":"be75%RzU","d":"{\"coordinates\":\"38.556694,-121.411225\",\"highest_pin\":0}","t":"email_test"}
   
    String payload = "{\"k\":\"" + k + "\",\"d\":" + d + ",\"t\":\"" + t + "\"}";
    int payload_length = payload.length()+1;
    
    Serial.println(payload);
    Serial.println(payload_length);

    // set length of data to send
    modem.sendAT("+CIPSEND="+String(payload_length));
    delay(2000);
    
     byte buf[payload_length];
     payload.getBytes(buf, payload_length);

     for (int i = 0; i<payload_length; i++){
      SerialAT.write(buf[i]);
     }
     Serial.println();
     Serial.println("sent!");
     // Expected Response = SEND OK, [0,0], CLOSED
}

int highestPin(){
  int highest_pin = -1;
  for(int i = 0; i < 6; i++){
    if (String(digitalRead(SENSOR_PINS[i])) == "1"){
      highest_pin = i;
    }
  }
  return highest_pin;
}

String buildPayload(String coordinates, int highest_pin){
  // String coordinates = "\\\"coordinates\\\":\\\"" + String(lat2) + "," + String(lon2) + "\\\"";
  // "\"{\\\"coordinates\\\":\\\"38.556694,-121.411225\\\",\\\"highest_pin\\\":\\\"" + SENSOR_PINS[highest_pin] + "\\\",\\\"pin_percent\\\":\\\"" + SENSOR_LEVELS[highest_pin] + "\\\"}\""
  // Sorry for this disaster of a string
  String output = "\"{\\\"coordinates\\\":\\\"" + coordinates + "\\\",\\\"highest_pin\\\":\\\"" + String(highest_pin) + "[GPIO " + String(SENSOR_PINS[highest_pin]) + "]" + "\\\",\\\"pin_percent\\\":\\\"" + String(SENSOR_LEVELS[highest_pin]) + "\\\"}\"";
  return output;
}

void loop() {

    if (highestPin() >= 0) {
      Serial.println("Sensor triggered");
      
      digitalWrite(LED_PIN, LOW);
      
      // begin 3s polling duration and test for highest pin during duration
      Serial.println("Begin polling");
      int absolute_highest_pin = 0;
      int current_highest_pin = 0;
      for(int i = 0; i < 300; i++){
        current_highest_pin = highestPin();
        if (current_highest_pin > absolute_highest_pin) {
          absolute_highest_pin = current_highest_pin;
        }
        if (i%10 == 0){
          Serial.println("Highest Pin: " + String(absolute_highest_pin));
        
          String output = "";
          for(int i = 0; i < 6; i++){ 
          output += String(digitalRead(SENSOR_PINS[i])) + ",";
          }
          Serial.println(output);
        }
        delay(10);
      }
      Serial.println("End polling");
      digitalWrite(LED_PIN, HIGH);
        
      
      
      Serial.println("Connecting to GPRS");
      connectToGPRS();

      Serial.println("Getting GPS Coordinates");
      String location = getGPSCoordinates();
      Serial.println("Location Payload:" + location);

      String payload = buildPayload(location,absolute_highest_pin);
      Serial.println("Full Payload:" + payload);
      
      Serial.println("Establish TCP Connection");
      establishTCP();

      Serial.println("Sending data");
      sendData(device_key, payload, payload_tag);

      Serial.println("Disconnecting from GPRS");
      disconnectFromGPRS();

      Serial.println("start 10 second delay");
      delay(10000);
      Serial.println("end 10 second delay");
      while (digitalRead(SENSOR_PIN) == HIGH){
        Serial.println("sensor still high (wait 5 seconds)");
        delay(5000);
      }
      Serial.println("loop active");
      blinkLED(100);
    }
    delay(10);
    
    /*
    while (true) {
    if (SerialAT.available()) {
      Serial.write(SerialAT.read());
    }
    if (Serial.available()) {
      SerialAT.write(Serial.read());
    }
    delay(1);
    }
    */
}
