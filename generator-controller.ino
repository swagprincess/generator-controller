const char* ssid = "";
const char* password = "";
const char* hostname = "generator-controller";

IPAddress local_IP(192, 168, 50, 5);  
IPAddress gateway(192, 168, 50, 1);   
IPAddress subnet(255, 255, 255, 0);   

#include "WebServer.h"
#include "esp_wifi.h"

String index_html;
String message;
bool recdata = false;
uint16_t crcfinal;
int count = 0;
int ind = 0;
char tempchar[8];
char response[256];
char responsechar;
unsigned long previousMillis = 0;
unsigned long previousMillisSerial = 0;
unsigned long previousWifiMillis = 0;
unsigned long previousGenMillisOn = 0;
unsigned long previousGenMillisOff = 0;

uint16_t crc;

uint8_t da;
uint8_t* ptr;
uint8_t bCRCHign;
uint8_t bCRCLow;

uint16_t crc_ta[16] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef
};


bool genforceon = false;
bool genforceoff = false;

bool setgenmillisoff = false;
bool setgenmillison = false;
bool generatorwanted = false;
bool generatordetected = false;
bool generatorseton = false;
bool generatorsetoff = false;

float genvoltage;

float acvoltage;
float acfrequency;
float battvoltage;


int acpower;
int battchrgcurrent;
int battchrgcurrentpvestimate;
int battchrgcurrentgridestimate;
int battmaxchrgcurrentgrid;
int battcapacity;
int battdischrgcurrent;
int battcurrent;
int pvpower1;
int pvpower2;
int pvpower;

WebServer server(100);


void sendpage(){
  server.send(200, "text/html", index_html.c_str());
}

void forcegenon(){
  server.send(200);
  genforceon = true;
  genforceoff = false;
}

void forcegenoff(){
  server.send(200);
  genforceon = false;
  genforceoff = true;
}

void noforcegen(){
  server.send(200);
  genforceon = false;
  genforceoff = false;
}

void setup() {
  
  pinMode(6, OUTPUT);

  Serial.begin(9600);
  Serial1.begin(2400, SERIAL_8N1, 17, 18);

  server.on("/", sendpage);
  server.on("/forcegenon", forcegenon);
  server.on("/forcegenoff", forcegenoff);
  server.on("/noforcegen", noforcegen);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps (WIFI_PS_NONE);
  WiFi.config(local_IP, gateway, subnet);  //comment out for DHCP
  WiFi.setHostname(hostname);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(WiFi.localIP());

  server.begin();

  previousMillis = millis();
}


void loop() {

  if (millis() - previousMillis >= 10000) {
    if (update()) {
      recdata = true;
    } else {
      recdata = false;
      Serial.println("inverter comms fail");
    }
    previousMillis = millis();
  }

  if (recdata) {

    index_html = "<!DOCTYPE HTML><html><head><meta http-equiv=\"refresh\" content=\"10\" ></head><body style=\"font-size:18px;\">\
    Output Voltage: " + String(acvoltage) + "V<br>Output Frequency: " + String(acfrequency) + "Hz<br>Active Power: " + String(acpower) + "W<br>PV Power: " + String(pvpower) + "W<br>Battery Voltage: " + String(battvoltage) + "V<br>Battery Capacity: " + String(battcapacity) + "%<br>Battery Current: " + String(battcurrent) + "A (" + String(battchrgcurrentpvestimate) + "A PV, " + String(battchrgcurrentgridestimate) + "A Grid)<br>Generator Wanted: " + String(generatorwanted) + "<br>Generator Detected: " + String(generatordetected) + "<br><br>Generator Forced On: " + String(genforceon) + "<br>Generator Forced Off: " + String(genforceoff) + "</body></html>";
    // im sorry
    if (!checkgenerator()){
      Serial.println("checkgenerator fail");
    }
    recdata = false;
  }

  if (WiFi.status() != WL_CONNECTED && millis() - previousWifiMillis >= 10000) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousWifiMillis = millis();
  }

  server.handleClient();

  delay(1);

}

int checkgenerator() {

  if (((battcapacity <= 20) || (battvoltage <= 47)) && (!generatorwanted) && (!genforceoff)) {
    if (!setgenmillison){
      previousGenMillisOn = millis();
      setgenmillison = true;
    }
    if (millis() - previousGenMillisOn >= 60000){
        generatorwanted = true;
        setgenmillison = false;
    }
  } else {
    setgenmillison = false;
  }
  
  if (((battcapacity >= 25) || ((battcapacity >= 22) && (pvpower - acpower >= -50))) && (generatorwanted) && (!genforceon)) {
    if (!setgenmillisoff){
      previousGenMillisOff = millis();
      setgenmillisoff = false;
    }
    if (millis() - previousGenMillisOff >= 60000){
      generatorwanted = false;
      setgenmillisoff = false;
    }
  } else {
    setgenmillisoff = false;
  }


  if ((generatorwanted) || (genforceon)) {
    if ((!generatorseton) && (!genforceoff)){
      
      delay(5000);

      message = "PBDV53.0";

      if (query() == 0) {
        return 0;
      }
       
      delay(5000);

      message = "PBCV51.0";

      if (query() == 0) {
        return 0;
      }

      delay(10000);

      digitalWrite(6, HIGH);
      
      generatorseton = true;
      generatorsetoff = false;

      previousMillis = millis();

    }
  }

  if ((!generatorwanted) || (genforceoff)) {
    if ((!generatorsetoff) && (!genforceon)){

      delay(5000);
      message = "PBCV47.0";
      if (query() == 0) {
       return 0;
      }

      delay(5000);
  
      message = "PBDV48.0";


      if (query() == 0) {
        return 0;
      }

      delay(10000);
    
      digitalWrite(6, LOW);

      generatorsetoff = true;
      generatorseton = false;

      previousMillis = millis();

    }
  }
return 1;
}

int update() {

  message = "QPIGS";

  if (query() == 0) {
    return 0;
  }


  memcpy(tempchar, &response[1], 5);
  tempchar[5] = '\0';
  genvoltage = strtof(tempchar, NULL);

  memcpy(tempchar, &response[12], 16);
  tempchar[5] = '\0';
  acvoltage = strtof(tempchar, NULL);

  memcpy(tempchar, &response[18], 21);
  tempchar[4] = '\0';
  acfrequency = strtof(tempchar, NULL);

  memcpy(tempchar, &response[28], 31);
  tempchar[4] = '\0';
  acpower = strtol(tempchar, NULL, 10);

  memcpy(tempchar, &response[41], 45);
  tempchar[5] = '\0';
  battvoltage = strtof(tempchar, NULL);

  memcpy(tempchar, &response[47], 49);
  tempchar[3] = '\0';
  battchrgcurrent = strtol(tempchar, NULL, 10);

  memcpy(tempchar, &response[51], 53);
  tempchar[3] = '\0';
  battcapacity = strtol(tempchar, NULL, 10);

  memcpy(tempchar, &response[80], 84);
  tempchar[5] = '\0';
  battdischrgcurrent = strtol(tempchar, NULL, 10);

  memcpy(tempchar, &response[99], 104);
  tempchar[5] = '\0';
  pvpower1 = strtol(tempchar, NULL, 10);



  message = "QPIGS2";

  if (query() == 0) {
    return 0;
  }

  memcpy(tempchar, &response[12], 16);
  tempchar[5] = '\0';
  pvpower2 = strtol(tempchar, NULL, 10);


  battcurrent = battchrgcurrent - battdischrgcurrent;

  pvpower = pvpower1 + pvpower2;

  if (genvoltage >= 180.00) {
    generatordetected = 1;
  } else {
    generatordetected = 0;
  }

  if (generatordetected == 0) {
    battchrgcurrentpvestimate = battchrgcurrent;
    battchrgcurrentgridestimate = 0;
  }

  if (generatordetected == 1) {
    battchrgcurrentpvestimate = pvpower / battvoltage;          
    battchrgcurrentgridestimate = battchrgcurrent - battchrgcurrentpvestimate; 
  }


  return 1;
}

int query() {

  sendmessage();
  previousMillisSerial = millis();
  while (!Serial1.available()) {  
    if (millis() - previousMillisSerial >= 5000) {
      return 0;
    }
  }
  responsechar = '\0';
  ind = 0;
  previousMillisSerial = millis();
  while (responsechar != '\r') {
    if (Serial1.available()){
      responsechar = Serial1.read();  
      if (responsechar != '\r') {   
        response[ind] = responsechar;
      } else {
        response[ind] = '\0';
      }
      ind++;
    }
    if (millis() - previousMillisSerial >= 3000){
      return 0;
    }
  }

  if (!(CheckCRC((unsigned char*)(&response), ind))){
    return 0;
  }

  return 1;
}


void sendmessage() {
  crcfinal = cal_crc_half((uint8_t*)message.c_str(), strlen(message.c_str()));
  Serial1.print(message);
  Serial1.write(crcfinal >> 8);
  Serial1.write(crcfinal & 0xff);
  Serial1.print("\r");  //1
}



uint16_t cal_crc_half(uint8_t* pin, uint8_t len) {


  ptr = pin;
  crc = 0;

  while (len-- != 0) {
    da = ((uint8_t)(crc >> 8)) >> 4;
    crc <<= 4;
    crc ^= crc_ta[da ^ (*ptr >> 4)];
    da = ((uint8_t)(crc >> 8)) >> 4;
    crc <<= 4;
    crc ^= crc_ta[da ^ (*ptr & 0x0f)];
    ptr++;
  }

  bCRCLow = crc;
  bCRCHign = (uint8_t)(crc >> 8);

  if (bCRCLow == 0x28 || bCRCLow == 0x0d || bCRCLow == 0x0a)
    bCRCLow++;
  if (bCRCHign == 0x28 || bCRCHign == 0x0d || bCRCHign == 0x0a)
    bCRCHign++;
  crc = ((uint16_t)bCRCHign) << 8;
  crc += bCRCLow;
  return (crc);
}

bool CheckCRC(unsigned char *data, int len) {
  uint16_t crc = cal_crc_half(data, len-3);
  return data[len-3]==(crc>>8) && data[len-2]==(crc&0xff);
}

