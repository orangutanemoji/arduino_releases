int SENSOR_PINS[] = {32, 33, 34, 35, 21, 22};
bool SENSOR_STATUS[6];
int SENSOR_LEVELS[] = {10, 25, 50, 75, 100, 200};

int counter, lastIndex, numberOfPieces = 24;
String pieces[24], input;
String res = "";


void setup() {
  // Set console baud rate
  Serial.begin(115200);

  for(int i = 0; i < 6; i++){ 
    pinMode(SENSOR_PINS[i], INPUT);
  }
}


void loop() {
  // put your main code here, to run repeatedly:

  String output = "";
  for(int i = 0; i < 6; i++){ 
    output += String(digitalRead(SENSOR_PINS[i])) + ",";
  }
  Serial.println(output);
  delay(10);
}
