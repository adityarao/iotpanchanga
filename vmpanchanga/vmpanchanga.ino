// Online OLED 64x128 .94 in display
// Copyright (c) Aditya Rao
// Heavily relies on OLED lib by Adafruit, ArduinoJson and WifiManager libraries

#include <ESP8266WiFi.h>
// controls the 7 segment display

#include <TimeLib.h>

// JSON management library 
#include <ArduinoJson.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
// or #include "SH1106.h" alis for `#include "SH1106Wire.h"`
// For a connection via I2C using brzo_i2c (must be installed) include
// #include <brzo_i2c.h> // Only needed for Arduino 1.6.5 and earlier
// #include "SSD1306Brzo.h"
// #include "SH1106Brzo.h"
// For a connection via SPI include
// #include <SPI.h> // Only needed for Arduino 1.6.5 and earlier
// #include "SSD1306Spi.h"
// #include "SH1106SPi.h"

// Include the UI lib
#include "OLEDDisplayUi.h"

// Include custom images
#include "images.h"

// custom fonts
#include "myfont.h"
#include "om.h"

// timezonedb host & URLs 
const char *timezonedbhost = "api.timezonedb.com";
const char *timezoneparams = "/v2/get-time-zone?key=NAQ9N7E21XFY&by=zone&zone=Asia/Kolkata&format=json";

#define MAX_CONTENT_SIZE  512


#define MAX_ATTEMPTS_FOR_TIME 5
#define MAX_CONNECTION_ATTEMPTS 30
#define GOOGLE_SYNCH_SCHEDULE 3000000

#define MAX_MESSAGE_PER_SLIDE 3


// http port 
#define HTTP_PORT 80
// http port 
#define HTTPS_PORT 443

// define host and connection params to Google Spread Sheets
const char *googlehost = "sheets.googleapis.com";
const char *sheetsURI = "/v4/spreadsheets/1n7tRwZXT9AU4lnxRNGHZRrRcaLSZIq6upOHqIjR1bpI/values/Sheet2!A1:L2?key=AIzaSyDA7nPA320L7-crzsV3OAhUzIkxqupSjhQ";

const String MONTHS[] = {
  "JAN",
  "FEB",
  "MAR",
  "APR",
  "MAY",
  "JUN",
  "JUL",
  "AUG",
  "SEP",
  "OCT",
  "NOV",
  "DEC"
};

const String WEEKDAY[] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};

unsigned long t_GOOGLE_SYNC; 

#define MAX_MESSAGES 12

String pToday[MAX_MESSAGES];
int pTodayMessages = 0;
String pTomorrow[MAX_MESSAGES];
int pTomorrowMessages = 0;

bool inTransition = false; 
bool wasInTransition = false;
bool inFixed = false; 
unsigned long currFrameCounter = 0, currImageFrameCounter = 0;


// Initialize the OLED display using brzo_i2c
// D3 -> SDA
// D5 -> SCL

// Initialize the OLED display using Wire library
SSD1306  display(0x3c, D2, D1);
// SH1106 display(0x3c, D3, D5);

OLEDDisplayUi ui ( &display );

int screenW = 128;
int screenH = 64;
int clockCenterX = screenW/2;
int clockCenterY = ((screenH-16)/2)+16;   // top yellow part is 16 px height
int clockRadius = 23;

// utility function for digital clock display: prints leading 0
String twoDigits(int digits){
  if(digits < 10) {
    String i = '0'+ String(digits);
    return i;
  }
  else {
    return String(digits);
  }
}

void beforeConnectionOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX, 0, "Connecting..." ); 
}

void beforeConnectionFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX, 16, "Connect to Wifi" );
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX, 32, "'panchanga'" );
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX, 48, "using your mobile" );
}

void clockOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  String timenow = String(hour())+ (second()%2? ":" : " ")
    + twoDigits(minute())+ (second()%2? ":" : " ")
    + twoDigits(second());
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX, 0, timenow ); 
}

void dateFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX+x, 16+y, day() + getDatePostFix(day()) + " " + MONTHS[month()-1]+ " " + (String)year()); 
  display->setFont(ArialMT_Plain_24);      
  display->drawString(clockCenterX+x, 32+y, getWeekDay());

  // Change the next slide transition
  if(state->frameState == IN_TRANSITION) {
    if(!inTransition) {
      inTransition = true;
      inFixed = false; 
    }
  } else if(state->frameState == FIXED){
    if(inTransition && !inFixed) { // detect the edge
      inTransition = false;
      inFixed = true;
      currFrameCounter++ ;
    }
  }  
}

void todayIsFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX+x, 16+y, "Today is"); 
  display->setFont(ArialMT_Plain_24);  

  if(display->getStringWidth(pToday[0]) > screenW)
    display->setFont(ArialMT_Plain_16);
  
  display->drawString(clockCenterX+x, 32+y, pToday[0]);
}

void tommorrowIsFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX+x, 16+y, "Tomorrow is"); 
  display->setFont(ArialMT_Plain_24); 

  if(display->getStringWidth(pTomorrow[0]) > screenW)
    display->setFont(ArialMT_Plain_16); 

  display->drawString(clockCenterX+x, 32+y, pTomorrow[0]);
}

void calendarFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {

  int indexCounter = currFrameCounter%(pTodayMessages/MAX_MESSAGE_PER_SLIDE);
  int index[MAX_MESSAGE_PER_SLIDE];
  
  display->setTextAlignment(TEXT_ALIGN_CENTER);

  for(int i = 0; i < MAX_MESSAGE_PER_SLIDE;i++) {
    index[i] = i + MAX_MESSAGE_PER_SLIDE*indexCounter;
    display->setFont(ArialMT_Plain_16);

    if(index[i] < pTodayMessages) {
      if(display->getStringWidth(pToday[index[i]]) > screenW) {
        display->setFont(ArialMT_Plain_10);
      }
      display->drawString(clockCenterX+x, (i+1)*16+y, pToday[index[i]]);     
    }
  }

  // Change the next slide transition
  if(state->frameState == IN_TRANSITION) {
    if(!inTransition) {
      inTransition = true;
      inFixed = false; 
    }
  } else if(state->frameState == FIXED){
    if(inTransition && !inFixed) { // detect the edge
      inTransition = false;
      inFixed = true;
      currImageFrameCounter++ ;
    }
  } 
}

void imageFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) 
{
    if(currImageFrameCounter%2)
      display->drawXbm(x+(screenW-hsvj_width)/2, 16+y, hsvj_width, hsvj_height, hsvj);
    else 
      display->drawXbm(x+(screenW-om_width)/2, 16+y, om_width, om_height, om);


      // Change the next slide transition
    if(state->frameState == IN_TRANSITION) {
      if(!inTransition) {
        inTransition = true;
        inFixed = false; 
      }
    } else if(state->frameState == FIXED){
      if(inTransition && !inFixed) { // detect the edge
        inTransition = false;
        inFixed = true;
        currFrameCounter++ ;
      }
    } 
}

// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { dateFrame , todayIsFrame, calendarFrame, imageFrame, calendarFrame,imageFrame, calendarFrame,imageFrame,tommorrowIsFrame };
FrameCallback beforeConnectionframes[] = { beforeConnectionFrame };
// how many frames are there?
int frameCount = 9;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { clockOverlay };
OverlayCallback beforeConnectionOverlays[] = { beforeConnectionOverlay};


int overlaysCount = 1;

void setup() {
  Serial.begin(115200);
  Serial.println();

	// The ESP is capable of rendering 60fps in 80Mhz mode
	// but that won't give you much time for anything else
	// run it in 160Mhz mode or just set it to 30 fps
  ui.setTargetFPS(60);

  ui.disableAllIndicators();

	// Customize the active and inactive symbol
  //ui.setActiveSymbol(activeSymbol);
  //ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  //ui.setIndicatorPosition(TOP);

  // Defines where the first frame is located in the bar.
  //ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN

  ui.setFrameAnimation(SLIDE_LEFT);


  ui.setFrames(beforeConnectionframes, 1);
  ui.setOverlays(beforeConnectionOverlays, 1);
  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();  

  ui.update();

  // now connect to the WiFi
  /*if(!connectToWiFi()){
    Serial.println("Could not connect to WiFi !");
    return; 
  } */

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.autoConnect("panchanga");

  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();

  //setTime(getTimeBackup());

  // set up sync provider for getting time every 180 seconds
  setSyncProvider(getTimeDataBackup);
  setSyncInterval(600);   
}


void loop() {
  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);

  }
}

// wrapper API for getTimeFromTimeZoneDB
time_t getTimeDataBackup() 
{
  syncPanchangaFromGoogleSheets(googlehost, sheetsURI);
  return getTimeFromTimeZoneDB(timezonedbhost, timezoneparams);
}

// connect to timezone db !
time_t getTimeFromTimeZoneDB(const char* host, const char* params) 
{
  Serial.print("Trying to connect to ");
  Serial.println(host);

  // Use WiFiClient for timezonedb 
  WiFiClient client;
  if (!client.connect(host, 80)) {
    Serial.print("Failed to connect to :");
    Serial.println(host);
    return 0;
  }

  Serial.println("connected !....");

  // send a GET request
  client.print(String("GET ") + timezoneparams + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" +
             "User-Agent: ESP8266\r\n" +
             "Accept: */*\r\n" +
             "Connection: close\r\n\r\n");

  // bypass HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print( "Header: ");
    Serial.println(line);
    if (line == "\r") {
      break;
    }
  }

  // get the length component
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print( "Body Length: ");
    Serial.println(line);    
      break;
  } 

  String line = "";

  // get the actual body, which has the JSON content
  while (client.connected()) {
    line = client.readStringUntil('\n');
    Serial.print( "Json body: ");
    Serial.println(line);    
    break;
  }   

  // Use Arduino JSON libraries parse the JSON object
  const size_t BUFFER_SIZE =
      JSON_OBJECT_SIZE(8)    // the root object has 8 elements
      + MAX_CONTENT_SIZE;    // additional space for strings

  // Allocate a temporary memory pool
  DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);

  JsonObject& root = jsonBuffer.parseObject(line);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return 0;
  }  

  Serial.println("Parsing a success ! -- ");

  // 'timestamp' has the exact UNIX time for the zone specified by zone param
  Serial.println((long)root["timestamp"]);

  return (time_t)root["timestamp"];
}

// connects to Wifi and sets up the WebServer !
bool connectToWiFi() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  int count = 0;

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    if(count++ > MAX_CONNECTION_ATTEMPTS) // 30 attempts
      return false; 
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  
  String localIP = WiFi.localIP().toString();

  Serial.println(localIP);

  return true;
}

// get the correct post fix ex: 1st, 22nd, 31st etc
String getDatePostFix(int d)
{
  if(d == 1 || d == 21 || d == 31)
    return "st";

  if(d == 2 || d == 22)
    return "nd";

  if(d == 3 || d == 23)
    return "rd";

  return "th";
}

String getFullDate()
{
  int currDay = day();
  return currDay + getDatePostFix(currDay) + " " + MONTHS[month()-1] + " " + year();
}

String getWeekDay()
{
  return WEEKDAY[weekday()-1];
}

String getCurrentTime()
{
  String fMinute; 
  int currMinute = minute();
  
  if(currMinute < 10)
    fMinute = "0" + (String) currMinute;
  else 
    fMinute = currMinute;

  return  (String)hourFormat12() + ":" + fMinute + " " + (isAM() ? "AM" : "PM");
}

// get Panchanga for today from Google spreadsheets
boolean syncPanchangaFromGoogleSheets(const char* host, const char* uri) {
  Serial.print("Trying to connect to ");
  Serial.println(host);

  // Use WiFiClient 
  WiFiClientSecure client;
  if (!client.connect(host, HTTPS_PORT)) {
    Serial.print("Failed to connect to :");
    Serial.println(host);
    return false;
  }

  Serial.println("connected !....");

  client.print(String("GET ") + uri + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" +
             "User-Agent: ESP8266\r\n" +
             "Accept: */*\r\n" +
             "Connection: close\r\n\r\n");

  // bypass HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print( "Header: ");
    Serial.println(line);
    if (line == "\r") {
      break;
    }
  }

  const size_t BUFFER_SIZE =
      JSON_OBJECT_SIZE(8)    // the root object has 8 elements
      + MAX_CONTENT_SIZE;    // additional space for strings

  // Allocate a temporary memory pool
  DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);

  JsonObject& root = jsonBuffer.parseObject(client);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return false;
  }  

  Serial.println("Parsing a success ! ");

  bool isGetTomm = false; 

  pTodayMessages = pTomorrowMessages = 0;

  JsonArray& scheduleArray = root["values"];
  for (JsonArray& t : scheduleArray) {
    for (JsonVariant o : t) {
      Serial.print("Values : ");
      Serial.println(o.as<String>());  

      if(!isGetTomm) { // get today's panchanga
        pToday[pTodayMessages++] = o.as<String>();
        if(pTodayMessages == MAX_MESSAGES)
          break;
      } else {
        pTomorrow[pTomorrowMessages++] = o.as<String>();
        if(pTomorrowMessages == MAX_MESSAGES)
          break;
      }
    }  

    isGetTomm = true;
  }

  return true;
}