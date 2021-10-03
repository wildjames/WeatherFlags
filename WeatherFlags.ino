#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <SPI.h> 
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <TimeLib.h>

#include "OWM_key.h"
#include "WiFi_cred.h"


// How many slots does the forecast return to us? From the docs.
int N_forecasts = 48;
int N_yesterday = 24;


// The open weather map API call
String OWM_url = "http://api.openweathermap.org/data/2.5/onecall?lat=53.383&lon=-1.4659&exclude=daily,alerts,minutely&appid=";
String OWM_past_url = "http://api.openweathermap.org/data/2.5/onecall/timemachine?lat=53.383&lon=-1.4659&dt=%d&appid=%s";

// WiFi and connectivity stuff
int status = WL_IDLE_STATUS; 

WiFiClient client; 
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets, for the NTP time sync


// Time getter stuff
const int timeZone = 0;
static const char ntpServerName[] = "time.nist.gov";
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
int daylight_savings = 0;

// This stores the weather info. We can loop over it later.
String json_buffer;
DynamicJsonDocument forecast_doc(26000);
DynamicJsonDocument todays_history_doc(12288);
DynamicJsonDocument yesterdays_history_doc(12288);
JsonArray forecast;
JsonArray todays_history;
JsonArray yesterdays_history;


// Servo stuff
int lowangle = 0;
int highangle = 180;

Servo servo_firepit;
int pin_firepit = 12;

Servo servo_umbrella;
int pin_umbrella = 14;

Servo servo_picnic;
int pin_picnic = 15;

Servo servo_hike;
int pin_hike = 13;

Servo servo_storm;
int pin_storm = 2;



void setup() {
  //Initialize serial and wait for port to open: 
  Serial.begin(115200);
//  while (!Serial);
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

  servo_picnic.attach(pin_picnic);
  servo_picnic.write(lowangle);

  servo_hike.attach(pin_hike);
  servo_hike.write(lowangle);

  servo_storm.attach(pin_storm);
  servo_storm.write(lowangle);


  // Demonstrate the servos are working correctly
  set_flag(servo_firepit, true);
  set_flag(servo_umbrella, true);
  set_flag(servo_picnic, true);
  set_flag(servo_hike, true);
  set_flag(servo_storm, true);
  
  set_flag(servo_firepit, false);
  set_flag(servo_umbrella, false);
  set_flag(servo_picnic, false);
  set_flag(servo_hike, false);
  set_flag(servo_storm, false);


  connect_wifi();
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}


void loop() {
  connect_wifi();
  
  getWeather();
  getPastWeather();
  
  tmElements_t tm;
  breakTime(now(), tm);
  Serial.print("Current day is ");
  Serial.println(tm.Day);


  // And then process all my flags.

  bool condition1 = evalFirepit();
  Serial.print("The firepit flag is ");
  if (condition1) {Serial.println("Raised");} else {Serial.println("Lowered");}
    Serial.println("Setting firepit angle");
  set_flag(servo_firepit, condition1);

  
  bool condition2 = evalUmbrella();
  Serial.print("The umbrella flag is ");
  if (condition2) {Serial.println("Raised");} else {Serial.println("Lowered");}
  Serial.println("Setting umbrella angle");
  set_flag(servo_umbrella, condition2);


  bool condition3 = evalPicnic();
  Serial.print("The picnic flag is ");
  if (condition3) {Serial.println("Raised");} else {Serial.println("Lowered");}
  Serial.println("Setting picnic angle");
  set_flag(servo_picnic, condition3);


  bool condition4 = evalStorm();
  Serial.print("The storm flag is ");
  if (condition4) {Serial.println("Raised");} else {Serial.println("Lowered");}
  Serial.println("Setting storm angle");
  set_flag(servo_storm, condition4);


  bool condition5 = evalHike();
  Serial.print("The hike flag is ");
  if (condition5) {Serial.println("Raised");} else {Serial.println("Lowered");}
  Serial.println("Setting hike angle");
  set_flag(servo_hike, condition5);
  
  delay(600000);
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


void set_flag(Servo &flag_servo, bool target_position) {
  // Get the angle I want to be at
  int target_angle;
  Serial.print("target_position is ");
  Serial.println(target_position);
  if (target_position) {
    target_angle = highangle;
  } else {
    target_angle = lowangle;
  }

  // Retrieve the servos current position
  int current_angle = flag_servo.read();

  // Move by 1 degree every 20ms
  if (current_angle < target_angle) {
    Serial.println("Raising flag");
    while (current_angle < target_angle) {
      current_angle++;
      flag_servo.write(current_angle);
      delay(20);
    }
  } else {
    Serial.println("Lowering flag");
    while (current_angle > target_angle) {
      current_angle--;
      flag_servo.write(current_angle);
      delay(20);
    }
  }

  Serial.println("");
}


bool evalFirepit() {
  // above 10 degrees from 5pm - 12pm, with no precipitation.
  Serial.println("\n\nChecking for firepit flag");

  bool is_OK = true;
  bool any_valid_times = false;
  bool is_valid_time;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int currDay = day();
  int lowtime = 10 - daylight_savings;
  int hightime = 23 - daylight_savings;
  Serial.print("lowtime: ");
  Serial.println(lowtime);
  Serial.print("hightime: ");
  Serial.println(hightime);
  Serial.print("Current day is ");
  Serial.println(currDay);

  // Temperature must be HIGHER than this. Units are kelvin.
  int minTemp = 273 + 10;
  
  Serial.print("Temperature threshold is ");
  Serial.println(minTemp);
  Serial.println("\n");

  int clear_skies = 700;

  // Check the future
  for (int i=0; i<N_forecasts; i++) {
    JsonObject this_forecast = forecast[i];
    JsonObject thisWeather = this_forecast["weather"][0];
    
    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for the %d day of the month, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = ((tm.Hour > lowtime) and (tm.Hour < hightime) and (tm.Day == currDay));

    Serial.print("Is this a valid time? -> ");
    Serial.println(is_valid_time);
    
    if (is_valid_time) {
      // Check if the temperature is high enough to be comfy
      
      int forecastTemp = this_forecast["feels_like"];
      int temp_OK = (forecastTemp > minTemp);
      
      Serial.print("For timestamp ");
      Serial.print(dt);
      Serial.println(" is the weather good?");
      Serial.print("Temperature feels like ");
      Serial.println(forecastTemp);
      Serial.print("Is this ok? ");
      if (temp_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      
      int weatherCode = thisWeather["id"];
      bool weather_OK = (weatherCode > clear_skies);

      Serial.print("The weather code is ");
      Serial.println(weatherCode);
      Serial.print("Is this ok? ");
      if (weather_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      is_OK = (temp_OK and weather_OK);
      Serial.println("---------------------------------------");
    }
  }

  Serial.println("\n\nNow checking history of today...\n\n");

  // Check the past for decent weather
  for (int i=0; i<N_yesterday; i++) {
    JsonObject this_forecast = todays_history[i];
    JsonObject thisWeather = this_forecast["weather"][0];
    
    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for day %d, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = ((tm.Hour > lowtime) and (tm.Hour < hightime) and (tm.Day == currDay));

    Serial.print("Is this a valid time? ");
    Serial.print(" -> ");
    Serial.println(is_valid_time);
    
    if (is_valid_time) {
      // Check if the temperature is high enough to be comfy
      
      int forecastTemp = this_forecast["feels_like"];
      int temp_OK = (forecastTemp > minTemp);
      
      Serial.print("For timestamp ");
      Serial.print(dt);
      Serial.println(" is the weather good?");
      Serial.print("Temperature feels like ");
      Serial.println(forecastTemp);
      Serial.print("Is this ok? ");
      if (temp_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      
      int weatherCode = thisWeather["id"];
      bool weather_OK = (weatherCode > clear_skies);

      Serial.print("The weather code is ");
      Serial.println(weatherCode);
      Serial.print("Is this ok? ");
      if (weather_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      is_OK = (temp_OK and weather_OK);
      Serial.println("---------------------------------------");
    }
  }
  
  return true;
}


bool evalUmbrella() {
  // if there's rain between 7am and 11am, or between 3pm and 7pm.
  Serial.println("\n\nChecking for umbrella flag");

  bool is_OK = true;
  bool any_valid_times = false;
  bool is_valid_time;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int currDay = day();
  int lowtime1 = 7 - daylight_savings;
  int hightime1 = 11 - daylight_savings;
  int lowtime2 = 15 - daylight_savings;
  int hightime2 = 19 - daylight_savings;
  Serial.print("lowtime1: ");
  Serial.println(lowtime1);
  Serial.print("hightime1: ");
  Serial.println(hightime1);
  Serial.print("lowtime2: ");
  Serial.println(lowtime2);
  Serial.print("hightime2: ");
  Serial.println(hightime2);
  Serial.print("Current day is ");
  Serial.println(currDay);

  int clear_skies = 600;

  // Check the future
  for (int i=0; i<N_forecasts; i++) {
    JsonObject this_forecast = forecast[i];
    
    JsonObject thisWeather = this_forecast["weather"][0];
    
    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for the %d day of the month, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = (
//     (tm.Day == currDay) and
      (tm.Hour > lowtime1) and (tm.Hour < hightime1) or 
      (tm.Hour > lowtime2) and (tm.Hour < hightime2)
    );

    Serial.print("Is this a valid time? -> ");
    Serial.println(is_valid_time);
    
    if (is_valid_time) {      
      int weatherCode = thisWeather["id"];
      bool weather_OK = (weatherCode < clear_skies);

      Serial.print("The weather code is ");
      Serial.println(weatherCode);
      Serial.print("Is this raining? ");
      if (weather_OK) {
        Serial.println("YES");
        return true;
      } else {
        Serial.println("NO");
      }

    }
    Serial.println("---------------------------------------");
  }

  return false;
}


bool evalPicnic() {
  // Sunny on weekdays 4pm-6pm, or weekends all day (10am - 6pm
  Serial.println("\n\nChecking for picnic flag");

  bool is_OK = true;
  bool any_valid_times = false;
  bool is_valid_time;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int curr_weekday = weekday();
  int curr_day = day();

  // Set the time limits
  int lowtime;
  int hightime;
  if ((curr_weekday == 1) or (curr_weekday == 7)) {
    // It's a weekend! Check for sum all day (10am - 6pm)
    Serial.println("It's a weekend!");
    lowtime = 12 - daylight_savings;
    hightime = 18 - daylight_savings;
  } else {
    // It's a weekday
    Serial.println("It's a weekday!");
    lowtime = 16 - daylight_savings;
    hightime = 20 - daylight_savings;
  }

    
  // Temperature must be HIGHER than this. Units are kelvin.
  int minTemp = 273 + 18;

  int clear_skies = 700;
    
  Serial.print("Temperature threshold is ");
  Serial.println(minTemp);
  Serial.println("\n");

  // Check the future
  for (int i=0; i<N_forecasts; i++) {
    JsonObject this_forecast = forecast[i];
    JsonObject this_weather = this_forecast["weather"][0];

    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for the %d day of the month, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = ((tm.Hour >= lowtime) and (tm.Hour <= hightime) and (tm.Day == curr_day));

    if (is_valid_time) {
      // Check if the temperature is high enough to be comfy
      
      int forecastTemp = this_forecast["feels_like"];
      int temp_OK = (forecastTemp > minTemp);
      
      Serial.print("For timestamp ");
      Serial.print(dt);
      Serial.println(" is the weather good?");
      Serial.print("Temperature feels like ");
      Serial.println(forecastTemp);
      Serial.print("Is this ok? ");
      if (temp_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      
      int weather_code = this_weather["id"];
      bool weather_OK = (weather_code >= clear_skies);

      Serial.print("The weather code is ");
      Serial.println(weather_code);
      Serial.print("Is this ok? ");
      if (weather_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      is_OK = (temp_OK and weather_OK);
      Serial.println("---------------------------------------");
    }
  }

    
  Serial.println("\n\nNow checking history of today...\n\n");
  // Check the past for decent weather
  for (int i=0; i<N_yesterday; i++) {
    JsonObject this_forecast = todays_history[i];
    JsonObject this_weather = this_forecast["weather"][0];
    
    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for day %d, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = ((tm.Hour >= lowtime) and (tm.Hour <= hightime) and (tm.Day == curr_day));

    if (is_valid_time) {
      // Check if the temperature is high enough to be comfy
      
      int forecastTemp = this_forecast["feels_like"];
      int temp_OK = (forecastTemp > minTemp);
      
      Serial.print("For timestamp ");
      Serial.print(dt);
      Serial.println(" is the weather good?");
      Serial.print("Temperature feels like ");
      Serial.println(forecastTemp);
      Serial.print("Is this ok? ");
      if (temp_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      
      int weather_code = this_weather["id"];
      bool weather_OK = (weather_code >= 700);

      Serial.print("The weather code is ");
      Serial.println(weather_code);
      Serial.print("Is this ok? ");
      if (weather_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      is_OK = (temp_OK and weather_OK);
      Serial.println("---------------------------------------");
    }
  }
  
  return true;
}


bool evalStorm() {
  // Check if there's gonna be a thunderstorm today, at any time in the future

  // Weather codes less than this are thunderstorms
  int storm_code = 300;

  for (int i=0; i<N_forecasts; i++) {
    JsonObject this_forecast = forecast[i];
    JsonObject this_weather = this_forecast["weather"][0];

    int weather_code = this_weather["id"];

    // Some reporting
    int dt = this_forecast["dt"];
    tmElements_t tm;
    breakTime(dt, tm);
    char buf[100];
    sprintf(buf, "The weather code is %d at day %d and hour %d", weather_code, tm.Day, tm.Hour);
    Serial.println(buf);
    
    if (weather_code < storm_code) {
      return true;
    }
  }

  return false;
}


bool evalHike() {
  // above 10 degrees from 5pm - 12pm, with no precipitation.
  Serial.println("\n\nChecking for hiking flag");

  bool is_OK = true;
  bool any_valid_times = false;
  bool is_valid_time;
  
  // I just need this temporary variable
  int dt;
  
  // Only examine data in this time range
  int currDay = day();
  int lowtime = 10 - daylight_savings;
  int hightime = 20 - daylight_savings;
  Serial.print("lowtime: ");
  Serial.println(lowtime);
  Serial.print("hightime: ");
  Serial.println(hightime);
  Serial.print("Current day is ");
  Serial.println(currDay);

  int clear_skies = 600;

  // Check the future
  for (int i=0; i<N_forecasts; i++) {
    JsonObject this_forecast = forecast[i];
    JsonObject thisWeather = this_forecast["weather"][0];
    
    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for the %d day of the month, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = ((tm.Hour > lowtime) and (tm.Hour < hightime) and (tm.Day == currDay));

    Serial.print("Is this a valid time? -> ");
    Serial.println(is_valid_time);
    
    if (is_valid_time) {
      // Check for rain     
      int weatherCode = thisWeather["id"];
      bool weather_OK = (weatherCode > clear_skies);

      Serial.print("The weather code is ");
      Serial.println(weatherCode);
      Serial.print("Is this ok? ");
      if (weather_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      is_OK = weather_OK;
      Serial.println("---------------------------------------");
    }
  }

  Serial.println("\n\nNow checking history of today...\n\n");

  // Check the past for decent weather
  for (int i=0; i<N_yesterday; i++) {
    JsonObject this_forecast = todays_history[i];
    JsonObject thisWeather = this_forecast["weather"][0];
    
    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for day %d, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = ((tm.Hour > lowtime) and (tm.Hour < hightime) and (tm.Day <= currDay));

    Serial.print("Is this a valid time? ");
    Serial.print(" -> ");
    Serial.println(is_valid_time);
    
    if (is_valid_time) {
      int weatherCode = thisWeather["id"];
      bool weather_OK = (weatherCode > clear_skies);

      Serial.print("The weather code is ");
      Serial.println(weatherCode);
      Serial.print("Is this ok? ");
      if (weather_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      is_OK = weather_OK;
      Serial.println("---------------------------------------");
    }
  }
  
  // Check the past for decent weather
  for (int i=0; i<N_yesterday; i++) {
    JsonObject this_forecast = yesterdays_history[i];
    JsonObject thisWeather = this_forecast["weather"][0];
    
    dt = this_forecast["dt"];

    tmElements_t tm;
    breakTime(dt, tm);

    char buf[100];
    sprintf(buf, "Looking at the forecast for day %d, hour %d", tm.Day, tm.Hour);
    Serial.println(buf);

    // Check that we're in the correct window
    is_valid_time = ((tm.Hour > lowtime) and (tm.Hour < hightime) and (tm.Day <= currDay));

    Serial.print("Is this a valid time? ");
    Serial.print(" -> ");
    Serial.println(is_valid_time);
    
    if (is_valid_time) {
      int weatherCode = thisWeather["id"];
      bool weather_OK = (weatherCode > clear_skies);

      Serial.print("The weather code is ");
      Serial.println(weatherCode);
      Serial.print("Is this ok? ");
      if (weather_OK) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
        return false;
      }

      is_OK = weather_OK;
      Serial.println("---------------------------------------");
    }
  }
  return true;
}


void getWeather() { 
  // Gets the weather forecast from OWM. Saves the hourly forecast to a global variable
  // https://openweathermap.org/api/one-call-api
  
  Serial.println("\nConnecting to OWM server..."); 
  json_buffer = httpGETRequest(OWM_url.c_str());
  
  DeserializationError error = deserializeJson(forecast_doc, json_buffer);
  forecast = forecast_doc["hourly"];

  int dt = forecast[0]["dt"];
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  } else {
    Serial.println("Successfully got the weather!\n");
  }
}


void getPastWeather() {
  // Get the weather history from OWM. Get the hourly forecast starting 24 hours ago

  Serial.println("\nGetting today's weather history from OWM server...");
  // Rewind 24 hours and get the 48 hour forecast
  int target_time = now();
  
  char url_buffer[250];
  sprintf(url_buffer, OWM_past_url.c_str(), target_time, API_KEY);
  Serial.println("Getting past weather from url:");
  Serial.println(url_buffer);
  
  json_buffer = httpGETRequest(url_buffer);
  
  DeserializationError error = deserializeJson(todays_history_doc, json_buffer);
  todays_history = todays_history_doc["hourly"];

  // OWM conveniently provides a daylight savings offset. Bonus, makes this code portable to other locations.
  int timezone_offset = todays_history_doc["timezone_offset"];
  Serial.print("Timeszone fooset is ");
  Serial.println(timezone_offset);
  daylight_savings = timezone_offset / 3600;

  char buf[100];
  sprintf(buf, "I will add %d hours to all my time constraints", daylight_savings);
  Serial.println(buf);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  } else {
    Serial.println("Successfully got the weather!\n");
  }


  Serial.println("\nGetting yesterday's weather history OWM server...");
  // Rewind 24 hours and get the 24 hour forecast
  target_time = now() - 1*24*60*60;
  
  sprintf(url_buffer, OWM_past_url.c_str(), target_time, API_KEY);
  Serial.println("Getting past weather from url:");
  Serial.println(url_buffer);
  
  json_buffer = httpGETRequest(url_buffer);
  
  error = deserializeJson(yesterdays_history_doc, json_buffer);
  yesterdays_history = yesterdays_history_doc["hourly"];

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


/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
