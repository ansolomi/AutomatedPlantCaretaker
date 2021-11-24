#include <Arduino.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include <DHT_U.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncTCP.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <ThingSpeak.h>;
#include <NTPClient.h>
#include <WiFiUdp.h>
#define USE_SERIAL Serial

WiFiClient client;
AsyncWebServer server(80);
DNSServer dns;
AsyncWiFiManager wifiManager(&server,&dns);

const char * ssid = "AGE";
const char * password = "Alvinhana99";

//NTP Client
const long utcOffsetInSeconds = 25200;
String weekDays[7] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
}; //Buat timer Pupuk
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
String pesticideDate = "Friday";
String weekDay;

const String myWriteAPIKey = "QDJSB9R29W1D35B1";
const char * myReadAPIKey = "66S8NP99BELZBR6X";
unsigned long myChannelNumber = 1319192;
unsigned long avgTempChannelNumber = 1320586;
const char * myAvgTempReadKey= "WRT94H570D5EFPKM";
const int FieldNumber = 1;
const char * serverThingSpeak = "api.thingspeak.com";
JSONVar myObject;

unsigned long lastTime = 100;
unsigned long lastRequest = 100;
unsigned long timerDelay = 1000;
unsigned long APIRequestDelay = 300000;

String jsonBuffer;

//Moisture and shit
const int moisturePin = A0;
int moistureLevelLow;
int moistureLevelHi;
unsigned long interval = 10000;
unsigned long previousMillis = 0;
unsigned long interval1 = 1000;
unsigned long previousMillis1 = 0;
float moisturePercentage;

//DHT22
#define DHTTYPE DHT22
uint8_t DHTPin = D8;
float h;
float t;
long avgTemp;
float Temperature;
float Humidity;
DHT dht(DHTPin, DHTTYPE);

//Flow Counter
byte sensorInterrupt = 0;
bool pupukOverride;
bool weeklyCount = false;
const int buttonPin = D2;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long takaranPupuk;
int pest, pest1, pest2, pest3;
unsigned long oldTime;
float calibrationFactor = 0.955;
float flowRate;
volatile byte pulseCount;

//Solenoid Valve
uint8_t solenoidAir = D7;
uint8_t solenoidPupuk = D6;

//EEPROM Needs
int addLowThresh = 0;
int addHiThresh = 1;
int pestDose = 2;
int pestDose1 = 3;
int pestDose2 = 4;
int pestDose3 = 5;
int valueDose;
int valueDose1;
int valueDose2;
int valueDose3;

//Web Server
const char * PARAM_INPUT_1 = "input1";
const char * PARAM_INPUT_2 = "input2";
const char * PARAM_INPUT_3 = "input3";
const char * PARAM_INPUT_4 = "input4";
String inputMessage1;
String inputMessage2;
String inputMessage3;
String inputParam1;
String inputParam2;
String inputParam3;

void pulseCounter(){
  // Increment the pulse counter
  pulseCount++;
}

void yfs() {
  detachInterrupt(sensorInterrupt);
  flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
  oldTime = millis();
  flowMilliLitres = (flowRate / 60) * 1000;
  totalMilliLitres += flowMilliLitres;
  Serial.print("Ttl mili: ");
  Serial.println(totalMilliLitres);
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
  pulseCount = 0;
}

void readAvgTemperature(){
  avgTemp = ThingSpeak.readLongField(avgTempChannelNumber, FieldNumber, myAvgTempReadKey);
  Serial.print("Average Temperature: ");
  Serial.println(avgTemp);
}

String httpGETRequest(const char * serverName){
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void sendThingspeak(){
  if (client.connect(serverThingSpeak, 80)) {
    String postStr = myWriteAPIKey; // add api key in the postStr string
    postStr += "&field1=";
    postStr += String(Temperature); // add Temperature reading
    postStr += "&field2=";
    postStr += String(Humidity); // add Humidity reading
    postStr += "&field4=";
    postStr += String(moisturePercentage);  // add Moisture Percentage
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + myWriteAPIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length()); //send lenght of the string
    client.print("\n\n");
    client.print(postStr); // send complete string
    Serial.println("The following data is sent to Thingspeak.");
  }
  client.stop();
}

void sendFertilizerThingspeak(){
  if (client.connect(serverThingSpeak, 80)) {
    String postStr = myWriteAPIKey; // add api key in the postStr string
    postStr += "&field6=";
    postStr += String(totalMilliLitres); // add Temperature reading
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + myWriteAPIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length()); //send lenght of the string
    client.print("\n\n");
    client.print(postStr); // send complete string
    Serial.println("Liquid Fertilzer amount is sent to Thingspeak.");
  }
  client.stop();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
    <meta http-equiv=”refresh” content=”5″>
    <style type="text/css">
        .appName {
            text-align: center;
        }
        .content {
            margin-top: 2rem;
        }
        .form {
            width: 40%;
        }
        .wrapper{
            margin-top: 2rem;
        }
        #submitButton{
            margin-top: 4px;
        }
    </style>
    <title>Nimbus Control Panel</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.0.1/dist/css/bootstrap.min.css" rel="stylesheet"
        integrity="sha384-+0n0xVW2eSR5OomGNYDnhzAbDsOXxcvSN1TPprVMTNDbiYZCxYbOOl7+AMvyTG2x" crossorigin="anonymous">
</head>

<script>
    function submitMessage() {
    alert("Value Has Been Saved!");
    setTimeout(function(){ 
      document.location.reload(true); }, 500);   
    }</script>

<body>
    <div class="container">

        <div class="d-flex justify-content-center align-items-center flex-column">
            <h1 class="appName">Nimbus v.01 beta</h1>
            <small class="text-muted">By: Alvin Genta Pratama | 1706043090</small>
        </div>

        <div class="wrapper">
            <div class="row">
                <div class="col-sm">
                    <div class="content">
                        <form action="/get">
                            <div class="form">
                                <h5 for="lower" class="form-label">Soil Moisture Low Threshold <br> (currently %input1%) :</h5>
                                <input id="lower" class="form-control" type="number" name="input1">
                            </div>
                            <br>
                            <div class="form">
                                <h5 for="higher" class="form-label">Soil Moisture High Threshold <br> (currently %input3%) :</h5>
                                <input id="higher" class="form-control" type="number" name="input3">
                            </div>

                            <br>
                            <div class="form">
                                <h5 for="volume" class="form-label">Liquid Fertilizer Dose <br> (Currently %input4%ml) : </h5>
                                <input id="volume" class="form-control" type="number" name="input4">
                                <button id="submitButton" type="submit" class="btn btn-primary" onclick="submitMessage()">Submit</button>
                            </div>
                        </form> <br>
                    </div>
                </div>
                <div class="col-sm">
                    <span>Temperature: </span>
                    <span id="temperature">%TEMPERATURE%</span>
                    <sup class="units">&deg;C</sup>
                    <br>
                    <span>Average Temperature: </span>
                    <span id="averageTemperature">%AVGTEMPERATURE%</span>
                    <sup class="units">&deg;C</sup>
                    <br>
                    <span>Humidity: </span>
                    <span id="humidity">%HUMIDITY%</span>
                    <sup class="units">&percnt;</sup>
                    <br>
                    <div class="content">
                    <h4>Hold to Dispense Water</h4>
                    <button type="button" class="btn btn-info" onmousedown="toggleCheckbox('onAir');"
                        ontouchstart="toggleCheckbox('onAir');" onmouseup="toggleCheckbox('offAir');"
                        ontouchend="toggleCheckbox('offAir');">Dispense Water</button>
                        <br>
                    <h4>Click to Dispense Fertilizer</h4>
                    <button ctype="button" class="btn btn-success" onclick="toggleCheckbox('overridePupuk');">Dispense Dose</button>
                    <br>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <script>
        function toggleCheckbox(x) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/" + x, true);
            xhr.send();
        }

//        setInterval(function ( ) {
//        var xhttp = new XMLHttpRequest();
//        xhttp.onreadystatechange = function() {
//          if (this.readyState == 4 && this.status == 200) {
//            document.getElementById("temperature").innerHTML = this.responseText;
//          }
//        };
//        xhttp.open("GET", "/temperature", true);
//        xhttp.send();
//        }, 5000 ) ;
//      
//        setInterval(function ( ) {
//        var xhttp = new XMLHttpRequest();
//        xhttp.onreadystatechange = function() {
//          if (this.readyState == 4 && this.status == 200) {
//            document.getElementById("humidity").innerHTML = this.responseText;
//          }
//        };
//        xhttp.open("GET", "/humidity", true);
//        xhttp.send();
//      }, 5000 ) ;
//
//      setInterval(function ( ) {
//        var xhttp = new XMLHttpRequest();
//        xhttp.onreadystatechange = function() {
//          if (this.readyState == 4 && this.status == 200) {
//            document.getElementById("averageTemperature").innerHTML = this.responseText;
//          }
//        };
//        xhttp.open("GET", "/averageTemperature", true);
//        xhttp.send();
//      }, 5000 ) ;
    </script>
</body>

</html>)rawliteral";

void notFound(AsyncWebServerRequest * request) {
  request -> send(404, "text/plain", "Not found");
}

String processor(const String & var) {
  //Serial.println(var);
  if (var == "input1") {
    return String(moistureLevelLow);
  } else if (var == "input3") {
    return String(moistureLevelHi);
  } else if (var == "input4") {
    return String(takaranPupuk);
  } else if(var == "TEMPERATURE"){
    return String(Temperature);
  } else if(var == "AVGTEMPERATURE"){
    return String(avgTemp);
  } else if(var == "HUMIDITY"){
    return String(Humidity);
  }
  return String();
}

void ICACHE_RAM_ATTR pulseCounter();

void setup() {
  Serial.begin(9600);
  pinMode(buttonPin, INPUT);
  pinMode(DHTPin, INPUT);
  pinMode(solenoidAir, OUTPUT);
  pinMode(solenoidPupuk, OUTPUT);
  digitalWrite(solenoidPupuk, HIGH);
  digitalWrite(solenoidAir, HIGH);
  digitalWrite(buttonPin, HIGH);
  attachInterrupt(digitalPinToInterrupt(buttonPin), pulseCounter, RISING);

  dht.begin();
  ThingSpeak.begin(client);
  wifiManager.autoConnect("NodeMCU AP Setup", "AP-PASSWORD");
  EEPROM.begin(1024);
  timeClient.begin();
  timeClient.setTimeOffset(25200);

  //Web Server Init START
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request -> send_P(200, "text/html", index_html, processor);
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request -> send_P(200, "text/html", index_html, processor);
  });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest * request) {
    request -> send_P(200, "text/html", index_html, processor);
  });

  server.on("/averageTemperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request -> send_P(200, "text/html", index_html, processor);
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request -> hasParam(PARAM_INPUT_1)) {
      inputMessage1 = request -> getParam(PARAM_INPUT_1) -> value();
      inputParam1 = PARAM_INPUT_1;
    }  
    if (request -> hasParam(PARAM_INPUT_3)) {
      inputMessage2 = request -> getParam(PARAM_INPUT_3) -> value();
      inputParam2 = PARAM_INPUT_3;
    }
    if (request -> hasParam(PARAM_INPUT_4)) {
      inputMessage3 = request -> getParam(PARAM_INPUT_4) -> value();
      inputParam3 = PARAM_INPUT_4;
    } 
  });

  server.on("/onAir", HTTP_GET, [](AsyncWebServerRequest * request) {
    digitalWrite(solenoidAir, LOW);
    request -> send(200, "text/plain", "ok");
  });

  server.on("/offAir", HTTP_GET, [](AsyncWebServerRequest * request) {
    digitalWrite(solenoidAir, HIGH);
    request -> send(200, "text/plain", "ok");
  });
  
  server.on("/overridePupuk", HTTP_GET, [](AsyncWebServerRequest * request) {
    pupukOverride = true;
    request -> send(200, "text/plain", "ok");
  });

  server.onNotFound(notFound);
  server.begin();
  //Web Server Init END
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Temperature = dht.readTemperature(); // read Temperature
    Humidity = dht.readHumidity(); // read Humidity
    moisturePercentage = ( 100.00 - ( (analogRead(moisturePin) / 1023.00) * 100.00 ) );
    moistureLevelHi = EEPROM.read(addHiThresh);
    moistureLevelLow = EEPROM.read(addLowThresh);
    valueDose = EEPROM.read(pestDose);
    valueDose1 = EEPROM.read(pestDose1);
    valueDose2 = EEPROM.read(pestDose2);
    valueDose3 = EEPROM.read(pestDose3);
    valueDose = (valueDose * 1) + (valueDose1 * 100) + (valueDose2 * 10000) + (valueDose3 * 1000000);
    takaranPupuk = valueDose;
    timeClient.update();
    weekDay = weekDays[timeClient.getDay()];
    Serial.print("Week Day: ");
    Serial.println(weekDay);
    readAvgTemperature();
    
    //Solenoid related things
    if ((weekDay.equals(pesticideDate) && weeklyCount == false) || pupukOverride == true) {
      delay(2000);
      digitalWrite(solenoidPupuk, LOW); //Buka Solenoid Pupuk
      while (totalMilliLitres < takaranPupuk && (millis() - oldTime) > 1000) {
        yfs();
      }

      if (totalMilliLitres >= takaranPupuk) {
        digitalWrite(solenoidPupuk, HIGH); //Tutup Solenoid Pupuk
        pupukOverride = false;
        totalMilliLitres = 0;
        if (weekDay.equals(pesticideDate)) {
          weeklyCount = true;
        }
        sendFertilizerThingspeak();
      }
    }

    if (weekDay != pesticideDate && weeklyCount == true) {
      weeklyCount == false;
    }
    if (moisturePercentage < float(moistureLevelLow)) {
      digitalWrite(solenoidAir, LOW); // Buka Solenoid Air
    }
    if (moisturePercentage > float(moistureLevelLow)) {
      digitalWrite(solenoidAir, HIGH); // Tutup Solenoid Air
    }
    delay(1000);
      if (inputParam1 == "input1" && inputMessage1.toInt()!= 0) {
        moistureLevelLow = inputMessage1.toInt();
        EEPROM.write(addLowThresh, inputMessage1.toInt());
        EEPROM.commit();
        delay(3000);
        Serial.println("New Lower Moisture Level Threshold is: ");
        Serial.print(moistureLevelLow);
        inputParam1 = "none";
        inputMessage1 = "none";
      }
      if (inputParam2 == "input3" && inputMessage2.toInt()!= 0) {
        moistureLevelHi = inputMessage2.toInt();
        EEPROM.write(addHiThresh, inputMessage2.toInt());
        EEPROM.commit();
        Serial.println("New Higher Moisture Level Threshold is: ");
        Serial.print(moistureLevelHi);
        inputParam2 = "none";
        inputMessage2 = "none";
      }
      if (inputParam3 == "input4" && inputMessage3.toInt()!= 0) {
        takaranPupuk = inputMessage3.toInt();
        pest = (takaranPupuk % 100);
        pest1 = (takaranPupuk / 100) % 100;
        pest2 = (takaranPupuk / 10000) % 100;
        pest3 = (takaranPupuk / 1000000) % 100;
        EEPROM.write(pestDose, pest);
        EEPROM.write(pestDose1, pest1);
        EEPROM.write(pestDose2, pest2);
        EEPROM.write(pestDose3, pest3);
        EEPROM.commit();
        Serial.println("New Pesticide Dose is: ");
        Serial.print(takaranPupuk);
        inputParam3 = "none";
        inputMessage3 = "none";
    }
    Serial.print("Day: ");
    Serial.println(weekDay);
    Serial.print("Current Temperature: ");
    Serial.println(Temperature);
    Serial.print("Humidity: ");
    Serial.println(Humidity);
    Serial.print("Soil Moisture: ");
    Serial.print(moisturePercentage);
    Serial.println("%");
    Serial.print("Pesticide Dose ");
    Serial.print(takaranPupuk);
    Serial.println("ml");
    Serial.print("Low Threshold: ");
    Serial.print(moistureLevelLow);
    Serial.println("%");
    Serial.print("High Threshold: ");
    Serial.print(moistureLevelHi);
    Serial.println("%");
    Serial.print("Average Temperature: ");
    Serial.println(avgTemp);
    Serial.println(" ");
    Serial.println(" ");
    sendThingspeak();
  } else {
    Serial.println("WiFi Disconnected");
    delay(5000);
    wifiManager.autoConnect("NodeMCU AP Setup", "AP-PASSWORD");
  }
}
