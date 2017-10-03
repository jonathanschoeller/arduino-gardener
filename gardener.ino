#include <CmdMessenger.h>
#include <ArduinoJson.h>
#include <LowPower.h>

CmdMessenger cmdMessenger = CmdMessenger(Serial, 0x13);
const int lightPin = A1;
const int xbeeWakePin = 2;
const int valvePins[] = { 3, 4, 5, 6, 7 };
const int valvePinCount = 5;
const char* mqttTopic = "arduino";

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

  pinMode(LED_BUILTIN, OUTPUT);to

  for (int valveId = 0 ; valveId < valvePinCount ; valveId++){
    pinMode(valvePins[valveId], OUTPUT);
  }
  
  attachCommandCallbacks();
}

void loop() {
  Serial.flush();
  sleepXbee();
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
  wakeXbee();
  delay(100);

  getCommands();
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
  cmdMessenger.sendCmdArg(mqttTopic);
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

  int startTime = millis();
  
  do
  {
    cmdMessenger.feedinSerialData();
  } while(receivingBatch && millis() - startTime < 2000 /*2 seconds*/);
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

void wakeXbee(){
  pinMode(xbeeWakePin, OUTPUT);
  digitalWrite(xbeeWakePin, LOW);
}

void sleepXbee(){
  pinMode(xbeeWakePin, INPUT);
  digitalWrite(xbeeWakePin, HIGH);
}

