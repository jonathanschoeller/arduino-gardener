#include <CmdMessenger.h>
#include <ArduinoJson.h>
#include <LowPower.h>

CmdMessenger cmdMessenger = CmdMessenger(Serial, 0x13);
const int lightPin = A1;
const int xbeeWakePin = 2;
const int valvePins[] = { 3, 4, 5, 6, 7 };
const int valvePinCount = 5;
const char* mqttTopicLight = "arduino/light";

// This is the list of recognized commands. These can be commands that can either be sent or received. 
// In order to receive, attach a callback function to these events
enum
{
  // Commands
  topicMessageCommand,
  wakeUp,
  sendBatch,
  batchDone
};

void attachCommandCallbacks() {
  cmdMessenger.attach(topicMessageCommand, onTopicMessageCommand);
  cmdMessenger.attach(batchDone, onBatchDoneCommand);
}

void setup() {
  Serial.begin(9600);
  pinMode(lightPin, INPUT);

  pinMode(LED_BUILTIN, OUTPUT);

  for (int valveId = 0 ; valveId < valvePinCount ; valveId++){
    pinMode(valvePins[valveId], OUTPUT);
  }
  
  attachCommandCallbacks();
}

void loop() {
  static unsigned long lastLightSent = 0;
  
  Serial.flush();
  sleepXbee();
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
  wakeXbee();
  delay(100);

  getCommands();
  
  unsigned long now = millis();
  
  // This will be very imprecise, since millis only advances when the arduino is not asleep.
  if (now - lastLightSent > 30000)
  {
    sendLightData();
    lastLightSent = now;
    delay(100);
  }
}

void onTopicMessageCommand(){
  String topic = cmdMessenger.readStringArg();
  String message = cmdMessenger.readStringArg();

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()){
    return;
  }

  const char* command = root["cmd"];
  if (String(command) == "open-valve"){
    int valveId = root["id"];
    long openForMs = root["ms"];
    openValve(valveId, openForMs);
  }
}

void openValve(int valveId, long openForMs){
  if (valveId > valvePinCount || valveId < 0){
    return;
  }
  
  int valvePin = valvePins[valveId];
  digitalWrite(valvePin, HIGH);
  delay(openForMs);
  
  digitalWrite(valvePin, LOW);
}

void sendLightData() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["light"] = analogRead(lightPin);
  char buffer[256];
  root.printTo(buffer, sizeof(buffer));
  cmdMessenger.sendCmdStart(topicMessageCommand);
  cmdMessenger.sendCmdArg(mqttTopicLight);
  cmdMessenger.sendCmdArg(buffer);
  cmdMessenger.sendCmdEnd();
}

bool receivingBatch = false;

void onBatchDoneCommand() {
  receivingBatch = false;
}

void getCommands() {
  receivingBatch = true;
  cmdMessenger.sendCmd(sendBatch);

  unsigned long startTime = millis();
  
  do
  {
    cmdMessenger.feedinSerialData();
  } while(receivingBatch && millis() - startTime < 2000 /*2 seconds*/);
}

void sleepXbee(){
  pinMode(xbeeWakePin, INPUT);
  digitalWrite(xbeeWakePin, HIGH);
}

void wakeXbee(){
  pinMode(xbeeWakePin, OUTPUT);
  digitalWrite(xbeeWakePin, LOW);
}

void toggleLight() {
  static int toggle;
  
  if (toggle == 0){
    toggle = 1;
    digitalWrite(LED_BUILTIN, HIGH);
  }else
  {
    toggle = 0;
    digitalWrite(LED_BUILTIN, LOW);
  }
}

