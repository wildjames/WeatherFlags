#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <SPI.h> 
#include <WiFi.h>
#include <HTTPClient.h>

#include "OWM_key.h"
#include "WiFi_cred.h"

//open weather map api key 
String apiKey = API_KEY; 

// How many slots does the forecast return to us? From the docs.
int N_forecasts = 48;
// unix time.
int currentTime;

// The open weather map API call
String OWM_url = "http://api.openweathermap.org/data/2.5/onecall?lat=53.383&lon=-1.4659&exclude=daily,alerts,minutely&appid=";
String UNIX_timeof_url = "http://showcase.api.linx.twenty57.net/UnixTime/tounix?date=";

int status = WL_IDLE_STATUS; 

WiFiClient client; 

// This stores the weather info. We can loop over it later.
String jsonBuffer;
DynamicJsonDocument doc(26000);
JsonArray forecast;

// Servo stuff
int lowangle = 0;
int highangle = 180;


Servo servo_firepit;
int pin_firepit = 12;

Servo servo_umbrella;
int pin_umbrella = 14;

Servo servo_3;
int pin_3 = 15;

Servo servo_4;
int pin_4 = 13;

Servo servo_5;
int pin_5 = 2;



void setup() {
  //Initialize serial and wait for port to open: 
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n\nBegun");

  // Add the API key to the OWM url tail
  OWM_url = OWM_url + API_KEY;

  // Set up servos
  Serial.print("Setting all servos to lowangle, or ");
  Serial.println(lowangle);
  servo_firepit.attach(pin_firepit);
  servo_firepit.write(lowangle);

  servo_umbrella.attach(pin_umbrella);
  servo_umbrella.write(lowangle);

  servo_3.attach(pin_3);
  servo_3.write(lowangle);

  servo_4.attach(pin_4);
  servo_4.write(lowangle);

  servo_5.attach(pin_5);
  servo_5.write(lowangle);


  // Demonstrate the servos are working correctly
  set_flag(servo_firepit, true);
  set_flag(servo_umbrella, true);
  set_flag(servo_3, true);
  set_flag(servo_4, true);
  set_flag(servo_5, true);
  
  set_flag(servo_firepit, false);
  set_flag(servo_umbrella, false);
  set_flag(servo_3, false);
  set_flag(servo_4, false);
  set_flag(servo_5, false);

}


void loop() {
  connect_wifi();
  
  getWeather();
  
  currentTime = getNowTime();
  Serial.print("Current time in loop is ");
  Serial.println(currentTime);


  // And then process all my flags.

  bool condition1 = eval_firepit();
  Serial.print("The firepit flag is ");
  if (condition1) {Serial.println("Raised");} else {Serial.println("Lowered");}

  bool condition2 = eval_umbrella();
  Serial.print("The umbrella flag is ");
  if (condition2) {Serial.println("Raised");} else {Serial.println("Lowered");}

  Serial.println("Setting firepit angle");
  set_flag(servo_firepit, condition1);
  Serial.println("Setting umbrella angle");
  set_flag(servo_umbrella, condition2);

  
  
  delay(60000);
}


void connect_wifi() {
  // attempt to connect to Wifi network, if I'm not already.
  while (status != WL_CONNECTED) { 
    
    Serial.print("Attempting to connect to SSID: "); 
    Serial.println(ssid); 
    status = WiFi.begin(ssid, pass); 

    if (status == WL_CONNECTED) {      
      Serial.println("Connected to wifi"); 
    } else {
      //wait, then retry
      delay(10000);
    }
  } 
}


void set_flag(Servo &flagServo, bool targetPos) {
  // Get the angle I want to be at
  int targetAngle;
  Serial.print("targetPos is ");
  Serial.println(targetPos);
  if (targetPos) {
    targetAngle = highangle;
  } else {
    targetAngle = lowangle;
  }

  // Retrieve the servos current position
  int currAngle = flagServo.read();
  Serial.print("Servo is currently at angle ");
  Serial.println(currAngle);
  Serial.print("And the target angle is ");
  Serial.println(targetAngle);

  // Move by 1 degree every 20ms
  if (currAngle < targetAngle) {
    Serial.println("Raising flag");
    while (currAngle < targetAngle) {
      currAngle++;
      flagServo.write(currAngle);
      delay(20);
    }
  } else {
    Serial.println("Lowering flag");
    while (currAngle > targetAngle) {
      currAngle--;
      flagServo.write(currAngle);
      delay(20);
    }
  }

  Serial.println("");
}

bool eval_firepit() {
  // above 10 degrees from 5pm - 12pm, with no precipitation.
  Serial.println("\n\nChecking for firepit flag");

  bool isOK = true;
  bool anyValid = false;
  bool isValidTime;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int lowtime = getTimeOf(String("10:00"));
  int hightime = getTimeOf(String("22:00"));
  Serial.print("lowtime: ");
  Serial.println(lowtime);
  Serial.print("hightime: ");
  Serial.println(hightime);

  // Temperature must be HIGHER than this. Units are kelvin.
  int minTemp = 273 + 10;
  
  Serial.print("Temperature threshold is ");
  Serial.println(minTemp);
  
  for (int i=0; i<N_forecasts; i++) {    
    JsonObject thisForecast = forecast[i];
    dt = thisForecast["dt"];
    isValidTime = ((dt > lowtime) and (dt < hightime));
    if (isValidTime) {anyValid = true;}

    Serial.print("Is this a valid time? ");
    Serial.print(dt);
    Serial.print(" -> ");
    Serial.println(isValidTime);
    
    if (isValidTime and isOK) {
      // Check if the temperature is high enough to be comfy
      
      int forecastTemp = thisForecast["feels_like"];
      isOK = forecastTemp > minTemp;
      
      Serial.print("\nFor timestamp ");
      Serial.print(dt);
      Serial.println(" is the weather good?");
      Serial.print("Temperature feels like ");
      Serial.println(forecastTemp);
      Serial.print("Is this good? ");
      if (isOK) {Serial.println("YES");} else {Serial.println("NO");}
    }
    
    if (isValidTime and isOK) {
      // Check if there's no rain or snow, or whatever. code 800+ is for no precipitation
      
      JsonObject thisWeather = thisForecast["weather"][0];
      
      int weatherCode = thisWeather["id"];
      isOK = (weatherCode >= 800);

      Serial.print("The weather code is ");
      Serial.println(weatherCode);
      Serial.print("Is this ok? ");
      if (isOK) {Serial.println("YES");} else {Serial.println("NO");}
    }
  }

  // if no timestamps were valid, the check failed due to it being too late in the day,
  // i.e. after 10pm.
  isOK = (isOK and anyValid);

  return isOK;
}


bool eval_umbrella() {
  // If it's raining between 8am and 11am, or between 3pm and 6pm, raise this flag
  Serial.println("\n\nChecking for umbrella flag");

  // Initially True, if we break a rule the loop exits.
  bool isOK = true;
  bool anyValid = false;
  bool isValidTime;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int lowtime1 = getTimeOf(String("8:00"));
  int hightime1 = getTimeOf(String("10:00"));
  
  int lowtime2 = getTimeOf(String("15:00"));
  int hightime2 = getTimeOf(String("18:00"));

  // If the first digit of the weather code is 5, {[no 4 codes exist], 3, 2}, it's raining.
  // Test for code less than 600
  int rain_code = 600;
  int weatherCode;
  
  // Loop over the forecast blocks.
  // Test to see if it's raining in these times
  for (int i=0; i<N_forecasts; i++) {
    JsonObject thisForecast = forecast[i];
    JsonObject thisWeather = thisForecast["weather"][0];
    
    dt = thisForecast["dt"];
    isValidTime = (((dt > lowtime1) and (dt < hightime1)) or ((dt > lowtime2) and (dt < hightime2)));
    
    if (isValidTime) {anyValid = true;}
    
    weatherCode = thisWeather["id"];
    
    if (isValidTime and isOK) {
      isOK = (weatherCode < rain_code);
      if (not isOK) {
        Serial.print("I see weather code ");
        Serial.print(weatherCode);
        Serial.print("At time ");
        Serial.println(dt);
      }
    }
  }

  return isOK;
}



bool eval_picnic() {
  // ONLY if sunset is >6pm
  // Sunny on weekdays 4pm-6pm, or weekends all day
  Serial.println("\n\nChecking for picnic flag");

  // Initially True, if we break a rule the loop exits.
  bool isOK = true;
  bool anyValid = false;
  bool isValidTime;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int lowtime = getTimeOf(String("10:00"));
  int hightime = getTimeOf(String("22:00"));


  // Machine-critera variables go here
  int minTemp = 273 + 10;

  // Loop over the forecast blocks.
  for (int i=0; i<N_forecasts; i++) {
    JsonObject thisForecast = forecast[i];
    JsonObject thisWeather = thisForecast["weather"][0];
    
    dt = thisForecast["dt"];
    isValidTime = ((dt > lowtime) and (dt < hightime));
    
    if (isValidTime) {anyValid = true;}
    
    if (isValidTime and isOK) {
      // Check stuff here
    }
  }

  return isOK;
}


bool eval_template() {
  // Criteria in human-speak go here
  Serial.println("\n\nChecking for XXX flag");

  // Initially True, if we break a rule the loop exits.
  bool isOK = true;
  bool anyValid = false;
  bool isValidTime;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int lowtime = getTimeOf(String("10:00"));
  int hightime = getTimeOf(String("22:00"));


  // Machine-critera variables go here
  int minTemp = 273 + 10;

  // Loop over the forecast blocks.
  for (int i=0; i<N_forecasts; i++) {
    JsonObject thisForecast = forecast[i];
    JsonObject thisWeather = thisForecast["weather"][0];
    
    dt = thisForecast["dt"];
    isValidTime = ((dt > lowtime) and (dt < hightime));
    
    if (isValidTime) {anyValid = true;}
    
    if (isValidTime and isOK) {
      // Check stuff here
    }
  }

  return isOK;
}



int getTimeOf(String timeString) {
  // Get the unix time of a date/time string. Accepts "now" as an option. 
  // If no date is given, assumes you mean today.
  
  String get_url = UNIX_timeof_url + timeString;

  jsonBuffer = httpGETRequest(get_url.c_str());
  jsonBuffer.replace('"', ' ');
  jsonBuffer.trim();
  int Time = jsonBuffer.toInt();

  return Time;
}


int getNowTime() {
  // Get the current time in Unix time
  
  int Time = getTimeOf("now");
  return Time;
}


void getWeather() { 
  // Gets the weather forecast from OWM. Saves the hourly forecast to a global variable
  // https://openweathermap.org/api/one-call-api
  
  Serial.println("\nConnecting to OWM server..."); 
  jsonBuffer = httpGETRequest(OWM_url.c_str());
  
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  forecast = doc["hourly"];

  int dt = forecast[0]["dt"];
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  } else {
    Serial.println("Successfully got the weather!\n");
  }
}


String httpGETRequest(const char* serverName) {
  //makes a GET request (obviously), and returns the payload response. 
  // If an error is hit, reports the error. 
  
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
//    Serial.print("HTTP Response code: ");
//    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
