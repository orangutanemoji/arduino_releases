/*
 * Firmware version 0.2
 * Adam Kahn
 * More board information can be found at https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G
 */

/* 
 *  Todo:
 * Disconnect detector/reset loop
 * 
 */

// Be sure to set the correct unit number for the board before flashing!
  #define UNIT_NUMBER 1

// I/O Setup
  int SENSOR_PINS[] = {32, 33, 34, 35, 21, 22};
  int SENSOR_LEVELS[] = {10, 25, 50, 75, 100, 200};

// Import Libraries
  #include <TinyGsmClient.h>
  #include <SPI.h>
  #include <SD.h>
  #include <Ticker.h>
  //#include <SoftwareSerial.h>

// Declare environment variables
  #define TINY_GSM_MODEM_SIM7000
  #define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
  #define SerialAT Serial1
  #define SerialMon Serial
  #define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
  #define TIME_TO_SLEEP  60          // Time ESP32 will go to sleep (in seconds) (Allegedly)
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

// GPRS credentials
  #define GSM_PIN ""
  const char apn[]  = "hologram";     //SET TO YOUR APN
  const char gprsUser[] = "";
  const char gprsPass[] = "";
  String DEVICE_KEYS[] = {"be75%RzU","R{#fvKLL","gJrxNYe5","F?,[UW)h","])^YRD,_","DPR^w}V$",
                          "cKB}Q9>8","i6i0C_jx","EMIO$uF>","w=t9]UCj","DpTtQ4Gn","mNW(h)[K"};
  String device_key = String(DEVICE_KEYS[UNIT_NUMBER]);
  String payload_tag = "unit_message";
  const char server[]   = "cloudsocket.hologram.io";
  const char resource[] = "9999";
  const char newline[] = "\r\n";

// Variables for recieving serial
  const byte numChars = 32;
  char receivedChars[numChars];   // an array to store the received data
  boolean newData = false;
  int counter, lastIndex, numberOfPieces = 24;
  String pieces[24], input;
  String res = "";


// Enable seeing all AT commands for debugging
  #define DUMP_AT_COMMANDS

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


  
/*
 * Setup board
 * 
 */
void setup() {
  // Set console baud rate
    Serial.begin(115200);
    delay(10);

  // Set up input pins
    for(int i = 0; i < 6; i++){ 
      pinMode(SENSOR_PINS[i], INPUT);
    }

  // Turn LED OFF
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

  // Power cycle GPS
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

  // Begin Serial
  Serial.println("\nWait...");
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Initialize Modem
    Serial.println("Initializing modem...");
    // Restart takes quite some time, to skip it, call init() instead of restart()
    if (!modem.restart()) {
      Serial.println("Failed to restart modem, attempting to continue without restarting");
    }

  //Print Modem info
    String name = modem.getModemName();
    delay(500);
    Serial.println("Modem Name: " + name);
    String modemInfo = modem.getModemInfo();
    delay(500);
    Serial.println("Modem Info: " + modemInfo);

  // Turn off GPS power
    modem.sendAT("+SGPIO=0,4,1,0");

  // Unlock your SIM card with a PIN if needed
    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
      modem.simUnlock(GSM_PIN);
    }

  // Test for Initialization
    Serial.println("========INIT_TEST========");
    if (!modem.init()) {
        modemRestart();
        delay(2000);
        Serial.println("Failed to restart modem, attempting to continue without restarting");
        return;
    }

  // Print inital diagnostics data
    Serial.println("========SIMCOMATI======");
    modem.sendAT("+SIMCOMATI");
    modem.waitResponse(1000L, res);
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
    res = "";
    Serial.println("=======================");

  //Select Preferred Modes
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

    
  // Unlock SIM card with a PIN if needed
    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
        modem.simUnlock(GSM_PIN);
    }

  //Connect to network (see bottom of script for deprecated version)
    Serial.println("connecting to network");
    String res;
    do {
      res = modem.setNetworkMode(38);
      delay(500);
      Serial.println("res: " + res);
    } while (!res);
    Serial.println("connected to network");
    
  //Set Mode
    Serial.println("setting mode");
    do {
      /* 
       * 1 CAT-M
       * 2 NB-Iot
       * 3 CAT-M and NB-IoT
       */
      res = modem.setPreferredMode(1);
      delay(500);
    } while (!res);

  // print connection
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

  // GPS pre-location
    Serial.println("GPS pre-locating");
    getGPSCoordinates();
    Serial.println("GPS pre-located");

  // print setup done
    blinkLED(100);
    Serial.println("loop active");
}

/*
 * blinks the led
 */
void blinkLED(float delay_input){
  digitalWrite(LED_PIN, LOW);
  delay(delay_input);
  digitalWrite(LED_PIN, HIGH);
}

/*
 * connects to cell network
 */
void connectToGPRS(){

  // begin connect
  // need to streamline this
    Serial.println("begin GPRS connect");
    SerialAT.println("AT+CGDCONT?");
    delay(500);
    if (SerialAT.available()) {
      input = SerialAT.readString();
      input = payload_tag;
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
      input = payload_tag;
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


  // test for connection
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

/*
 * disconnects from cell network
 */
void disconnectFromGPRS(){
  modem.gprsDisconnect();
  delay(500);
  if (!modem.isGprsConnected()) {
    Serial.println("GPRS disconnected");
  } else {
    Serial.println("GPRS disconnect: Failed.");
  }
}

/*
 * enables the GPS
 */
void enableGPS(void)
{
    modem.sendAT("+SGPIO=0,4,1,1");
    if (modem.waitResponse(10000L) != 1) {
        DBG(" SGPIO=0,4,1,1 false ");
    }
    modem.enableGPS();
}

/*
 * disables the GPS
 */
void disableGPS(void)
{
    modem.sendAT("+SGPIO=0,4,1,0");
    if (modem.waitResponse(10000L) != 1) {
        DBG(" SGPIO=0,4,1,0 false ");
    }
    modem.disableGPS();
}

/*
 * enable the modem
 */
void modemPowerOn()
{
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, LOW);
    delay(1000);    //Datasheet Ton mintues = 1S
    digitalWrite(PWR_PIN, HIGH);
}

/*
 * disable the modem
 */
void modemPowerOff()
{
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, LOW);
    delay(1500);    //Datasheet Ton mintues = 1.2S
    digitalWrite(PWR_PIN, HIGH);
}

/*
 * restart the modem
 */
void modemRestart()
{
    modemPowerOff();
    delay(1000);
    modemPowerOn();
}

/*
 * gets gps coordinates 
 * takes about 30s first time, quicker on subsequent runs
 */
String getGPSCoordinates(){

  //start positioning
    Serial.println("Start positioning. Try to locate outdoors.");
    Serial.println("The blue indicator light flashes to indicate positioning.");
    enableGPS();

  // get lat and long
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

  // disable GPS
    disableGPS();
    digitalWrite(LED_PIN, HIGH);
  
  // return coordinates as a string
    String coordinates = String(lat2) + "," + String(lon2);
    return coordinates;
}

/*
 * gets connection information
 */
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

/*
 * connect to hologram servers
 * takes about 5 seconds, needs streamlining
 */
void establishTCP(){

  // close existing connections
    modem.sendAT("+CIPSHUT");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("cipshut " + res);
    }

  // set to single connection (CIPMUX=1 supports multiple connections)
    modem.sendAT("+CIPMUX=0");
    if (modem.waitResponse(1000L, res) == 1) {
        Serial.println("cipmux " + res);
    }

  // check status
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
    
    // unsure if delay is necessary, could be streamlined
    delay(2000);    
}

/*
 * sends a json to hologram.io
 */
void sendData(String k, String d, String t){
    // https://support.hologram.io/hc/en-us/articles/360036559494-SIMCOM-SIM7000
    // Host: cloudsocket.hologram.io:9999
    // {"k":"[DEVICE_KEY]","d":"[MESSAGE_BODY]","t":"TOPIC_NAME"}
   
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

/*
 * create the data part of the message json
 */
String buildPayload(String coordinates, int highest_pin){
  // Sorry for this disaster of a string, not really much of a choice
  String output = "\"{\\\"coordinates\\\":\\\"" + coordinates + "\\\",\\\"highest_pin\\\":\\\"" + String(highest_pin) + "[GPIO " + String(SENSOR_PINS[highest_pin]) + "]" + "\\\",\\\"pin_percent\\\":\\\"" + String(SENSOR_LEVELS[highest_pin]) + "\\\"}\"";
  return output;
}

/*
 * test for highest pin
 */
int highestPin(){
  int highest_pin = -1;
  for(int i = 0; i < 6; i++){
    if (String(digitalRead(SENSOR_PINS[i])) == "1"){
      highest_pin = i;
    }
  }
  return highest_pin;
}

/*
 * main loop
 */
void loop() {
  // test for pin triggered
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

    // Connect to GPRS     
      Serial.println("Connecting to GPRS");
      connectToGPRS();

    // Get GPS Coordinates
      Serial.println("Getting GPS Coordinates");
      String location = getGPSCoordinates();
      Serial.println("Location Payload:" + location);

    // Create message
      String payload = buildPayload(location,absolute_highest_pin);
      Serial.println("Full Payload:" + payload);

    // Connect to hologram.io
      Serial.println("Establish TCP Connection");
      establishTCP();

      Serial.println("Sending data");
      sendData(device_key, payload, payload_tag);

    // Disconnect from GPRS
      Serial.println("Disconnecting from GPRS");
      disconnectFromGPRS();

    // Wait 10s and  prevent repeat texts
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
}


/*
 * Deprecated Code
 */

// 1. Connection
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
