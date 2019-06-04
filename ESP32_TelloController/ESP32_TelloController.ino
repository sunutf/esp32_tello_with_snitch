
// --
// Tello controller for ESP32
// written by bishi 2018.04.06
// --
#include <WiFi.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <SPIFFS.h>
// #include "BluetoothSerial.h"
#include "SimpleBLE.h"

#define CMD_FROM_SNITCH
// #define CMD_FROM_SPIFFS
#define WAIT_TIME 20000

#define CMD_SNITCH_SERIAL Serial2
// -- literal
// pin
const int buttonPin = 12;
const int ledPin = 13;

// WiFi - AP mode (for self)
String ssidAp = "TELLO-D30D28";
String passwordAp = "0000";
const IPAddress ipAp(192, 168, 4, 1);
const IPAddress gatewayAp = ipAp;
const IPAddress subnetAp(255, 255, 255, 0);
const int udpPortAp = 1060;
const int receiveBufferLen = 64;

// WiFi - Client mode (for Tello)
String ssidTello = "TELLO-D30D28";  // overridden later
String passwordTello = "0000";  // overridden later
String ipTello = "192.168.10.1";
const int udpPortTello = 8889;

char c_temp;
String s_command = "";

// function prototype
void writeSsid(String arg);
void writePass(String arg);
void writeDroneCmd(String arg);
void clearDroneCmd(String arg);



// command table
typedef void (pfunc)(String);
typedef struct
{
  char *cmd;    // command code
  pfunc *func;  // function pointer
} settingCommand_t;

settingCommand_t settingCommandTable[] =
{
  // command code   function pointer
  {"ssid",          writeSsid },      // write ssid to SSID.txt
  {"pass",          writePass },      // write password to PASS.txt
  {"cmd",           writeDroneCmd },  // write command to DRONECMD.txt
  {"clear",         clearDroneCmd },  // clear DRONECMD.txt
  {NULL,            NULL }            // terminator
};

// -- variable
// pin
int buttonState = HIGH;

// WiFi - AP mode (for self)
WiFiUDP udp;
char receiveBuffer[receiveBufferLen];

// WiFi - Client mode (for Tello)
bool connectedTello = false;

// other
bool settingModeEnable = false;

//BLE
// BluetoothSerial SerialBT;
SimpleBLE ble;

// -- setup function
void setup() {
  // serial
  Serial.begin(115200);
  CMD_SNITCH_SERIAL.begin(115200); // HardwareSerial UART2 RX/TX
  // SerialBT.begin("ESP32test"); //Bluetooth device name
  ble.begin("leviosa_esp32");

  Serial.println("The device started, now you can pair it with bluetooth!");

  // pin
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  delay(100); // required delay
  digitalWrite(ledPin, HIGH);

  // file system (on internal Flash memory)
  SPIFFS.begin(true);

  // -- determine the operation mode at startup
  buttonState = digitalRead(buttonPin);
  if (buttonState == LOW) {
    settingModeEnable = true;
  }
  Serial.print("settingModeEnable : ");
  Serial.println(settingModeEnable);

  if (settingModeEnable == true) {

    // -- setting mode
    // WiFi
    Serial.println("Initialize WiFi");
    WiFi.softAP(ssidAp.c_str(), passwordAp.c_str());
    delay(100); // required delay
    WiFi.softAPConfig(ipAp, gatewayAp, subnetAp);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address : ");
    Serial.println(myIP);

    // UDP
    udp.begin(udpPortAp);
    Serial.print("AP UDP port : ");
    Serial.println(udpPortAp);
    digitalWrite(ledPin, LOW);

  } else {

    // -- client mode
    // WiFi connect to Tello
//    ssidTello = readTextFile("SSID.txt");
//    Serial.print("SSID Tello : ");
//    Serial.println(ssidTello);
//    passwordTello = readTextFile("PASS.txt");
//    Serial.print("Password Tello : ");
//    Serial.println(passwordTello);
    connectToWiFi(ssidTello.c_str(), passwordTello.c_str());
    digitalWrite(ledPin, LOW);
  }
}

// -- main loop function
void loop() {
  // check mode
  if (settingModeEnable == true) {

    // -- setting mode
    digitalWrite(ledPin, HIGH);
    settingProcess();

  } else {

    // -- client mode
    if (connectedTello == true)
    {
      digitalWrite(ledPin, HIGH);
      buttonState = digitalRead(buttonPin);
      
      #ifdef CMD_FROM_SPIFFS
        if (buttonState == LOW) {
          controlTelloProcess();
        }
      #endif

      #ifdef CMD_FROM_SNITCH
        Serial.println("connect with snitch");
        controlTelloProcess();
      #endif
    } else {
      digitalWrite(ledPin, LOW);
    }
  }
}

// -- setting mode process function
void settingProcess(void)
{
  int receiveLength = udp.parsePacket();
  if (receiveLength) {
    udp.read(receiveBuffer, receiveBufferLen);
    String onePacket = String(receiveBuffer).substring(0, receiveLength);
    Serial.println(onePacket);
    selectFunction(onePacket);
  }
}

// -- select function from table
void selectFunction(String packet)
{
  int index = packet.indexOf(":");
  int len = packet.length();
  String rcvCmd = packet.substring(0, index);

  int i = 0;
  while (settingCommandTable[i].cmd != NULL)
  {
    // serch matched command from table
    if (rcvCmd.equals(settingCommandTable[i].cmd))
    {
      // call function
      String arg = packet.substring(index+1, len);
      settingCommandTable[i].func(arg);
      break;
    }
    i++;
  }
}

// -- client(controll Tello) mode process function
void controlTelloProcess(void)
{

  //send command from snitch, in real time.
  #ifdef CMD_FROM_SNITCH
  unsigned long previous_time = millis();
  bool no_command = false;
  s_command = "";
  
  // while(1){
  //   if (SerialBT.available())
  //   {
  //     Serial.write(SerialBT.read());
  //   }
  // }
  while (!CMD_SNITCH_SERIAL.available())
  {
    if(millis()-previous_time > WAIT_TIME)
    {
      no_command = true;
      break;
    }
  };

  if(false == no_command)
  {
    while (CMD_SNITCH_SERIAL.available())
    {
      c_temp = CMD_SNITCH_SERIAL.read();
      s_command.concat(c_temp);
    }
    Serial.println("end read command");
  }  else  {
    s_command = "command,land,";
  };

  String fileData = s_command;
  int fileDataLen = fileData.length();
  int indexPos = 0;
  int startPos = 0;
  int delayIndexPos = 0;

  while(1)
  {
    indexPos = fileData.indexOf(",", startPos);
    if (indexPos != -1)
    {
      // cut text to comma
      String sendData = fileData.substring(startPos, indexPos);

      if (sendData.startsWith("delay "))
      {
        delayIndexPos = sendData.indexOf(" ");
        String arg = sendData.substring(delayIndexPos + 1);
        Serial.print("delay : ");
        Serial.println(arg);
        delay(arg.toInt());

      } else {
        // send to UDP
        Serial.print("send : ");
        Serial.println(sendData);
        udp.beginPacket(ipTello.c_str(), udpPortTello);
        udp.printf(sendData.c_str());
        udp.endPacket();
      }
      startPos = indexPos + 1;

    } else {
      // end of file
      break;
    }
  }
  Serial.println("finish!");
  #endif

  //send command from internal flash. which upload by pc
  #ifdef CMD_FROM_SPIFFS
  String fileData = readTextFile("DRONECMD.txt");
  int fileDataLen = fileData.length();
  int indexPos = 0;
  int startPos = 0;
  int delayIndexPos = 0;

  while(1)
  {
    indexPos = fileData.indexOf(",", startPos);
    if (indexPos != -1)
    {
      // cut text to comma
      String sendData = fileData.substring(startPos, indexPos);

      if (sendData.startsWith("delay "))
      {
        delayIndexPos = sendData.indexOf(" ");
        String arg = sendData.substring(delayIndexPos + 1);
        Serial.print("delay : ");
        Serial.println(arg);
        delay(arg.toInt());

      } else {
        // send to UDP
        Serial.print("send : ");
        Serial.println(sendData);
        udp.beginPacket(ipTello.c_str(), udpPortTello);
        udp.printf(sendData.c_str());
        udp.endPacket();
      }
      startPos = indexPos + 1;

    } else {
      // end of file
      break;
    }
    
  }
  Serial.println("finish!");
  #endif
}

// -- write ssid to SSID.txt
// registered functions in table
void writeSsid(String arg)
{
  writeTextFile("SSID.txt", arg, "w");
}

// -- write password to PASS.txt
// registered functions in table
void writePass(String arg)
{
  writeTextFile("PASS.txt", arg, "w");
}

// -- write command to DRONECMD.txt
// registered functions in table
void writeDroneCmd(String arg)
{
  // append csv format data to end of file
  String writeArg = arg + ',';
  writeTextFile("DRONECMD.txt", writeArg, "a");
}

// -- clear DRONECMD.txt
// registered functions in table
void clearDroneCmd(String arg)
{
  writeTextFile("DRONECMD.txt", arg, "w");
}

// -- write data to file(internal flash memory)
void writeTextFile(String filename, String text, char *mode) {
  File fd = SPIFFS.open("/" + filename, mode);
  if (!fd) {
    Serial.println("open error:write");
  }
  fd.print(text);
  fd.close();
}

// -- read data from file(internal flash memory)
String readTextFile(String filename) {
  File fd = SPIFFS.open("/" + filename, "r");
  String text = fd.readStringUntil('\n');
  if (!fd) {
    Serial.println("open error:read");
  }
  fd.close();
  return text;
}

// start connect to WiFi AP(Tello)
void connectToWiFi(const char *ssid, const char *password){
  Serial.print("Connecting : ");
  Serial.println(ssid);

  // delete old config
  WiFi.disconnect(true);

  //register event handler
  WiFi.onEvent(wifiEvent);

  WiFi.begin(ssid, password);
  Serial.println("Waiting for WiFi connection...");
}

//wifi event handler
void wifiEvent(WiFiEvent_t event){

  switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        // connected 
        Serial.println("WiFi connected!");
        Serial.print("IP address : ");
        Serial.println(WiFi.localIP());

        //initialize UDP
        udp.begin(WiFi.localIP(), udpPortTello);
        connectedTello = true;
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        // disconnected
        Serial.println("WiFi lost connection");
        connectedTello = false;
        break;
  }
}
