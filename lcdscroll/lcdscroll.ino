#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <LiquidCrystal_I2C.h>

// enables UDP
#include <WiFiUdp.h>

// time management libraries - uses millis() to manage time
#include <Time.h>
#include <TimeLib.h>

// JSON management library 
#include <ArduinoJson.h>

// Secure WiFi client 
#include <WiFiClientSecure.h>

#define WIDTH 20
#define ROWS 4
#define ADDR 0x3f

#define SCROLL_DELAY 250

// Wifi parameters / auth
#define ssid  "harisarvottama"
#define password "9885104058"

#define MAX_ATTEMPTS_FOR_TIME 5
#define MAX_CONNECTION_ATTEMPTS 30

// http port 
#define HTTP_PORT 80
// http port 
#define HTTPS_PORT 443

#define GOOGLE_SYNCH_SCHEDULE 3000000

#define MESSAGE_DELAY 2000

#define WAIT_BEFORE_SCROLL 1000

const size_t MAX_CONTENT_SIZE = 512;

// timezonedb host & URLs 
const char *timezonedbhost = "api.timezonedb.com";
const char *timezoneparams = "/v2/get-time-zone?key=NAQ9N7E21XFY&by=zone&zone=Asia/Kolkata&format=json";

// define host and connection params to Google Spread Sheets
const char *googlehost = "sheets.googleapis.com";
const char *sheetsURI = "/v4/spreadsheets/1n7tRwZXT9AU4lnxRNGHZRrRcaLSZIq6upOHqIjR1bpI/values/Sheet2!A1:L2?key=AIzaSyDA7nPA320L7-crzsV3OAhUzIkxqupSjhQ";

time_t timeVal; 

const String MONTHS[] = {
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
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

/*
LCD config
  SDA pin - D2
  SCL pin - D1
  VCC Pin - 5v

*/

// 16x2 works on 0x27 addr
LiquidCrystal_I2C lcd(ADDR, WIDTH, ROWS);

void printStringLCD1602(LiquidCrystal_I2C, String, String, int);
void printStringLCD2004(LiquidCrystal_I2C, String [],int ,int, bool);
bool connectToWiFi();
time_t getTime();
time_t getTimeFromTimeZoneDB(const char* , const char* ) ;
boolean syncPanchangaFromGoogleSheets(const char* , const char* );
String getCurrentTime();
String getWeekDay();
String getDatePostFix(int);
String getFullDate(); 

String data[4]; // store the values of the string to display

void setup() {

  int attempt = 0;
  t_GOOGLE_SYNC = millis();

  Serial.begin(115200);
  lcd.init();

  // Turn on the backlight.
  lcd.backlight();

  printStringLCD1602(lcd, "Connecting ...", "", 0);
  delay(MESSAGE_DELAY); 
  lcd.clear();

  if(connectToWiFi()) {
    printStringLCD1602(lcd, "Connected !!", "Getting time ...",1000);
    delay(MESSAGE_DELAY);
    lcd.clear(); 
    // get time 
    
    while(attempt < MAX_ATTEMPTS_FOR_TIME) {
      timeVal = getTime();
      if(timeVal) // got time !
        break;

      attempt++; 
    }
  
    Serial.print("Get time value : ");
    Serial.println(timeVal);

    if(timeVal > 0) {
      setTime(timeVal);
    
      // set up sync provider for getting time every 180 seconds
      setSyncProvider(getTime);
      setSyncInterval(180);  

      // sync data with google sheets
      syncPanchangaFromGoogleSheets(googlehost, sheetsURI);

    } else { 
      printStringLCD1602(lcd, "Error!", "Unable to get time!",1000);
      delay(MESSAGE_DELAY);
      lcd.clear(); 
    }    
  } else {
    printStringLCD1602(lcd, "Unable to connect to internet ! ", "Check your connection!",1000);
    delay(MESSAGE_DELAY*5); 
    lcd.clear();
  }   
}


void loop() {



  // this is for 1602 display 
  /*
  printStringLCD1602(lcd, "Hari Sarvottama", "Vayu Jeevothama", 3000);
  delay(MESSAGE_DELAY);
  lcd.clear(); 

  printStringLCD1602(lcd, "TIME: " + getCurrentTime(), getWeekDay(), 3000);
  delay(MESSAGE_DELAY);
  lcd.clear();  

  printStringLCD1602(lcd, (String)day() + getDatePostFix(day()) + " " + MONTHS[month()-1], (String)year(), 3000);
  delay(MESSAGE_DELAY);
  lcd.clear();   

  printStringLCD1602(lcd, "Panchanga for", "TODAY", 1000);
  delay(MESSAGE_DELAY);
  lcd.clear();  

  printStringLCD1602(lcd, "TODAY IS", pToday[0], 1000);
  delay(MESSAGE_DELAY);
  lcd.clear();    

  // display for today
  for(int i = 0; i < pTodayMessages/2; i++) {
    printStringLCD1602(lcd, pToday[i*2], pToday[i*2+1], 3000);
    delay(MESSAGE_DELAY);
    lcd.clear();       
  }

  // just display tomorrow's thiti
  printStringLCD1602(lcd, "TOMORROW IS", pTomorrow[0], 3000);
  delay(MESSAGE_DELAY);
  lcd.clear();       
  */


  data[0] = "";
  data[1] = "Hari Sarvottama";
  data[2] = "Vayu Jeevottama";
  data[3] = "";
  printStringLCD2004(lcd, data, WAIT_BEFORE_SCROLL, MESSAGE_DELAY, true);


  data[0] = "TIME : " + getCurrentTime();
  data[1] = day() + getDatePostFix(day()) + " " + MONTHS[month()-1] + " " + year();
  data[2] = getWeekDay();
  data[3] = "";
  printStringLCD2004(lcd, data, WAIT_BEFORE_SCROLL, MESSAGE_DELAY, true);

  
  if((millis() - t_GOOGLE_SYNC)> GOOGLE_SYNCH_SCHEDULE) {
    t_GOOGLE_SYNC = millis();
    lcd.clear(); 
    printStringLCD1602(lcd, "Synch from Google ..", "", 3000);
    syncPanchangaFromGoogleSheets(googlehost, sheetsURI);
  }

}

void printStringLCD2004(LiquidCrystal_I2C l, String strings[], int delayBeforeScroll, int delayAfterScroll, bool clearScreen)
{
  int lengths[4];

  for(int i = 0; i < 4; i++) {
    lengths[i] = strings[i].length();
  }

  int longerLength = 0; 
  // find the largest string
  for(int i = 0; i < 4; i++) {
    if(longerLength < lengths[i])
      longerLength = lengths[i];
  }

  bool needsScrolling = (longerLength > WIDTH);

  if(!needsScrolling) { // does not need scrolling
    for(int i = 0; i < 4; i++) {
      l.setCursor((WIDTH-lengths[i])/2, i);
      l.print(strings[i]);     
    }
  } else {
    int shiftSteps = longerLength - WIDTH; 

    for(int i = 0; i < 4; i++) {
      if(lengths[i] >= WIDTH) { // bigger than the LCD width
        l.setCursor(0, i);
        l.print(strings[i]);
        Serial.print("Longer String : ");
        Serial.println(strings[i]);
      } else {
        l.setCursor((WIDTH-lengths[i])/2, i);
        l.print(strings[i]);   
        Serial.print("Shorter String : ");
        Serial.println(strings[i]);                 
      }      
    } 

    delay(delayBeforeScroll);
    for(int i = 0; i <shiftSteps; i++) {
      lcd.scrollDisplayLeft();
      delay(SCROLL_DELAY);     
    }
  }

  delay(delayAfterScroll);
  if(clearScreen)
    l.clear();
}

void printStringLCD1602(LiquidCrystal_I2C l, String line1, String line2, int delayBeforeScroll)
{
  int line1Length = line1.length();
  int line2Length = line2.length();

  int longerLength = (line1Length > line2Length) ? line1Length : line2Length;

  bool needsScrolling = (longerLength > WIDTH);

  if(!needsScrolling) { // does not need scrolling
    l.setCursor((WIDTH-line1Length)/2, 0);
    l.print(line1);

    l.setCursor((WIDTH-line2Length)/2, 1);
    l.print(line2);
  } else {
    int shiftSteps = longerLength - WIDTH; 

    if(line1Length >= WIDTH) { // bigger than the LCD width
      l.setCursor(0, 0);
      l.print(line1);
    } else {
      l.setCursor((WIDTH-line1Length)/2, 0);
      l.print(line1);            
    }

    if(line2Length >= WIDTH) { // bigger than the LCD width
      l.setCursor(0, 1);
      l.print(line2);
    } else {
      l.setCursor((WIDTH-line2Length)/2, 1);
      l.print(line2);            
    }  

    delay(delayBeforeScroll);

    for(int i = 0; i <shiftSteps; i++ ) {
      lcd.scrollDisplayLeft();
      delay(SCROLL_DELAY);     
    }
  }
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
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  
  String localIP = WiFi.localIP().toString();
  Serial.println(localIP);

  return true;
}

// wrapper API for getTimeFromTimeZoneDB
time_t getTime() 
{
  lcd.clear();
  printStringLCD1602(lcd, "Sync Time", "Connecting...", 1000); 
  time_t  t = getTimeFromTimeZoneDB(timezonedbhost, timezoneparams);  
  return t;  
}

// connect to timezone db !
time_t getTimeFromTimeZoneDB(const char* host, const char* params) 
{
  Serial.print("Trying to connect to ");
  Serial.println(host);

  // Use WiFiClient for timezonedb 
  WiFiClient client;
  if (!client.connect(host, HTTP_PORT)) {
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