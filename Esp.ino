#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Firebase configuration
#define DATABASE_URL "https://testnode-5d196-default-rtdb.firebaseio.com/"
#define API_KEY "AIzaSyBYnicFYaNY8-vRucycGIkRnzA_tmyznWQ"

// Initialize Firebase and authentication
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String data = "";
bool signupOK = false;

// Wi-Fi credentials
const char* ssid = "asdf";
const char* password = "asdf@123";

// Relay pins (adjust according to your wiring)
#define RELAY1_PIN D2  // Relay 1 connected to D2
#define RELAY2_PIN D3  // Relay 2 connected to D3

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    // Initialize relay pins
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);

    // Initial relay states (adjust LOW/HIGH depending on your relay module)
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);

    // Wait for Wi-Fi connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    // Set up Firebase configuration
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    // Authenticate anonymously
    if (Firebase.signUp(&config, &auth, "", "")) {
        Serial.println("Firebase authentication successful!");
        signupOK = true;
    } else {
        Serial.printf("Firebase signup error: %s\n", config.signer.signupError.message.c_str());
    }

    config.token_status_callback = tokenStatusCallback; // For debugging token generation issues
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
}

void loop() {
    // Read and process incoming serial data
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '>') {  // End of data packet received
            Serial.println("\nReceived: " + data);

            // Remove the starting '<' character if present
            if (data.startsWith("<")) {
                data = data.substring(1);
            }

            // Parsing the data
            int comma1 = data.indexOf(',');
            int comma2 = data.indexOf(',', comma1 + 1);
            int comma3 = data.indexOf(',', comma2 + 1);
            int comma4 = data.indexOf(',', comma3 + 1);
            int comma5 = data.indexOf(',', comma4 + 1);
            int comma6 = data.indexOf(',', comma5 + 1);

            if (comma1 != -1 && comma2 != -1 && comma3 != -1 && comma4 != -1 && comma5 != -1 && comma6 != -1) {
                // Extracting individual values
                String errorHandlingCode = data.substring(0, comma1);
                float temp = data.substring(comma1 + 1, comma2).toFloat();
                float humidity = data.substring(comma2 + 1, comma3).toFloat();
                float voltage = data.substring(comma3 + 1, comma4).toFloat();
                float Itotal = data.substring(comma4 + 1, comma5).toFloat();
                float I1 = data.substring(comma5 + 1, comma6).toFloat();
                float I2 = data.substring(comma6 + 1).toFloat();

                // Debugging output
                Serial.println("Parsed Data:");
                Serial.println("Error Code: " + errorHandlingCode);
                Serial.println("Temperature: " + String(temp, 2));
                Serial.println("Humidity: " + String(humidity, 2));
                Serial.println("Voltage: " + String(voltage, 2));
                Serial.println("Itotal: " + String(Itotal, 2));
                Serial.println("I1: " + String(I1, 2));
                Serial.println("I2: " + String(I2, 2));

                // Update Firebase Realtime Database
                if (Firebase.ready() && signupOK) {
                    Firebase.RTDB.setFloat(&fbdo, "random/temperature", temp);
                    Firebase.RTDB.setFloat(&fbdo, "random/humidity", humidity);
                    Firebase.RTDB.setFloat(&fbdo, "random/voltage", voltage);
                    Firebase.RTDB.setFloat(&fbdo, "random/Itotal", Itotal);
                    Firebase.RTDB.setFloat(&fbdo, "random/I1", I1);
                    Firebase.RTDB.setFloat(&fbdo, "random/I2", I2);
                }

                // Control relays based on Firebase values
                if (Firebase.ready() && signupOK) {
                    bool s1_state = false;
                    bool s2_state = false;

                    // Get s1 state
                    if (Firebase.RTDB.getBool(&fbdo, "random/s1")) {
                        s1_state = fbdo.boolData();
                    } else {
                        Serial.println("Failed to get s1: " + fbdo.errorReason());
                    }

                    // Get s2 state
                    if (Firebase.RTDB.getBool(&fbdo, "random/s2")) {
                        s2_state = fbdo.boolData();
                    } else {
                        Serial.println("Failed to get s2: " + fbdo.errorReason());
                    }

                    // Update relay states
                    digitalWrite(RELAY1_PIN, s1_state ? HIGH : LOW);
                    digitalWrite(RELAY2_PIN, s2_state ? HIGH : LOW);

                    Serial.println("Relay1 State: " + String(s1_state));
                    Serial.println("Relay2 State: " + String(s2_state));
                }

                data = "";  // Clear buffer for the next set of data
            } else {
                Serial.println("Error: Data format mismatch");
                data = ""; // Clear buffer in case of format mismatch
            }
        } else {
            data += c;  // Append character to buffer
        }
    }
}
