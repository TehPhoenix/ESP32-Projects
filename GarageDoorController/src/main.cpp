/* PC-RGC Firmware

   Remote Garage Control

   Paul Cavill
   February 2017

   This is a file of firmware for the WEMOS D1 Mini, ESP8266 WiFi device.
   It enables remote (internet) nonitoring and control of an electrically operated garage door.

  August 2020
  * Code restructured
  * Blynk Authentication Code moved to external .h file
  
  * August 2020  V2.0
  * Force update of door status at the time of activation instead of waiting for the periodic update.
  * Add the duration that the door has not been closed to the notification.

*/

/* Comment this out to disable prints and save space */
// #define BLYNK_PRINT Serial
#include "AUTH_TOKEN.h" // Blink Authorisation Code
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>

const int kMeasuurementFrequency = 5000; // 5 seconds (Milliseconds)
const int kFiveMins = 300000; // Length of time door to be allowed open befor notification sent.  5 minutes (Milliseconds)
const int kButtonDuration = 1000; // Milliseconds the door activation buttin is HIGH
const int kActivatePin = D1; // Activate the door
const int kClosedPin = D8; // Door closed
const int kOpenPin = D4; // Door open (Optional)
const int kEchoPin = D6; // Ultrasound Receiver
const int kTrigPin = D7; // Ultrasound Trigger
const int kDoorThreshold = 25; // Door Threshold (An intermediate distance between the sensor and the door and the sensor and the top of the car in cm)
const int kFloorThreshold = 180; // Floor Threshold (An intermediate distance between the sensor and the top of the car and the sensor and the floor in cm)
// const int kNtpPacketSize = 48;  // NTP time stamp is in the first 48 bytes of the message

int ObjectDistance; // Measured ultrasonic distance
int DoorStatusCode = 0; // DoorStatus
long DoorOpenedTime = 0; // Time the door has been monitored as open
bool NewButtonPress = false; // True = a new door activation request
bool InitialisationComplete = false; // True = Initialisation sequence complete
int IntervalCount = 0; // Number of five minute intervals that the door has been open
String Occupancy; // Garage Occupancy

// Define non-standard functions

void ActivateDoor();
void MonitorDoor();
void CalcDistance();
void InitialiseDoor();
void MonitorDoor(int);
void DetermineDoorStatus(int);
void DetermineOccupancy(int);


BlynkTimer UsTimer; // Timer - To set the delay between door status measurements
BlynkTimer OpnTimer; // Timer - Send "Door Open" notifications at this interval

String DoorStatus[] = {
    "*** UNKNOWN - ASSUME OPEN ***", // 0
    "OPEN", // 1
    "CLOSED", // 2
    "CLOSING", // 3
    "OPENING", // 4
    "STOPPED WHILE CLOSING", // 5
    "STOPPED WHILE OPENING", // 6
    "INITIALISING" // 7
};

// --------------------------------------------------------------------------------------------------------------------------------------

// Door activation request on Blynk app and not already processing a button press
BLYNK_WRITE(V0)
{
    if (param.asInt() == 1 && !NewButtonPress) {
        ActivateDoor();
        NewButtonPress = true;
        MonitorDoor();
    }
}

// Activete the physical door activation relay for the time set in configuration
void ActivateDoor()
{
    digitalWrite(kActivatePin, HIGH);
    delay(kButtonDuration);
    Blynk.virtualWrite(V0, LOW); // Set the activation button off in the Blynk app
    digitalWrite(kActivatePin, LOW);
}

// Main Setup code
void setup()
{

    Serial.begin(115200);

    WiFiManager wifiManager;

    wifiManager.setTimeout(120); // Set the timeout for WiFi connection (2 minutes - Seconds)

    if (!wifiManager.autoConnect("AutoConnectAP")) {
        // Timed out - reset and try again
        ESP.reset();
        delay(5000);
    }

    pinMode(kTrigPin, OUTPUT);
    pinMode(kEchoPin, INPUT);
    pinMode(kClosedPin, INPUT);
    pinMode(kActivatePin, OUTPUT);

    digitalWrite(kClosedPin, LOW); // turn on pull-down resistor

    // Blynk.config(auth);
    Blynk.config(BLYNK_AUTH_TOKEN);

    UsTimer.setInterval(kMeasuurementFrequency, MonitorDoor); // Monitor the status of the door

    ArduinoOTA.setHostname("door-controller");
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
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });

    ArduinoOTA.begin();

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// Monitor the status of the door
void MonitorDoor()
{
    if (!InitialisationComplete) { // Initialisation cycle not executed
        InitialiseDoor();
        InitialisationComplete = true;
    }

    CalcDistance();
    DetermineDoorStatus(DoorStatusCode);
    DetermineOccupancy(DoorStatusCode);
}

// Ensure the door returns to the closed status on startup
void InitialiseDoor()
{
    Blynk.virtualWrite(V1, "INITIALISING"); // Show status as Initialising on the app
    Blynk.virtualWrite(V2, " "); // Blank the occupancy status on the app
    Blynk.virtualWrite(V5, " "); // Blank the ultrasonic distance wiget
    Blynk.virtualWrite(V3, "V2.0");  // Display Version on app

    if (digitalRead(kClosedPin) != HIGH) // Door not closed
    {
        DoorStatusCode = 7; // Initialising
        Serial.print("Initialising");
        Serial.println();
        ActivateDoor(); // Set the door in motion
        do {
            delay(5000); // After 5 seconds check if door is open
            CalcDistance();
            if (ObjectDistance < kDoorThreshold) { // Door open
                Serial.print("Initialising - Door Open");
                Serial.println();
                ActivateDoor(); //  Door open, so activte to start closing
            }
        } while (digitalRead(kClosedPin) != HIGH); // While not closed
    }
    DoorStatusCode = 2; // Closed
}

//  Determine the door status and display on the app
void DetermineDoorStatus(int CurrentDoorStatus)
{
    if (ObjectDistance < kDoorThreshold || digitalRead(kClosedPin) == HIGH) { // Door Open or Closed
        Serial.print("1 ");
        Serial.print(ObjectDistance);
        Serial.println();
        if (ObjectDistance < kDoorThreshold && digitalRead(kClosedPin) == HIGH) {
            Serial.print("2 ");
            Serial.print(digitalRead(kClosedPin));
            Serial.println();
            DoorStatusCode = 0; // Error - Door both Open and Closed
        } else {
            Serial.print("3 ");
            Serial.print(digitalRead(kClosedPin));
            Serial.println();
            if (ObjectDistance < kDoorThreshold) {
                DoorStatusCode = 1; // Door is Open
            } else {
                Serial.print("4 ");
                Serial.print(digitalRead(kClosedPin));
                Serial.println();
                DoorStatusCode = 2; // Door is Closed
            }
        }
    } else {
        if (NewButtonPress) {
            Serial.print("5 ");
            Serial.print(CurrentDoorStatus);
            Serial.println();
            switch (CurrentDoorStatus) {
            case 1:
                DoorStatusCode = 3; // Door Closing
                break;
            case 2:
                DoorStatusCode = 4; // Door Opening
                break;
            case 3:
                DoorStatusCode = 5; // Door Stopped while Closing
                break;
            case 4:
                DoorStatusCode = 6; // Door Stopped while Opening
                break;
            case 5:
                DoorStatusCode = 4; // Door Opening
                break;
            case 6:
                DoorStatusCode = 3; // Door Closing
                break;
            }
            NewButtonPress = false; // Reset the button activation status.
        } else {
            if (CurrentDoorStatus == 1 || CurrentDoorStatus == 2) {
                DoorStatusCode = 0; // Error - Was Open or Closed, now neither Open nor Closed amd no actvation request was detected
            }
        }
    }

    Serial.print("Revised Door Status\t=\t"); // *********************************************************************************************************************
    Serial.print(DoorStatusCode);
    Serial.print(DoorStatus[DoorStatusCode]);
    Serial.println();
    Serial.println();
    Blynk.virtualWrite(V1, DoorStatus[DoorStatusCode]);

    // If the door is not closed send a warning notification at 5-minute intivals
    if (DoorStatusCode != 2) { // Door is not closed
        if (DoorOpenedTime == 0) {
            DoorOpenedTime = millis();
            IntervalCount = 0;
        } else {
            int CurrentInterval = (millis() - DoorOpenedTime) / kFiveMins;
             if (CurrentInterval > IntervalCount)
            {
                Blynk.notify("Minutes The Garage Door Has Now Been Open = " + (CurrentInterval * 5));

                IntervalCount = CurrentInterval;
            }
        }
    } else {
        DoorOpenedTime = 0;
        if (IntervalCount > 0) {
            Blynk.notify("The Garage Door Has Now Been Closed");
            IntervalCount = 0;
        }
    }
}

// If the door is not open, use the measured distance to determine if a vehicle is present and display the occupancy status on the app
void DetermineOccupancy(int CurrentDoorStatus)
{
    Occupancy = " ";
    if (CurrentDoorStatus != 1) {
        if (ObjectDistance > kDoorThreshold && ObjectDistance < kFloorThreshold) {
            Occupancy = "VEHICLE PRESENT";
        } else {
            Occupancy = "GARAGE VACANT";
        }
    }
    Serial.print("Occupancy = ");
    Serial.print(Occupancy);
    Serial.println();
    Serial.println();
    Blynk.virtualWrite(V2, Occupancy);
    Blynk.virtualWrite(V5, ObjectDistance);
}

//  Calculate the distance of the nearest object to the ultrasonic sensor
void CalcDistance()
{

    // Ensure the trigger pin is set LOW for a short while before initiating measurement
    digitalWrite(kTrigPin, LOW);
    delayMicroseconds(2);

    // Initiate distance measurement with a 10ms pulse on the trigger pin
    digitalWrite(kTrigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(kTrigPin, LOW);

    // Read the echo pin and calculate the distance in cm
    ObjectDistance = pulseIn(kEchoPin, HIGH) / 58.0; //The echo time is converted into cm

    Serial.print("Distance\t=\t"); // *****************************************************************************************************
    Serial.print(ObjectDistance);
    Serial.print("cm");
    Serial.println(); // ********************************************************************************************************************************
    Serial.println();

    Serial.print("Current Door Status\t=\t"); // ******************************************************************************************************************
    Serial.print(DoorStatusCode);
    Serial.print(DoorStatus[DoorStatusCode]);
    Serial.println();
    Serial.println();
}

// Main Loop -------------------------------------------------------------------------------------------------
void loop()
{

    ArduinoOTA.handle();
    Blynk.run();
    UsTimer.run();
}