/* Count birds entering and leaving a birdhouse

   Bird House Counter

   Paul Cavill
   January 2021

   This is a file file of firmware for the WEMOS D1 Mini, ESP8266 WiFi device.
   It counts activations of an infrared sensor placed on the entrance to a birdbox with a view to determining
   activity.  It records count and time data locally and periodically transmits this data via WiFi for remote 
   display on a Blynk app. 

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

*/

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>
#include "AUTH_TOKEN.h"
#include <ArduinoOTA.h>
#include <WiFiManager.h>


const int ksampleInterval = 500; // 0.5 second (Milliseconds)
const int ksampleDuration = 5; // 5 Milliseconds
const int kobjectDetectedDuration = 1000; // Duration an object detected to register as a count. 1 second (Milliseconds)
const int ksensorThreshold = 750; // Sensor value under which an object is deemed detected
const int ksensor = A0; // Analog Sensor
const int kirLed = D8;  // Infrared LED power
// const int kNtpPacketSize = 48;  // NTP time stamp is in the first 48 bytes of the message

BlynkTimer SensorTimer; // Timer - Check sensor at defined intervals
int SensorValue; // Value read from the IR Sensor

// --------------------------------------------------------------------------------------------------------------------------------------

// Main Setup code
void setup() {

  Serial.begin(115200);

  WiFiManager wifiManager;

  wifiManager.setTimeout(120);  // Set the timeout for WiFi connection (2 minutes - Seconds)

  if (!wifiManager.autoConnect("AutoConnectAP")) {
    // Timed out - reset and try again
    ESP.reset();
    delay(5000);
  }

  pinMode(ksensor, INPUT);
  pinMode(kirLed, OUTPUT);

  digitalWrite(kirLed, LOW);  // turn on pull-down resistor

  // Blynk.config(auth);
  Blynk.config(BLYNK_AUTH_TOKEN);

  SensorTimer.setInterval(ksampleInterval, MonitorBox); // Monitor birdbox entrance

  ArduinoOTA.setHostname("birdbox-counter");
  // No authentication by default
  ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}



// Monitor the sensor
void MonitorBox() {
  digitalWrite(kirLed, HIGH);
  delay(2); // Short duration for light to stabalise
  SensorValue = analogRead(ksensor);
  digitalWrite(kirLed, LOW);
          Serial.print("The sensor is reading ");
        // Serial.print(SensorValue);

  if (SensorValue < ksensorThreshold){
    LogActivity(SensorValue);
  }
}
  void LogActivity(int SensorValue){
    
  }

// Main Loop -------------------------------------------------------------------------------------------------
void loop() {

  ArduinoOTA.handle();
  Blynk.run();
  SensorTimer.run();

}