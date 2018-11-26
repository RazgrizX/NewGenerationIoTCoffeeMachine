#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Time.h>
#include <Wire.h>
#include <EEPROM.h>

#define Addr 0x4A

//Messages
String sensorWarning = "";
String errorMessage = "";
String sensorOutput = "";

//Classes
class State {
    int id;
    String sign, link, color;
  public:
    State();
    State(int id_, String sign_, String link_, String color_) {
      id = id_;
      sign = sign_;
      link = link_;
      color = color_;
    }
    int getId() {
      return id;
    }
    String getSign() {
      return sign;
    }
    String getLink() {
      return link;
    }
    String getColor() {
      return color;
    }
};

class StateList {
    State * states;
    int s;
  public:
    StateList(State * states_, int s_) {
      states = states_;
      s = s_;
    }
    State getState(int id) {
      for (int i = 0; i < s; i++) {
        if (id == states[i].getId()) {
          return states[i];
        }
      }
      return State(0, "", "", "");
    }
    bool isState(int id) {
      for (int i = 0; i < s; i++) {
        if (id == states[i].getId()) {
          return true;
        }
      }
      return false;
    }
    int getLength() {
      return s;
    }
};

class Sensor {
    int sda, scl, threshold, range, maxLight;
  public:
    Sensor(int sda_, int scl_, float threshold_, float range_, float maxLight_) {
      sda = sda_;
      scl = scl_;
      threshold = threshold_;
      range = range_;
      maxLight = maxLight_;
    }

    int getSda() {
      return sda;
    }

    int getScl() {
      return scl;
    }

    int getThreshold() {
      return threshold;
    }

    int getRange() {
      return range;
    }

    int getMax() {
      return maxLight;
    }

    int ReadSensor() {
      float light = GetLight();
      Serial.println("Sensor #" + String(sda) + " - Ambient Light Level (Lux) = " + String(light));
      sensorOutput += "</br>Sensor #" + String(scl) + " - Ambient Light Level (Lux) = " + String(light);
      if (maxLight != 0 && light > maxLight) {
        return 9;
      }
      float dif = light - threshold;
      if (range != 0 && fabs(dif) <= range) {
        return 1;
      } else if (dif > 0) {
        return 0;
      } else if (dif <= 0) {
        return 2;
      }
      //default
      Serial.println("checkSensorError");
      return 9;
    }

  private:
    float GetLight() {
      Wire.begin(sda, scl);
      delay(100);

      //SensorConfiguration
      Wire.beginTransmission(Addr);
      Wire.write(0x02);  // Select configuration register
      Wire.write(0x40);  // Continuous mode, Integration time = 800 ms
      Wire.endTransmission();  // Stop I2C transmission
      delay(300);

      unsigned int data[2];

      Wire.beginTransmission(Addr);  // Start I2C Transmission
      Wire.write(0x03);  // Select data register
      Wire.endTransmission();  // Stop I2C transmission
      Wire.requestFrom(Addr, 2);  // Request 2 bytes of data

      // Read 2 bytes of data: luminance msb, luminance lsb
      if (Wire.available() == 2) {
        data[0] = Wire.read();
        data[1] = Wire.read();
      }

      // Convert the data to lux
      int exponent = (data[0] & 0xF0) >> 4;
      int mantissa = ((data[0] & 0x0F) << 4) | (data[1] & 0x0F);
      float luminance = pow(2, exponent) * mantissa * 0.045;
      return luminance;
    }
};

//Cridentials for the network
//Debug
String ssid = "VladWiFi";
String pass = "sigmadelta";
IPAddress ip(192, 168, 1, 200);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

//Production
//char* ssid = "MA210";
//char* pass = "studentMAMK";
//IPAddress ip(172, 16, 1, 150);
//IPAddress gateway(172, 16, 0, 5);
//IPAddress subnet(255, 255, 248, 0);

//Sensors Config sda/scl/threshold/range/max
int SENSNUM = 4;
Sensor sensors[] = {
  Sensor(12, 5, 2, 1, 20),
  Sensor(4 , 5, 3, 1, 25),
  Sensor(14, 5, 4, 1, 35),
  Sensor(13, 5, 4, 2, 40)
};

//States
State states[] = {
  State(   0, "&nbsp0%-20%",       "https://www.dropbox.com/s/iqie1455hxwisnf/Jug0-20.jpg?raw=1",    "black"),
  State(   1, "&nbsp20%",          "https://www.dropbox.com/s/iqie1455hxwisnf/Jug0-20.jpg?raw=1",    "black"), //change image?
  State(   2, "&nbsp20%-40%",      "https://www.dropbox.com/s/xq9zvam7vu10ya7/Jug20-40.jpg?raw=1",   "black"),
  State(  12, "&nbsp40%",          "https://www.dropbox.com/s/xq9zvam7vu10ya7/Jug20-40.jpg?raw=1",   "black"), //change image?
  State(  22, "&nbsp40%-60%",      "https://www.dropbox.com/s/mk2y9f1l3w5290u/Jug40-60.jpg?raw=1",   "black"),
  State( 122, "&nbsp60%",          "https://www.dropbox.com/s/mk2y9f1l3w5290u/Jug40-60.jpg?raw=1",   "black"), //change image?
  State( 222, "&nbsp60%-80%",      "https://www.dropbox.com/s/wr6dstkucd3u1ew/Jug60-80.jpg?raw=1",   "white"),
  State(1222, "&nbsp80%",          "https://www.dropbox.com/s/wr6dstkucd3u1ew/Jug60-80.jpg?raw=1",   "white"), //change image?
  State(2222, "80%-100%",          "https://www.dropbox.com/s/4p5ems8ccu0qmmm/Jug80-100.jpg?raw=1",  "white"),
  State(9999, "Jug missing &nbsp", "https://www.dropbox.com/s/mk2y9f1l3w5290u/Jug40-60.jpg?raw=1",   "black")
};
StateList stateList(states, SENSNUM * 2 + 1 + 1);
State currentState = stateList.getState(9999);
State contenderState = stateList.getState(9999);

//Configurations
bool netStatic = true;
int CHANGESTATE_INTERVAL = 4;
int LOOP_INTERVAL = 1000;
int COFFEERISES_INTERVAL = 2;
int reloadTimer = 600;
bool debug = true;

//Counters
int configTimeLimit = 180;
int changeStateCounter = 0;
int loopCounter = 0;
int coffeeRisesCounter = 0;

//Global variables
String CURRENT_MODE = "CONFIG_MODE";
bool APSuccess = false;
bool coffeeRises = false;
unsigned long coffeeBrewedSeconds;
bool Blackout = false;

//Modules
ESP8266WebServer server;

void setup() {
  Serial.begin(115200);
  delay(1000);
  EEPROM.begin(512);
  SetEndpoints();
  SetMode(CURRENT_MODE);
}

void loop() {
  if (CURRENT_MODE == "USER_MODE" && WiFi.status() != WL_CONNECTED) {
    Connect();
  }

  if (loopCounter == 0) {
    if (CURRENT_MODE == "USER_MODE") {

      //Message into console
      if (debug == true) {
        Serial.println("------------------------------------------------------------");
        Serial.println("Current State: " + String(currentState.getId()));
        Serial.println("Contender State: " + String(contenderState.getId()));
        Serial.println("Counter: " + String(changeStateCounter));
        Serial.println("CoffeeRises: " + String(coffeeRises));
        Serial.println("CoffeeRises counter: " + String(coffeeRisesCounter));
        if (coffeeBrewedSeconds != NULL) 
          Serial.println("Coffee brewed on " + String(coffeeBrewedSeconds));
      } else
        Serial.println(":");
        
      //Clean message wariables
      errorMessage = "";
      sensorWarning = "";
      sensorOutput = "";

      int readings[SENSNUM];
      for ( int i = 0; i < SENSNUM; i++) {
        readings[i] = sensors[i].ReadSensor();
      }
      State newState = GetNewState(readings);
      ProcessState(newState);
      delay(200);
    }
    else if (CURRENT_MODE == "CONFIG_MODE") {
      Serial.println();
      Serial.print("Server IP address: ");
      Serial.println(WiFi.softAPIP());
      Serial.print("AP Started:");
      Serial.println(APSuccess);
      Serial.print("Time Elapsed: ");
      Serial.println(millis() / 1000);
      Serial.print("Time limit: ");
      Serial.println(configTimeLimit);
      delay(500);
      if (configTimeLimit < millis() / 1000)
        SetMode("USER_MODE");
    }
  }
  loopCounter = (loopCounter + 1) % LOOP_INTERVAL;
  server.handleClient();
}

void SetMode(String MODE) {
  if (MODE == "CONFIG_MODE") {
    CURRENT_MODE = MODE;
    StartAP();
    configTimeLimit = millis() / 1000 + 180;
  } else if (MODE == "USER_MODE") {
    CURRENT_MODE = MODE;
    SetConfigs();
    Connect();
  }
  server.begin();
}

void Connect() {
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(1000);
  if (netStatic) {
    WiFi.config(ip, gateway, subnet);
    Serial.print("IP:");
    Serial.println(ip);
    Serial.print("Mask:");
    Serial.println(subnet);
    Serial.print("Gateway:");
    Serial.println(gateway);
  }
  
  char ssidchar[ssid.length()];
  ssid.toCharArray(ssidchar, ssid.length() + 1);
  char passchar[pass.length() + 1];
  pass.toCharArray(passchar, pass.length() + 1);
  WiFi.begin(ssidchar, passchar);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  
  if (netStatic)
    WiFi.config(ip, gateway, subnet);
  Serial.println("");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void StartAP() {
  IPAddress ipAP(192, 168, 0, 1);
  IPAddress gatewayAP(192, 168, 0, 1);
  IPAddress subnetAP(255, 255, 255, 0);
  char* ssidAP = "IoTCoffeeConfig";
  char* passAP = "12345678";
  String passAPLoad = ReadString(385, 401);
  if (passAPLoad.length() >= 8) {
    for (int i = 0; i < passAPLoad.length(); i++) {
      passAP[i] = passAPLoad[i];
    }
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig (ipAP, gatewayAP, subnetAP);
  APSuccess = WiFi.softAP(ssidAP, passAP);
  delay(4000);
}

State GetNewState(int readings[]) {
  int id = ArrayToInt(readings, SENSNUM);
  //Check the validity. If not valid run error check
  Serial.println(id);
  while (!stateList.isState(id)) {
    //check for 9's
    int maxes = 0;
    for (int i = 0; i < SENSNUM; i++) {
      if (readings[i] == 9) {
        readings[i] = 0;
        maxes++;
      }
    }
    if (maxes > (SENSNUM / 2))
      id = 9999;
    else
      id = ArrayToInt(readings, SENSNUM);

    if (!stateList.isState(id)) {
      //error check
      int errors[SENSNUM];
      for (int i = 0; i < SENSNUM; i++) {
        errors[i] = 0;
      }
      for (int i = 0; i < SENSNUM; i++) {
        for (int j = 0; j < SENSNUM; j++) {
          if ((i > j && readings[i] < readings[j]) || (i < j && readings[i] > readings[j]) || (i != j && readings[i] == 1 && readings[j] == 1)) {
            Serial.println("error " + String(i) + " - " + String(j) + "readings: " + String(readings[i]) + " - " + String(readings[i]));
            errors[i]++;
            errors[j]++;
            delay(200);
          }
        }
      }
      if (debug == true) {
        Serial.println(errors[0]);
        Serial.println(errors[1]);
        Serial.println(errors[2]);
        Serial.println(errors[3]);
      }
      int maxError = 0;
      int failedSensor = 0;
      int potentiallyFailed[SENSNUM];
      int pFCount = 0;
      for (int i = 0; i < SENSNUM; i++) {
        if (errors[i] > maxError) {
          maxError = errors[i];
          failedSensor = i;
          for (int j = 0; j < pFCount; j++) {
            potentiallyFailed[pFCount] = NULL;
          }
          pFCount = 0;
        }
        else if (errors[i] != 0 && errors[i] == maxError) {
          potentiallyFailed[pFCount] = i;
          pFCount++;
        }
      }
      sensorWarning += "</br>Sensor # " + String(failedSensor) + " is probably dirty or inoperational!";
      Serial.println("Sensor # " + String(failedSensor) + " is probably dirty or inoperational!\n");
      for (int i = 0; i < pFCount; i++) {
        if (potentiallyFailed[i] != NULL) {
          sensorWarning += "</br>Sensor # " +  String(potentiallyFailed[i]) + " could also have caused this issue.";
          Serial.println("Sensor # " +  String(potentiallyFailed[i]) + " Could also be a reason of this issue.\n");
        }
      }

      //Fix error
      if (failedSensor == 0)
        readings[failedSensor] = 0;
      else if (failedSensor == SENSNUM - 1) {
        if (readings[failedSensor - 1] == 0)
          readings[failedSensor] = 0;
        else
          readings[failedSensor] = 2;
      }
      else
      {
        if (readings[failedSensor + 1] < 2 || readings[SENSNUM - 1] < 2)
          readings[failedSensor] = 0;
        else {
          if (readings[failedSensor - 1] == 0)
            readings[failedSensor - 1] = 1;
          else
            readings[failedSensor] = 2;
        }
      }
      id = ArrayToInt(readings, SENSNUM);
    }
  }
  State result = stateList.getState(id);
  return result;
}

int ArrayToInt(int *arr, int s) {
  int result = 0;
  for (int i = 0; i < s; i++) {
    result += pow(10, s - i - 1) * arr[i];
  }
  return result;
}

void ProcessState(State newState) {
  if (newState.getId() != currentState.getId()) {
    if (newState.getId() == contenderState.getId()) {
      changeStateCounter++;
      if (changeStateCounter >= CHANGESTATE_INTERVAL) {
        if (currentState.getId() != contenderState.getId())
          StateChanged(currentState, contenderState); //check for events
        currentState = contenderState;
        changeStateCounter = 0;
      }
    }
    else {
      contenderState = newState;
      changeStateCounter = 0;
    }
  }
}

void StateChanged(State oldState, State newState) {
  //check for coffee been brewing
  if (oldState.getId() < newState.getId()) {
    coffeeRisesCounter++;
    if (coffeeRisesCounter >= COFFEERISES_INTERVAL) {
      coffeeBrewedSeconds = millis()/1000;
      coffeeRises = true;
    }
  }
  else if (oldState.getId() > newState.getId()) {
    coffeeRisesCounter = 0;
    coffeeRises = false;
  }

  //check for blackout
  if (newState.getId() == 2222 && oldState.getId() < 222 && !coffeeRises)
    Blackout = true;
  else
    Blackout = false;
}

bool WriteShort(unsigned short value, int addres1, int addres2) {
  int a = value / 256;
  int b = value % 256;
  EEPROM.write(addres1, a);
  EEPROM.write(addres2, b);
  bool commit = EEPROM.commit();
  return commit;
}

unsigned short ReadShort(int address1, int address2) {
  int a = EEPROM.read(address1);
  int b = EEPROM.read(address2);
  short result = a * 256 + b;
  return result;
}

bool WriteString(String source, int addressMin, int addressMax) {
  Serial.println("source");
  Serial.println(source);
  Serial.println(source.length());
  if (source.length() > addressMax - addressMin + 1)
    return false;
  int j = 0;
  for (int i = addressMin; i <= addressMax; i++) {
    if (j < source.length())
      EEPROM.write(i, (byte)source[j]);
    else
      EEPROM.write(i, 0);
    j++;
  }
  bool commit = EEPROM.commit();
  return commit;
}

String ReadString(int addressMin, int addressMax) {
  String result = "";
  int slots = addressMax - addressMin + 1;
  for (int i = addressMin; i <= addressMax; i++) {
    char r = EEPROM.read(i);
    if (r != NULL)
      result += r;
  }
  return result;
}

bool CleanEEPROM(int addressMin, int addressMax) {
  for (int i = addressMin; i <= addressMax; i++)
    EEPROM.write(i, 0);
  bool commit = EEPROM.commit();
  return commit;
}

void SetConfigs() {
  //Network
  String ssidLoad = "";
  ssidLoad = ReadString(0, 15);

  if (ssidLoad != "") {
    ssid = ssidLoad;
  }
  Serial.print("ssid: ");
  Serial.println(ssid);
  String passLoad = "";
  passLoad = ReadString(16, 31);
  Serial.print("passLoad: ");
  Serial.println(passLoad);
  Serial.println(passLoad.length());
  if (passLoad.length() >= 8) {
    pass = passLoad;
  }
  Serial.print("pass: ");
  Serial.println(pass);
  IPAddress ipLoad(EEPROM.read(32), EEPROM.read(33), EEPROM.read(34), EEPROM.read(35));
  IPAddress subnetLoad(EEPROM.read(36), EEPROM.read(37), EEPROM.read(38), EEPROM.read(39));
  IPAddress gatewayLoad(EEPROM.read(40), EEPROM.read(41), EEPROM.read(42), EEPROM.read(43));
  if (ipLoad != NULL) {
    ip = ipLoad;
  }
  if (subnetLoad != NULL) {
    subnet = subnetLoad;
  }
  if (gatewayLoad != NULL) {
    gateway = gatewayLoad;
  }
  Serial.print("ip: ");
  Serial.println(ip);
  Serial.print("subnet: ");
  Serial.println(subnet);
  Serial.print("gateway: ");
  Serial.println(gateway);
  byte SENSNUMLoad = EEPROM.read(44);
  if (SENSNUMLoad <= 4)
    SENSNUM = SENSNUMLoad;
  Serial.print("sensnum: ");
  Serial.println(SENSNUM);
  
  //Sensors
  for (int i = 0; i < SENSNUM; i++) {
    byte sdaLoad = EEPROM.read(i * 5 + 45);
    byte sclLoad = EEPROM.read(i * 5 + 46);
    byte thrLoad = EEPROM.read(i * 5 + 47);
    byte rngLoad = EEPROM.read(i * 5 + 48);
    byte maxLoad = EEPROM.read(i * 5 + 49);
    if (sdaLoad != sclLoad && ((sdaLoad >= 1 && sdaLoad <= 5) || (sdaLoad >= 9 && sdaLoad <= 16)) && ((sclLoad >= 1 && sclLoad <= 5) || (sclLoad >= 9 && sclLoad <= 16))) {

    }
    else {
      sdaLoad = sensors[i].getSda();
      sclLoad = sensors[i].getScl();
    }
    if (thrLoad == 0)
      thrLoad = sensors[i].getThreshold();
    sensors[i] = Sensor(sdaLoad, sclLoad, thrLoad, rngLoad, maxLoad);
    Serial.print("Sensor#");
    Serial.print(i);
    Serial.println(" : ");
    Serial.print("sda: ");
    Serial.println(sensors[i].getSda());
    Serial.print("scl: ");
    Serial.println(sensors[i].getScl());
    Serial.print("threshold: ");
    Serial.println(sensors[i].getThreshold());
    Serial.print("range: ");
    Serial.println(sensors[i].getRange());
    Serial.print("max: ");
    Serial.println(sensors[i].getMax());
  }
  
  //States
  for (int i = 0; i < 10; i++) {
    String signLoad = ReadString(i * 32 + 65, i * 32 + 88);
    if (signLoad == "")
      signLoad = states[i].getSign();
    String colorLoad = ReadString(i * 32 + 89, i * 32 + 96);
    if (colorLoad == "")
      colorLoad = states[i].getColor();
    states[i] = State(StateName(i).toInt(), signLoad, states[i].getLink(), colorLoad);
    Serial.print("State#");
    Serial.print(states[i].getId());
    Serial.println(" : ");
    Serial.print("sign: ");
    Serial.println(states[i].getSign());
    Serial.print("threshold: ");
    Serial.println(states[i].getLink());
    Serial.print("color: ");
    Serial.println(states[i].getColor());
  }
  
  //Configs
  byte netStaticLoad = EEPROM.read(402);
  if (netStaticLoad == 0)
    netStatic = false;
  if (netStaticLoad == 1)
    netStatic = true;
  Serial.print("netstatic: ");
  Serial.println(netStatic);
  CHANGESTATE_INTERVAL = EEPROM.read(403);
  Serial.print("change state interval: ");
  Serial.println(CHANGESTATE_INTERVAL);
  unsigned short loopIntervalLoad = ReadShort(404, 405);
  if (loopIntervalLoad != 0)
    LOOP_INTERVAL = loopIntervalLoad;
  Serial.print("loop interval: ");
  Serial.println(LOOP_INTERVAL);
  byte coffeeRisesIntevalLoad = EEPROM.read(406);
  if (coffeeRisesIntevalLoad <= 3)
    COFFEERISES_INTERVAL = coffeeRisesIntevalLoad;
  Serial.print("coffee rises interval: ");
  Serial.println(COFFEERISES_INTERVAL);
  reloadTimer = ReadShort(407, 408);
  Serial.print("reload timer: ");
  Serial.println(reloadTimer);
  byte debugLoad = EEPROM.read(409);
  if (debugLoad == 0)
    debug = false;
  if (debugLoad == 1)
    debug = true;
  Serial.print("debug mode enabled: ");
  Serial.println(debug);
}

void SetEndpoints() {
  server.on("/", HTTP_GET, []() {
    configTimeLimit = millis() / 1000 + 180;
    server.send(200, "text/html", Layout(CURRENT_MODE));
  });

  server.on("/network", HTTP_POST, []() {
    if (CURRENT_MODE == "CONFIG_MODE") {
      configTimeLimit = millis() / 1000 + 180;
      bool result = false;
      if (server.hasArg("ssid") && server.hasArg("pass")) {
        bool op1 = WriteString(server.arg("ssid"), 0, 15);
        bool op2 = WriteString(server.arg("pass"), 16, 31);
        if (op1 == true && op2 == true) {
          result = true;
        }
      }
      if (result) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
      } else
        server.send ( 400, "text/plain", "Bad request");
    } else
      server.send(403, "text/plain", "Device is in user mode now!");

  });

  server.on("/address", HTTP_POST, []() {
    if (CURRENT_MODE == "CONFIG_MODE") {
      configTimeLimit = millis() / 1000 + 180;
      bool result = false;
      if (server.hasArg("ipoct0") && server.hasArg("ipoct1") && server.hasArg("ipoct2") && server.hasArg("ipoct3") &&
          server.hasArg("mkoct0") && server.hasArg("mkoct1") && server.hasArg("mkoct2") && server.hasArg("mkoct3") &&
          server.hasArg("gtoct0") && server.hasArg("gtoct1") && server.hasArg("gtoct2") && server.hasArg("gtoct3") && server.hasArg("static"))
      {
        IPAddress ipget(server.arg("ipoct0").toInt(), server.arg("ipoct1").toInt(), server.arg("ipoct2").toInt(), server.arg("ipoct3").toInt());
        IPAddress mkget(server.arg("mkoct0").toInt(), server.arg("mkoct1").toInt(), server.arg("mkoct2").toInt(), server.arg("mkoct3").toInt());
        IPAddress gtget(server.arg("gtoct0").toInt(), server.arg("gtoct1").toInt(), server.arg("gtoct2").toInt(), server.arg("gtoct3").toInt());
        //IP address/mask validation works?
        EEPROM.write(32, (byte)server.arg("ipoct0").toInt());
        EEPROM.write(33, (byte)server.arg("ipoct1").toInt());
        EEPROM.write(34, (byte)server.arg("ipoct2").toInt());
        EEPROM.write(35, (byte)server.arg("ipoct3").toInt());
        EEPROM.write(36, (byte)server.arg("mkoct0").toInt());
        EEPROM.write(37, (byte)server.arg("mkoct1").toInt());
        EEPROM.write(38, (byte)server.arg("mkoct2").toInt());
        EEPROM.write(39, (byte)server.arg("mkoct3").toInt());
        EEPROM.write(40, (byte)server.arg("gtoct0").toInt());
        EEPROM.write(41, (byte)server.arg("gtoct1").toInt());
        EEPROM.write(42, (byte)server.arg("gtoct2").toInt());
        EEPROM.write(43, (byte)server.arg("gtoct3").toInt());
        EEPROM.write(402, (byte)server.arg("static").toInt());
        result = EEPROM.commit();
      }

      if (result) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
      } else
        server.send ( 400, "text/plain", "Bad request");
    } else
      server.send(403, "text/plain", "Device is in user mode now!");

  });

  server.on("/sensors", HTTP_POST, []() {
    if (CURRENT_MODE == "CONFIG_MODE") {
      configTimeLimit = millis() / 1000 + 180;
      bool result = false;
      //cycle throught from 0 to 3. use created strings
      if (server.hasArg("sensnum")) {
        int sensnum = server.arg("sensnum").toInt();
        EEPROM.write(44, sensnum);
        for (int i = 0; i < sensnum; i++) {
          String index = String(i);
          if (server.hasArg("sda" + index) && server.hasArg("scl" + index) && server.hasArg("thr" + index) && server.hasArg("rng" + index) && server.hasArg("max" + index)) {
            byte sda = (byte)server.arg("sda" + index).toInt();
            byte scl = (byte)server.arg("scl" + index).toInt();
            byte thr = (byte)server.arg("thr" + index).toInt();
            byte rng = (byte)server.arg("rng" + index).toInt();
            byte maxi = (byte)server.arg("max" + index).toInt();
            if ((sda > 0 && sda <= 5 || sda >= 9 && sda <= 16) && (scl > 0 && scl <= 5 || scl >= 9 && scl <= 16) && sda != scl &&
                maxi >= 0 && maxi <= 255 && thr > 0  && thr <= 255 && rng >= 0 && rng <= 255 && thr - rng > 0 && thr + rng < maxi) {
              EEPROM.write(45 + (i * 5), sda);
              EEPROM.write(46 + (i * 5), scl);
              EEPROM.write(47 + (i * 5), thr);
              EEPROM.write(48 + (i * 5), rng);
              EEPROM.write(49 + (i * 5), maxi);
            }
          }
        }
        result = EEPROM.commit();
      }
      if (result) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
      } else
        server.send ( 400, "text/plain", "Bad request");
    } else
      server.send(403, "text/plain", "Device is in user mode now!");
  });

  server.on("/states", HTTP_POST, []() {
    if (CURRENT_MODE == "CONFIG_MODE") {
      configTimeLimit = millis() / 1000 + 180;
      bool result = false;
      for (int i = 0; i < 10; i++) {
        String index = String(i);
        if (server.hasArg("lvl" + index) && server.hasArg("clr" + index)) {
          String lvl = server.arg("lvl" + index);
          String clr = server.arg("clr" + index);
          WriteString(lvl, 65 + (i * 32), 88 + (i * 32));
          WriteString(clr, 89 + (i * 32), 96 + (i * 32));
        }
      }
      result = EEPROM.commit();
      if (result) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
      } else
        server.send ( 400, "text/plain", "Bad request");
    } else
      server.send(403, "text/plain", "Device is in user mode now!");


  });

  server.on("/consts", HTTP_POST, []() {
    if (CURRENT_MODE == "CONFIG_MODE") {
      configTimeLimit = millis() / 1000 + 180;
      bool result = false;
      if (server.hasArg("appass")) {
        String appass = server.arg("appass");
        WriteString(appass, 385, 401);
      }
      if (server.hasArg("chstint")) {
        byte chstint = (byte)server.arg("chstint").toInt();
        EEPROM.write(403, chstint);
      }
      if (server.hasArg("checkint")) {
        unsigned short checkint = server.arg("checkint").toInt();
        WriteShort(checkint, 404, 405);
      }
      if (server.hasArg("cfrsint")) {
        byte cfrsint = (byte)server.arg("cfrsint").toInt();
        if (cfrsint >= 0 && cfrsint <= 3)
          EEPROM.write(406, cfrsint);
      }
      if (server.hasArg("rltm")) {
        unsigned short rltm = server.arg("rltm").toInt();
        WriteShort(rltm, 407, 408);
      }
      if (server.hasArg("dbg")) {
        byte dbg = (byte)server.arg("dbg").toInt();
        Serial.print("dbg here: ");
        Serial.println(dbg);
        if (dbg >= 0 && dbg <= 1)
          EEPROM.write(409, dbg);
      }
      result = EEPROM.commit();
      if (result) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
      } else
        server.send ( 400, "text/plain", "Bad request");
    } else
      server.send(403, "text/plain", "Device is in user mode now!");
  });

  server.on("/clear", HTTP_POST, []() {
    Serial.println("CLEANING EEPROM!");
    //Disabled until confirmation added
    if (CURRENT_MODE == "DISABLED!") {
      configTimeLimit = millis() / 1000 + 180;
      bool result = CleanEEPROM(0, 511);
      if (result) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
      } else
        server.send ( 400, "text/plain", "Bad request");
    } else
      server.send(403, "text/plain", "Device is in user mode now!");
  });

  server.on("/usermode", HTTP_POST, []() {
    if (CURRENT_MODE == "CONFIG_MODE") {
      Serial.println("start");
      configTimeLimit = 0;
      Serial.println("success?");
      server.send ( 200, "text/plain", "Changing mode...");
    } else
      server.send(403, "text/plain", "Device is in user mode now!");
  });
}

String Layout(String MODE) {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  if (MODE == "USER_MODE" && reloadTimer > 0)
    html += "<meta http-equiv='refresh' content='" + String(reloadTimer) + "'; charset=utf-8'/>";
  html += "<link rel='shortcut icon' href=''>";
  html += "<title>Coffee now!</title>";
  html += Style(MODE);
  html += "</head>";
  html += "<body>";
  html += "<header>";
  html += "<h1>Coffee Now!</h1>";
  html += "</header>";
  html += "<div class=row>";
  html += "<div class=left></div>";
  html += "<div class='content'>";
  if (MODE == "USER_MODE")
    html += Jug();
  else if (MODE == "CONFIG_MODE")
    html += Forms();
  html += "</div>";
  html += "<div class=right>";
  if (MODE == "USER_MODE")
    html += Indicators();
  else if (MODE == "CONFIG_MODE")
    html += Info();
  html += "</div>";
  html += "</div>";
  html += "<footer>";
  html += "<p>Copyright &copy; 2018 Vladislav Vishnevskii</p>";
  html += "</footer>";
  if (MODE == "USER_MODE") {
    if (coffeeBrewedSeconds != NULL) {
      if (millis()/1000-coffeeBrewedSeconds < reloadTimer)
        html += "<script>window.onload = function(){alert('New Coffee has been created recently!');}</script>";
    }
  } else if (MODE == "CONFIG_MODE") {
    html += "<script type='text/javascript'>";
    html += "window.addEventListener('keydown',function(e){if(e.keyIdentifier=='U+000A'||e.keyIdentifier=='Enter'||e.keyCode==13){if(e.target.nodeName=='INPUT'||e.target.nodeName=='SELECT'){e.preventDefault();return false;}}},true);";
    html += "</script>";
  }
  html += "</body>";
  html += "</html>";
  return html;
}

String Style(String MODE) {
  String html = "<style type=text/css>";
  html += "html{height: 100%; box-sizing: border-box;}";
  html += "body{position: relative; min-height: 100%; margin: 0 0 0 0; padding-bottom: 90px; box-sizing: border-box;}";
  html += "header{background-color: #FFD293; text-align: center; min-height: 70px; display: inline-block; width: 100%; padding-top: 30px;}";
  html += "header h1{float:none; margin: auto;}";
  html += "div.row{display: table; width:100%; border-spacing: 10px; white-space: nowrap;}";
  html += "div.content{display: table-cell; width:50%; text-align: center; margin-top: 20px; margin-bottom: 20px; white-space: nowrap;}";
  html += "form {margin-bottom:25px; width: 100%;}";
  html += "form input[type='submit']{width:120px; margin:5px;}";
  html += "form.networkform input[type='text']{width:150px; margin:5px;}";
  html += "form.addressform input[type='number']{width:100px; margin:5px;}";
  html += "form.sensorform input[type='number']{width:40px; margin:5px;}";
  html += "form.statesform input[type='text']{width:80px; margin:5px;}";
  html += "form.constsform input[type='text']:not(.password){width:80px; margin:5px;}";
  html += "form.constsform input[type='number']:not(.password){width:80px; margin:5px;}";
  html += "input.password {width:150px; margin:5px;}";
  html += "table{margin: 0px auto;}";
  html += "div.left{display: table-cell; width:25%; padding: 20px;}";
  html += "div.right{display: table-cell; width:25%; max-width:30%; padding: 20px; background: rgba(150, 150, 150, 0.2); box-shadow: 5px 5px 5px rgba(0,0,0,0.15); white-space:normal;}";
  html += "div.right p{margin-left:20px;}";
  html += "div.right span{ margin-left:-15px;}";
  html += "span.warn{background-color: yellow; clip-path: polygon(50% 10%, -5% 83%, 105% 83%);}";
  html += "span.error{background-color: red; clip-path: polygon(50% 10%, -5% 83%, 105% 83%);}";
  html += "span.OK{background-color: green; clip-path: polygon(10% 18%, 92% 18%, 92% 80%, 10% 80%);}";
  html += "span.Attention{background-color: yellow; clip-path: polygon(10% 18%, 92% 18%, 92% 80%, 10% 80%);}";
  html += "span.Problem{background-color: red; clip-path: polygon(10% 18%, 92% 18%, 92% 80%, 10% 80%);}";
  html += "a.button{ text-decoration: none; background-color: #966C42; font-weight: 500; font-size:20px; color: black; margin-top:40px; padding: 15px 40px 15px 40px; border-top: 1px solid #CCCCCC; border-right: 1px solid #4B3621; border-bottom: 1px solid #4B3621;border-left: 1px solid #CCCCCC; border-radius: 5px;}";
  html += ".disabled{opacity: 0.70; cursor: not-allowed;}";
  html += "footer{background-color: #A66813; text-align: center; width: 100%; height: 70px; position:absolute; bottom:0;}";
  if (MODE == "USER_MODE") {
    html += "div.jug{background-image: url('" + currentState.getLink() + "'); max-width:300px; max-height:300px; background-size:cover;margin:auto;}";
    html += "div.missing{opacity: 0.3;}";
    html += "h2.sign{color:" + currentState.getColor() + "; font-size:34px; text-align: center; vertical-align: middle; line-height: 335px;}";
  }
  html += "</style>";
  return html;
}

String Jug() {
  String html = "";
  if (coffeeBrewedSeconds != NULL){
    int hours = (int)floor((millis()/1000 - coffeeBrewedSeconds)/3600);
    int minutes = (int)floor((millis()/1000 - coffeeBrewedSeconds)%3600/60);
    String sMinutes, sSeconds;
    Serial.println(minutes);
    if (minutes<10)
    sMinutes="0"+String(minutes);
    else
    sMinutes=String(minutes);
    
    int seconds = (int)((millis()/1000 - coffeeBrewedSeconds)%3600%60);
    if (seconds<10) 
      sSeconds="0"+String(seconds);
    else
      sSeconds=String(seconds);
    Serial.println(minutes);
    Serial.println(seconds);
    Serial.println(sMinutes);
    Serial.println(sSeconds);
    html += "<h2>Coffee has been brewed " + String(hours) + ":" +sMinutes + ":"+sSeconds+" ago</h2>";
  }else
    html += "<h2>Coffee brewed at unknown time</h2>";
  html += "<div class='jug ";
  if (currentState.getId() == 9999)
    html += "missing";
  html += "'>";
  html += "<h2 class='sign'>" + currentState.getSign() + "</h2>";
  html += "</div>";
  html += "<a class='button disabled' href='#'>BREW MORE COFFEE</a>";
  return html;
}

String Indicators() {
  String html = "<p class='disabled'><span class ='OK'>&#9744;</span> Water</p>";
  html += "<p class='disabled'><span class ='Problem'>&#9744;</span> Coffee Beans <i>(No beans in the box)</i></p>";
  html += "<p class='disabled'><span class ='Attention'>&#9744;</span> Filter <i>(2 days until change)</i></p>";
  if (Blackout) {
    html += "<p><span class='warn'>&#9888;</span>Warning! Blackout detected.</p>";
  }
  if (sensorWarning != "") {
    html += "<p><span class='warn'>&#9888;</span>Warning! Sensor error detected:" +
    sensorWarning + "</p>";
  }
  if (errorMessage != "") {
    html += "<p><span class='error'>&#9888;</span>Warning! Errors detected:" +
    errorMessage + "</p>";
  }
  if (debug == true) {
    html += "<p><span class='warn'>&#9888;</span>Warning debug enabled! Sensor outputs:" +
    sensorOutput + "</p>";
  }
  return html;
}

String Forms() {
  //Get data
  String ssid = ReadString(0, 15);
  String pass = ReadString(16, 31);
  String ipoct0, ipoct1, ipoct2, ipoct3, mkoct0, mkoct1, mkoct2, mkoct3, mkoct4, gtoct0, gtoct1, gtoct2, gtoct3;
  ipoct0 = String(EEPROM.read(32));
  ipoct1 = String(EEPROM.read(33));
  ipoct2 = String(EEPROM.read(34));
  ipoct3 = String(EEPROM.read(35));
  mkoct0 = String(EEPROM.read(36));
  mkoct1 = String(EEPROM.read(37));
  mkoct2 = String(EEPROM.read(38));
  mkoct3 = String(EEPROM.read(39));
  gtoct0 = String(EEPROM.read(40));
  gtoct1 = String(EEPROM.read(41));
  gtoct2 = String(EEPROM.read(42));
  gtoct3 = String(EEPROM.read(43));
  String appass = ReadString(385, 401);
  byte isstatic = EEPROM.read(402);
  if (isstatic > 1)
    isstatic = 0;
  byte chstint = EEPROM.read(403);
  unsigned short checkint = ReadShort(404, 405);
  byte cfrsint = EEPROM.read(406);
  if (cfrsint > 4)
    cfrsint = 2;
  unsigned short rltm = ReadShort(407, 408);
  byte dbg = EEPROM.read(409);
  if (dbg > 1)
    dbg = 0;
  byte syncat = EEPROM.read(412);
  byte accurch = EEPROM.read(413);
  delay(200);

  String html = "<form action='/network' method='POST' class='networkform'>";
  html += "<table>";
  html += "<tr><td>SSID:</td><td colspan='2'><input type='text' name='ssid' value='" + ssid + "' maxlength='16'></td><td>Password:</td><td colspan='2'><input type='text' name='pass' value='" + pass + "' maxlength='16'></td></tr>";
  html += "<tr><td colspan='6'><input type='submit' value='Apply'></td></tr>";
  html += "</table>";
  html += "</form>";
  html += "<form action='/address' method='POST' class='addressform'>";
  html += "<table>";
  html += "<tr><td colspan='1'></td><td colspan='2'><input type='radio' name='static' value='1'";
  if (isstatic == 1)
    html += "checked";
  html += ">Static IP</td><td colspan='2'><input type='radio' name='static' value='0'";
  if (isstatic == 0)
    html += "checked";
  html += ">Dynamic IP</td></tr>";
  html += "<tr><td>IP address:</td><td colspan='4'><input type='number' name='ipoct0' value='" + ipoct0 + "' min='0' max='255'><input type='number' name='ipoct1' value='" + ipoct1 + "' min='0' max='255'><input type='number' name='ipoct2' value='" + ipoct2 + "' min='0' max='255'><input type='number' name='ipoct3' value='" + ipoct3 + "' min='0' max='255'></td></tr>";
  html += "<tr><td>Mask:</td><td colspan='4'><input type='number' name='mkoct0' value='" + mkoct0 + "' min='0' max='255'><input type='number' name='mkoct1' value='" + mkoct1 + "' min='0' max='255'><input type='number' name='mkoct2' value='" + mkoct2 + "' min='0' max='255'><input type='number' name='mkoct3' value='" + mkoct3 + "' min='0' max='255'></td></tr>";
  html += "<tr><td>Gateway:</td><td colspan='4'><input type='number' name='gtoct0' value='" + gtoct0 + "' min='0' max='255'><input type='number' name='gtoct1' value='" + gtoct1 + "' min='0' max='255'><input type='number' name='gtoct2' value='" + gtoct2 + "' min='0' max='255'><input type='number' name='gtoct3' value='" + gtoct3 + "' min='0' max='255'></td></tr>";
  html += "<tr><td colspan='5'><input type='submit' value='Apply'></td></tr>";
  html += "</table>";
  html += " </form>";
  html += SensorsForm();
  html += StatesForm();
  html += "<form action='/consts' method='POST' class='constsform'>";
  html += "<table>";
  html += "<tr><td colspan='3'>Configuration AP password</td><td colspan='3'><input class='password' type='text' name='appass' value='" + appass + "' maxlength='16'></td></tr>";
  html += "<tr><td colspan='2'>Change state interval</td><td><input type='number' name='chstint' value='" + String(chstint) + "' min='0' max='255'></td><td colspan='2'>Check interval</td><td><input type='number' name='checkint' value='" + String(checkint) + "' min='0' max='65535'></td></tr>";
  html += "<tr><td colspan='2'>Coffee rises interval</td><td><input type='number' name='cfrsint' value='" + String(cfrsint) + "' min='0' max='3'></td><td colspan='2'>Reload Time </td><td><input type='number' name='rltm' value='" + String(rltm) + "' min='0' max='65535'></td></tr>";
  html += "<tr><td colspan='3'>Debug mode: <input type='radio' name='dbg' value='1'";
  if (dbg == 1)
    html += "checked";
  html += ">enable<input type='radio' name='dbg' value='0'";
  if (dbg == 0)
    html += "checked";
  html += ">disable</td><td colspan='3'></td></tr>";
  html += "<td colspan='6'><input type='submit' value='Apply'></td></tr>";
  html += "</table>";
  html += "</form>";
  html += "<form action='/clear' method='POST' class='clearform' disabled>";
  html += "<input type='submit' value='Clear EEPROM'>";
  html += "</form>";
  html += "<form action='/usermode' method='POST' class='clearform'>";
  html += "<input type='submit' value='To user mode'>";
  html += "</form>";
  return html;
}

String SensorsForm() {
  byte sensnum = EEPROM.read(44);
  if (sensnum > 4)
    sensnum = 4;
  String html = "<form action='/sensors' method='POST' class='sensorsform'>";
  html += "<table>";
  for (int i = 0; i < sensnum; i++) {
    String inx = String(i);
    String sda = String(EEPROM.read(5 * i + 45));
    String scl = String(EEPROM.read(5 * i + 46));
    String thr = String(EEPROM.read(5 * i + 47));
    String rng = String(EEPROM.read(5 * i + 48));
    String maxi = String(EEPROM.read(5 * i + 49));
    html += "<tr><td>Sensor #" + inx + " - </td><td>SDA:<input type='number' name='sda" + inx + "' value='" + sda + "' min='1' max='16'></td><td>SCL:<input type='number' name='scl" + inx + "' value='" + scl + "' min='1' max='16'></td>";
    html += "<td>Threshold:<input type='number' name='thr" + inx + "' value='" + thr + "' min='1' max='255'></td><td>Range:<input type='number' name='rng" + inx + "' value='" + rng + "' min='0' max='255'></td><td>Max:<input type='number' name='max" + inx + "' value='" + maxi + "' min='0' max='255'></td></tr>";
  }
  html += "<tr><td>Amount:</td><td><select name='sensnum'>";
  for (int i = 4; i > 0; i--) {
    html += "<option value=" + String(i) + "";
    if (sensnum == i)
      html += " selected";
    html += ">" + String(i) + "</option>";
  }
  html += "</select></td>";
  html += "<td colspan='2'><input type='submit' value='Apply'></td><td colspan='2'></td></tr>";
  html += "</table>";
  html += "</form>";
  return html;
}

String StatesForm() {
  String html = "<form action='/states' method='POST' class='statesform'>";
  html += "<table>";
  for (int i = 0; i < 10; i++) {
    if (i % 2 == 0) {
      int id = i / 2;
      String sign = ReadString(65 + 32 * id, 88 + 32 * id);
      String color = ReadString(89 + 32 * id, 96 + 32 * id);
      html += "<tr><td>State " + StateName(id) + " - </td><td><input type='text' name='lvl" + String(id) + "' value='" + sign + "' maxlength='24'></td><td>color:<select name='clr" + String(id) + "'><option value='black'";
      if (color == "black")
        html += " selected";
      html += "> black </option> <option value = 'white'";
      if (color == "white")
        html += " selected";
      html += ">white </option> </select> </td > ";
    }
    else {
      int id = 5 + (i - 1) / 2;
      String sign = ReadString(65 + 32 * id, 88 + 32 * id);
      String color = ReadString(89 + 32 * id, 96 + 32 * id);
      html += "<td>State " + StateName(id) + " - </td><td><input type = 'text' name = 'lvl" + String(id) + "' value = '" + sign + "' maxlength = '24'> </td><td>color: <select name = 'clr" + String(id) + "'><option value = 'black'";
      if (color == "black")
        html += " selected";
      html += "> black </option> <option value = 'white'";
      if (color == "white")
        html += " selected";
      html += ">white </option > </select> </td></tr> ";
    }
  }
  html += "<tr><td colspan = '6'><input type = 'submit' value = 'Apply'> </td> </tr> ";
  html += "</table>";
  html += "</form>";
  return html;
}

String StateName(int id) {
  String result = "";
  char dig[4];
  if (id == 9)
    result = "9999";
  else {
    for (int i = 3; i >= 0; i--) {
      if (id >= 2) {
        dig[i] = "2"[0];
        id = id - 2;
      }
      else {
        dig[i] = String(id)[0];
        id = 0;
      }
    }
    result = String(dig[0]) + String(dig[1]) + String(dig[2]) + String(dig[3]);
  }

  return result;
}

String Info() {
  String html = "<p>IP settings allows to make the device use apropiate network address while in user mode, so you can easily access it.<br><br><br></p>";
  html += "<p>Sensor settings allows to calibrate sensors to detect coffee better. You can ajust parameters, to suit light level of the room.<br> Theshold - amount of light passing to the sensor when the coffee on the sensor's level.<br> Range - the range of light levels which considered to mean that the coffee is on the sensor's level.<br> Max - the light level, above which the readings mean the absence of the jug.</p>";
  html += "<p>If the light level is above Threshold + Range the result is 0 (no coffee), if less then Threshold - Range is 2 (coffee is above).</p>";
  html += "<p> State settings allows to set the sign that appears on top of the coffee jug picture and a color of this sign</p>";
  html += "<p> Config settings allows to set various constants that influence the device behavior. Change State interval - the amount of repeating readings needed to deem the state stable. Check interval represents the amont of loops with only hande client between the reading processing. Coffee rises interval defines how much times the coffee level should rise before the device will decide that new coffee has been brewed. Reload time set the inteval between automatic page reloads. Dubug mode will put logs into the UI and on the port.</p>";
  return html;
}
