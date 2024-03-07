const char* ssid = "";
const char* password = "";
const char* hostname = "generator-controller";

IPAddress local_IP(192, 168, 50, 5);  
IPAddress gateway(192, 168, 50, 1);   
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 50, 1);

#include "WiFi.h"
#include "esp_wifi.h"
#include "Esp.h"

char header[512];
bool recdata = false;
uint16_t crcfinal;
int count = 0;
int ind = 0;
char tempchar[20];
char response[512];
char responsechar;
unsigned long previousMillis = 0;
unsigned long previousMillisSerial = 0;
unsigned long previousWifiMillis = 0;
unsigned long previousGenMillisOn = 0;
unsigned long previousGenMillisOff = 0;

unsigned long genOnMillis = 0;
unsigned long totalGenOnHours = 0;


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

int heap;


WiFiServer server(100);


void setup() {
  
  pinMode(6, OUTPUT);

  Serial.begin(9600);
  Serial1.begin(2400, SERIAL_8N1, 17, 18);

  WiFi.setHostname(hostname);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);  //comment out for DHCP
  //esp_wifi_set_ps (WIFI_PS_NONE);
  WiFi.mode(WIFI_STA);
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

    if (!checkgenerator()){
      Serial.println("checkgenerator fail");
    }
    recdata = false;
  }

  if (((WiFi.status() != WL_CONNECTED) || (WiFi.status() == 0) || (WiFi.status() == 5)) && millis() - previousWifiMillis >= 15000) {
    Serial.println("reconnecting to wifi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    previousWifiMillis = millis();
  }else{
    WiFiClient client = server.available();
    if(client){

      heap = ESP.getFreeHeap();

      ind = 0;
      while(client.available()) {
        responsechar = client.read();
        header[ind] = responsechar;
        ind++;
      }
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/html");
      client.println("Connection: close");
      client.println();
      client.print("<!DOCTYPE HTML><html><head><meta http-equiv=\"refresh\" content=\"10\" ></head><body style=\"font-size:18px;\">Output Voltage: ");
      client.print(acvoltage);
      client.print(" V<br>Output Frequency: ");
      client.print(acfrequency);
      client.print(" Hz<br>Active Power: ");
      client.print(acpower);
      client.print(" W<br>PV Power: ");
      client.print(pvpower);
      client.print(" W<br>Battery Voltage: ");
      client.print(battvoltage);
      client.print(" V<br>Battery Capacity: ");
      client.print(battcapacity);
      client.print(" %<br>Battery Current: ");
      client.print(battcurrent);
      client.print(" A");
      client.print("<br>Generator Wanted: ");
      client.print(generatorwanted);
      client.print("<br>Generator Detected: ");
      client.print(generatordetected);
      client.print("<br>Generator On Time: ");
      client.print(roundf(totalGenOnHours * 100) / 100);
      client.print(" H<br><br>Generator Forced On: ");
      client.print(genforceon);
      client.print("<br>Generator Forced Off: ");
      client.print(genforceoff);
      client.print("<br><br>Free heap: ");
      client.print(heap);
      client.print("<br>Uptime: ");
      client.print(roundf(esp_timer_get_time() / 3600000000.0 * 100) / 100);
      client.print("H</body></html>");
      client.println("");
      client.stop();

      if (strstr(header, "forcegenon") != NULL){
        genforceon = true;
        genforceoff = false;
      }

      if (strstr(header, "forcegenoff") != NULL){
        genforceon = false;
        genforceoff = true;
      }

      if (strstr(header, "noforcegen") != NULL){
        genforceon = false;
        genforceoff = false;
      }

      memset(header, 0, sizeof(header));

    }
  }

  delay(50);

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
  
  if (((generatorwanted) && (!genforceon)) && (millis() - genOnMillis >= 1200000)) {
    if (!setgenmillisoff){
      previousGenMillisOff = millis();
      setgenmillisoff = true;
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

      if (query("PBDV53.0") == 0) {
        return 0;
      }
       
      delay(5000);

      if (query("PBCV51.0") == 0) {
        return 0;
      }

      delay(10000);

      digitalWrite(6, HIGH);
      
      generatorseton = true;
      generatorsetoff = false;

      previousMillis = millis();

      genOnMillis = millis();

    }
  }

  if ((!generatorwanted) || (genforceoff)) {
    if ((!generatorsetoff) && (!genforceon)){

      delay(5000);

      if (query("PBCV47.0") == 0) {
       return 0;
      }

      delay(5000);


      if (query("PBDV48.0") == 0) {
        return 0;
      }

      delay(10000);
    
      digitalWrite(6, LOW);

      generatorsetoff = true;
      generatorseton = false;

      previousMillis = millis();

      totalGenOnHours = (totalGenOnHours + (genOnMillis / 3600000));

      genOnMillis = 0;

    }
  }
return 1;
}

int update() {

  if (query("QPIGS") == 0) {
    return 0;
  }


  memcpy(tempchar, response+1, 5);
  tempchar[5] = '\0';
  genvoltage = strtof(tempchar, NULL);

  memcpy(tempchar, response+12, 5);
  tempchar[5] = '\0';
  acvoltage = strtof(tempchar, NULL);

  memcpy(tempchar, response+18, 4);
  tempchar[4] = '\0';
  acfrequency = strtof(tempchar, NULL);

  memcpy(tempchar, response+28, 4);
  tempchar[4] = '\0';
  acpower = strtol(tempchar, NULL, 10);

  memcpy(tempchar, response+41, 5);
  tempchar[5] = '\0';
  battvoltage = strtof(tempchar, NULL);

  memcpy(tempchar, response+47, 3);
  tempchar[3] = '\0';
  battchrgcurrent = strtol(tempchar, NULL, 10);

  memcpy(tempchar, response+51, 3);
  tempchar[3] = '\0';
  battcapacity = strtol(tempchar, NULL, 10);

  memcpy(tempchar, response+80, 5);
  tempchar[5] = '\0';
  battdischrgcurrent = strtol(tempchar, NULL, 10);

  memcpy(tempchar, response+99, 5);
  tempchar[5] = '\0';
  pvpower1 = strtol(tempchar, NULL, 10);


  if (query("QPIGS2") == 0) {
    return 0;
  }

  memcpy(tempchar, response+12, 5);
  tempchar[5] = '\0';
  pvpower2 = strtol(tempchar, NULL, 10);


  battcurrent = battchrgcurrent - battdischrgcurrent;

  pvpower = pvpower1 + pvpower2;

  if (genvoltage >= 180.00) {
    generatordetected = 1;
  } else {
    generatordetected = 0;
  }



  return 1;
}

int query(char *message) {

  crcfinal = cal_crc_half((uint8_t*)message, strlen(message));
  Serial1.print(message);
  Serial1.write(crcfinal >> 8);
  Serial1.write(crcfinal & 0xff);
  Serial1.print("\r");  //1

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

