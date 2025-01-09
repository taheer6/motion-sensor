#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Firebase Defenitions
#define DATABASE_URL "insert_database_url"
#define API_KEY "insert_api_key"
#define WIFI_SSID "insert-wifi_ssid"
#define WIFI_PASSWORD "insert_wifi_pass"

// Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;


// Pin Definitions
#define echoPin 5   // Echo connected to GPIO5
#define trigPin 4   // Trigger connected to GPIO4

// Variables
long duration;
int distance; // Distance in cm
bool motionDetected = false;
const int distanceValue = 95; // Max distance (cm) sensor detects

// Avoiding Firebase lag
bool prevMotionDetected = false;
unsigned long lastNoMotionUpdate = 0;
const int noMotionInterval = 9000;

// Cooldown logic
unsigned long lastMotionDetectedTime = 0;
const unsigned long cooldownPeriod = 5000;

void setup() {
  Serial.begin(11520);          // Start the serial monitor
  pinMode(trigPin, OUTPUT);    // Set trigPin as an output
  pinMode(echoPin, INPUT);     // Set echoPin as an input

  // Connect to WiFi
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  
  //WiFi connection status
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Assign API key and RTDB URL
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Sign up (anonymous in our case)
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  // Firebase Init
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

}

void loop() {

  // Trigger the ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo pin and calculate distance DISTANCE NOT USED
  duration = pulseIn(echoPin, HIGH);
  distance = duration / 58.2;  // Convert duration to distance in cm

  // Motion detection logic
  if ((millis() - lastMotionDetectedTime) >= cooldownPeriod) { 
    // Only update motionDetected if cooldown period has passed
    motionDetected = (distance > 0 && distance <= distanceValue);


    // Event Driven Firebase Updates
    if (motionDetected != prevMotionDetected) {
      if (Firebase.RTDB.setBool(&fbdo, "/motionDetected", motionDetected)) {
        Serial.println("Motion state updated to Firebase.");
        Serial.print("Motion Detected: ");
        Serial.println(motionDetected ? "TRUE" : "FALSE");
      } else {
        Serial.println("Failed to update motion state.");
        Serial.println("Reason: " + fbdo.errorReason());
      }

      // Update the previous state
      prevMotionDetected = motionDetected;

      // Reset the no-motion timer when motion is detected
      if (motionDetected) {
        lastNoMotionUpdate = millis();
        lastMotionDetectedTime = millis();
      }
    }
  }

  // Periodic no-motion reset
  if (!motionDetected && (millis() - lastNoMotionUpdate >= noMotionInterval)) {
    // Send a reset to Firebase after no-motion interval
    if (Firebase.RTDB.setBool(&fbdo, "/motionDetected", false)) {
      Serial.println("No-motion state reset to Firebase.");
    } else {
      Serial.println("Failed to reset no-motion state.");
      Serial.println("Reason: " + fbdo.errorReason());
    }

    // Reset the timer
    lastNoMotionUpdate = millis();
  }

  delay(200); // Local delay
}
