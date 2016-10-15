#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <SD.h>
#include <SPI.h>

#include "util.h";
#include "ublox.h";

SoftwareSerial GPS=SoftwareSerial(D1, D2);
Ublox ublox=Ublox(&GPS);
TinyGPSPlus nmea;

char secret[33];

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println("Debugging started");

  // Input for user button
  pinMode(D3, INPUT_PULLUP);

  // Softserial pins for GPS module
  pinMode(D1, INPUT);
  pinMode(D2, OUTPUT);
  
  // LED on the ESP-12e
  pinMode(D4, OUTPUT);

  // Start Softserial for GPS
  GPS.begin(9600);
  GPS.enableRx(true);
  Serial.println("SoftSerial started");
  Serial.println("Sending gps settings");
  ublox.setupGPS();
  Serial.println("Setttings done");

  // Check if button is pressed on boot
  if(!digitalRead(D3)){
    while(true){
      if(Serial.available()){
        GPS.write(Serial.read());
      }
      if(GPS.available()){
        Serial.write(GPS.read());
      }
      delay(0); 
    }
  }

  Serial.println("Starting SD");
  if(SD.begin(SS)){
    Serial.println("SD Init success");
    blink(4);
  }else{
    Serial.println("SD Init failure");
    blink(8);
    ESP.deepSleep(0);
  }

  File secretFile = SD.open("SYSTEM/SECRET.TXT");
  secret[32]= '\0';
  secretFile.read(secret, 32);
  secretFile.close();
  Serial.printf("Secret: %s\n", secret);

  SD.mkdir("system");
}

bool connect_wifi(){
  Serial.println("Connecting to wifi network");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("Scanning nearby networks");
  int scanCount = WiFi.scanNetworks();

  Serial.println("Loading possible networks");
  File wifiFile = SD.open("SYSTEM/WIFI.TXT");
  char fileSSID[40];
  char fileKEY[40];
  int charIndex = 0;
  bool inSSID = true;
  bool matchFound = false;
  while(!matchFound){
    int fileChar = wifiFile.read();
    if(fileChar < 0){
      break;
    }
    if(fileChar == '\n'){
      fileKEY[charIndex]=(char)0;
      charIndex = 0;
      inSSID = true;

      for(int i=0;i<scanCount;i++){
        if(strcmp(fileSSID, WiFi.SSID(i).c_str())==0){
          matchFound = true;
          WiFi.begin(fileSSID, fileKEY);
          break;
        }
      }
      
    }else if(fileChar == ':'){
      inSSID = false;
      fileSSID[charIndex]=(char)0;
      charIndex = 0;
    }else{
      if(inSSID){
        fileSSID[charIndex++]=(char)fileChar;
      }else{
        fileKEY[charIndex++]=(char)fileChar;
      }
    }
  }
  
  if(matchFound){
    int timeout = 2*30;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      if(--timeout == 0){
        Serial.println("Couldn't connect to the SSID");
        return false;
      }
    }
    Serial.println(WiFi.localIP());
    
  }else{
    Serial.println("No network available");
  }

  return matchFound;
}

void queueFile(const char* name){
  Serial.printf("Queueing %s\n", name);
  File queue = SD.open("system/queue.txt", FILE_WRITE);
  if(!queue){
    Serial.printf("Error opening queue file");
  }
  queue.seek(queue.size());
  queue.printf("%s\n", name);
  queue.flush();
  queue.close();
}

void uploadTrack(char* name){
  File log = SD.open(name);
  WiFiClient client;
  int tracker = ESP.getChipId();
  String trackerId = String(tracker);
  Serial.printf("Uploading %s\n", name);
  if(client.connect("tracker.brixit.nl", 80)){
    Serial.println("Sending headers");
    String length = String(log.size());
    String headers = String("POST /upload.php HTTP/1.0\r\n")+
      "Host: tracker.brixit.nl\r\n" +
      "Connection: close\r\n" + 
      "Content-Type: text/csv\r\n" + 
      "Content-Length: " + length + "\r\n" +
      "X-Tracker: " + trackerId + "\r\n" +
      "X-Secret: " + secret + "\r\n" + 
      "X-Track: " + name + "\r\n" +
      "\r\n";
    client.print(headers);
    Serial.println("Sending data");

    char buffer[1470];
    while(true){
      int bytes = log.read(buffer,1470);
      if(bytes < 1){
        break;
      }
      client.write_P(buffer, bytes);
      delay(0);
    }
    Serial.println("Upload done");
    client.stop();
  }

  
}

void uploadQueue(){
  digitalWrite(D4, HIGH);
  delay(100);
  digitalWrite(D4, LOW);
  File queue = SD.open("system/queue.txt");
  char filename[17];
  char charIndex = 0;
  Serial.println("Starting upload queue");
  while(true){
    int fileChar = queue.read();
    if(fileChar < 0){
      Serial.println("Reached EOF");
      break;
    }
    if(fileChar == '\n'){
      filename[charIndex] = (char)0;
      charIndex = 0;
      uploadTrack(filename);
    }else{
      filename[charIndex++]=(char)fileChar;
    }
  }
  queue.close();
  SD.remove("system/queue.txt");
}

void loop() {
  blink(4);
  Serial.println("Starting to parse nmea messages");
  bool locked = false;
  int lastSats = 0;
  File logfile;

  bool driving = true;
  char name[22] = {0};
  
  while(driving){
    delay(10);
    while(GPS.available() && driving){

      if(!digitalRead(D3)){
        driving=false;
      }
      int gpsChar = GPS.read();
      //Serial.write(gpsChar);
      if(nmea.encode(gpsChar) && nmea.location.isUpdated()){
          if(!locked && nmea.location.isValid() && nmea.date.isValid()){
            Serial.println("Got GPS fix");
            char directory[9]={0};
            sprintf(directory, "%d%d%d", nmea.date.year(), nmea.date.month(), nmea.date.day());
            sprintf(name, "%s/%d.log", directory, nmea.time.value());
            
            queueFile(name);
            Serial.printf("Creating directory %s\n", directory);
            SD.mkdir(directory);
            logfile = SD.open(name, FILE_WRITE);
      	    if(!logfile){
              Serial.println("Error creating logfile");
      	    }else{
              Serial.printf("Logging to %s\n", name);
      	    }
            logfile.print("date,time,latitude,longitude,speed,checksum\n");
            
            locked = true;
          }
          if(locked && !nmea.location.isValid()){
            Serial.println("Lost GPS fix");
            logfile.close();
            locked = false;
          }
        
          if(locked){
            digitalWrite(D4, LOW);
            char lat[20];
            char lng[20];
            char speed[20];
            fmtDouble(nmea.location.lat(), 10, lat);
            fmtDouble(nmea.location.lng(), 10, lng);
            fmtDouble(nmea.speed.kmph(),2, speed);
            char logline[100] = {0};
            sprintf(logline, "%d,%d,%s,%s,%s", nmea.date.value(), nmea.time.value(), lat, lng, speed);
            int checksum = calculateChecksum(logline);
            logfile.printf("%s,%d\n", logline, checksum);
            logfile.flush();
            if(!driving){
              logfile.close();
            }
                digitalWrite(D4, HIGH);

          }else{
            if(nmea.satellites.value() > lastSats){
              lastSats = nmea.satellites.value();
              Serial.printf("Got new sattelite. Now %d\n", lastSats);
            }
          }
      }
    }
  }
  GPS.enableRx(false);

  Serial.println("Track done");
  digitalWrite(D4, LOW);
  if(connect_wifi()){
    uploadQueue();
  }
  ublox.sleepyGPS();
  digitalWrite(D4, HIGH);
  ESP.deepSleep(0);
}
