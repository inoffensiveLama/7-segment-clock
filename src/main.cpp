#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>

/*
#########################
###     VARIABLES     ###
#########################
*/

// network credentials
//for internet access
const char* ssid = "Redmi Note 12";
const char* password = "12345678";

//for own hotspot to change settings
const char* apSSID = "ESP32_Hotspot";
const char* apPassword = "12345678";

// API endpoint
const char* apiEndpoint = "https://www.timeapi.io/api/time/current/zone?timeZone=Asia%2FManila";

//set up the pin numbers
const int Latch_pin = 21;
const int Clock_pin = 19;
const int Data_pin = 18;

//variables to hold time, these are primarily used for displaying the numbers
int seconds = 0;
int minutes = 59;
int hours = 23;

int lastTimeUpdate = 0;
int lastTimeFetch = 0;
int lastTimeWiFi = 0;

int fetchInterval = 10000; //this could be around 3600000 for once an hour

// WebServer instance
WebServer server(80);

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);  // typically same as local_IP
IPAddress subnet(255, 255, 255,0); // standard subnet mask

//array that holds all the binary values for the shiftregisters
int digits [10][8]{
  {1,1,1,1,1,1,0,0}, // digit 0
  {0,1,1,0,0,0,0,0}, // digit 1
  {1,1,0,1,1,0,1,0}, // digit 2
  {1,1,1,1,0,0,1,0}, // digit 3
  {0,1,1,0,0,1,1,0}, // digit 4
  {1,0,1,1,0,1,1,0}, // digit 5
  {1,0,1,1,1,1,1,0}, // digit 6
  {1,1,1,0,0,0,0,0}, // digit 7
  {1,1,1,1,1,1,1,0}, // digit 8
  {1,1,1,1,0,1,1,0}  // digit 9

  /*
  (A, B, C, D, E, F, G, DP)

    * A *
    F   B
    * G *
    E   C
    * D * DP
  */
};

/*
#####################################
###     FUNCTION DECLARATIONS     ###
#####################################
*/

void WriteDigitToShiftRegister(int);
void initWiFi();
void IncreaseTime();
void FetchTime();
void DisplayTime();
void loop2 (void* pvParameters);
void initHotspot();
void hostWebsite();
void setTimeFromWebsite();


/*
#####################
###     SETUP     ###
#####################
*/

void setup() {
  //setup pinmodes
  pinMode(Latch_pin, OUTPUT);
  pinMode(Clock_pin, OUTPUT);
  pinMode(Data_pin, OUTPUT);

  Serial.begin(9600);

  //setup WiFi
  //the for loop is to show the all 0s in the screen. this is a simplified "loadingscreen"
  digitalWrite(Latch_pin, LOW); 
  for (int i = 0; i < 6; i++)
  {
    WriteDigitToShiftRegister(0);
  }
  digitalWrite(Latch_pin, HIGH); 

  initWiFi();

  //initialize Hotspot
  initHotspot();
  // Initialize web server routes
  server.on("/", hostWebsite);
  server.on("/settime", HTTP_POST, setTimeFromWebsite);
  
  server.begin();

  //fetch the time to initiate the clock and showing the current time
  FetchTime();
  DisplayTime();


  xTaskCreatePinnedToCore (
    loop2,     // Function to implement the task
    "loop2",   // Name of the task
    4000,      // Stack size in words
    NULL,      // Task input parameter
    0,         // Priority of the task
    NULL,      // Task handle.
    0          // Core where the task should run
  );


  //After the whole setup take the current runtime of the program (millis()) and put it into lastTimeUpdate
  lastTimeUpdate = millis();
}



/*
####################
###     LOOP     ###
####################
*/

//first loop only deals with displaying the time
void loop() {

  //Serial.print("loop running on core: ");
  // Serial.println(xPortGetCoreID());

  //check if 1 second has passed since the last time the seconds have been incremented by one
  if (millis() > (lastTimeUpdate + 1000))
  {
    IncreaseTime();
    lastTimeUpdate += 1000;
    DisplayTime();
  }

delay(1000);
  
}


//second loop only deals with wifi related things
void loop2(void* pvParameters) {

  while(true)
  {
    // Serial.print("loop2 running on core: ");
    // Serial.println(xPortGetCoreID());

    //fetch time from api once an hour (if connected to the internet)
    if (millis() > (lastTimeFetch + fetchInterval))
    {
      FetchTime();
      lastTimeFetch += fetchInterval;
      lastTimeUpdate = millis();
      DisplayTime();
    }


    delay(1000);
  
    server.handleClient();
  
  }

}




/*
#########################
###     FUNCTIONS     ###
#########################
*/

//function to connect to WiFi
void initWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  //Serial.print("Connecting to WiFi ..");
  int attempts = 5;
  while (WiFi.status() != WL_CONNECTED) {
    //Serial.print('.');
    delay(1000);
    attempts--;
    if (attempts <= 0)
    {
      break;
    }
    Serial.print("attempt: ");
    Serial.println(20 - attempts);
  }
  //Serial.println(WiFi.localIP());
}


//function to initialize hotspot
void initHotspot() {
  WiFi.softAP(apSSID, apPassword);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  // Serial.print("IP address: ");
  // Serial.println(WiFi.softAPIP());
}


//function to host the website where you can change the time
void hostWebsite() {
  String html = "<html><body><h1>Set Time</h1>";
  html += "<form action='/settime' method='POST'>";
  html += "Hours: <input type='number' name='hours' min='0' max='23'><br>";
  html += "Minutes: <input type='number' name='minutes' min='0' max='59'><br>";
  html += "Seconds: <input type='number' name='seconds' min='0' max='59'><br>";
  html += "Timezone: <input type='list' name='timezone'><br>";
  html += "<input type='submit' value='Set Time'>";
  html += "</form></body></html>";
  
  server.send(200, "text/html", html);
}


//function to set time from the user input
void setTimeFromWebsite() {
  if (server.hasArg("hours") && server.hasArg("minutes") && server.hasArg("seconds")) {
    hours = server.arg("hours").toInt();
    minutes = server.arg("minutes").toInt();
    seconds = server.arg("seconds").toInt();

    server.send(200, "text/html", "<html><body><h1>Time Updated</h1><a href='/'>Go Back</a></body></html>");
    DisplayTime();
  } else {
    server.send(400, "text/html", "Invalid request.");
  }
}


//function to send a 1 digit number to the shift register (if you have more than 1 digit simply call it as many times as needed)
void WriteDigitToShiftRegister(int Digit)
{
  //they need to be sent in to the shift register the "wrong way" so the last digit is being sent in first 
  for (int i = 7; i>=0; i--)
   {
    digitalWrite(Clock_pin,LOW);
    if (digits[Digit][i]==1) {digitalWrite(Data_pin, LOW); /* Serial.print(0); */} 
    if (digits[Digit][i]==0) {digitalWrite(Data_pin, HIGH); /* Serial.print(1); */}
    digitalWrite(Clock_pin,HIGH);
   }
}


//function to increase seconds by 1, and also check if it is 60 to increase minutes and hours
void IncreaseTime()
{
  seconds++;
  if (seconds >= 60)
  {
    minutes++;
    seconds = 0;
  }
  if (minutes >= 60)
  {
    hours++;
    minutes = 0;
  }
  if (hours >= 24)
  {
    hours = 0;
  }
}


//function to fetch the time from an API and store the seconds, minutes and hours into the variables.
void FetchTime()
{
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(apiEndpoint);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      //Serial.println("HTTP Response payload: ");
      //Serial.println(payload);
      
      // Parse JSON response
      JsonDocument doc;
      deserializeJson(doc, payload);
      
      // Extract time-related information
      hours = doc["hour"];
      minutes = doc["minute"];
      seconds = doc["seconds"];
    } else {
      //Serial.printf("Error in HTTP request, code: %d\n", httpCode);
    }
    
    http.end(); // Close connection
  } else {
    Serial.println("WiFi not connected.");
  }
}


//function to put all digits into the shift registers and update the screen
void DisplayTime()
{

  digitalWrite(Latch_pin, LOW);
  
  //write the seconds ones to the shift registers:
  WriteDigitToShiftRegister(seconds % 10);
  //wirte the seconds tens to the shift registers:
  WriteDigitToShiftRegister(seconds / 10);
  
  //write the minutes ones to the shift registers:
  WriteDigitToShiftRegister(minutes % 10);
  //wirte the minutes tens to the shift registers:
  WriteDigitToShiftRegister(minutes / 10);

  //write the hours ones to the shift registers:
  WriteDigitToShiftRegister(hours % 10);
  //wirte the hours tens to the shift registers:
  WriteDigitToShiftRegister(hours / 10);

  digitalWrite(Latch_pin, HIGH); 

}