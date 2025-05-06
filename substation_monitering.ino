#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ZMPT101B.h>
#include "DHT.h"
#include "ACS712.h"

// LCD and sensor initialization
LiquidCrystal_I2C lcd(0x27, 16, 2); // Ensure the I2C address matches your module
#define SENSITIVITY 500.0f
ZMPT101B voltageSensor(A1, 50.0);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

ACS712 sensorTotal(ACS712_05B, A0);  // Itotal sensor connected to A0
ACS712 sensor1(ACS712_05B, A3);      // I1 sensor connected to A3
ACS712 sensor2(ACS712_05B, A2);      // I2 sensor connected to A2

bool isOverloaded = false;           // State to track overload condition
unsigned long lastCheck = 0;         // Timer for debouncing
const unsigned long debounceDelay = 3000; // 3-second delay for stable readings

const float overloadThreshold = 1.0; // Overload threshold
const float resetThreshold = 0.25;    // Lower threshold to reset relay

float currentReadingTotal = 0.0;    // Smoothed current reading for Itotal
float previousReadingTotal = 0.0;   // For smoothing calculation for Itotal

float currentReading1 = 0.0;        // Smoothed current reading for I1
float previousReading1 = 0.0;       // For smoothing calculation for I1

float currentReading2 = 0.0;        // Smoothed current reading for I2
float previousReading2 = 0.0;       // For smoothing calculation for I2

void setup() {
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  pinMode(3, OUTPUT);         // Relay connected to pin 3
  digitalWrite(3, HIGH);      // Relay ON by default (HIGH = connected, LOW = cutoff)

  Serial.begin(115200);
  voltageSensor.setSensitivity(SENSITIVITY);
  dht.begin();                // Initialize the DHT sensor

  // Calibrate the ACS712 current sensors
  sensorTotal.calibrate();
  sensor1.calibrate();
  sensor2.calibrate();
}

void loop() {
  unsigned long currentMillis = millis(); // Track elapsed time

  // Read and smooth current sensor values for Itotal
  float rawCurrentTotal = sensorTotal.getCurrentAC();
  if (rawCurrentTotal <= 0.20) {
    rawCurrentTotal = 0; // Ignore noise
  }
  currentReadingTotal = (0.6 * previousReadingTotal) + (0.4 * rawCurrentTotal);  // 90% smoothing factor
  previousReadingTotal = currentReadingTotal;

  // Read and smooth current sensor values for I1
  float rawCurrent1 = sensor1.getCurrentAC();
  if (rawCurrent1 <= 0.20) {
    rawCurrent1 = 0; // Ignore noise
  }
  currentReading1 = (0.6 * previousReading1) + (0.4 * rawCurrent1);
  previousReading1 = currentReading1;

  // Read and smooth current sensor values for I2
  float rawCurrent2 = sensor2.getCurrentAC();
  if (rawCurrent2 <= 0.20) {
    rawCurrent2 = 0; // Ignore noise
  }
  currentReading2 = (0.6 * previousReading2) + (0.4 * rawCurrent2);
  previousReading2 = currentReading2;

  float voltage = voltageSensor.getRmsVoltage();
  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();

  // Serial output for communication
  Serial.print("<");
  Serial.print("10.2");                   // Error handling code
  Serial.print(",");
  Serial.print(temperatureC);             // Temperature
  Serial.print(",");
  Serial.print(humidity);                 // Humidity
  Serial.print(",");
  Serial.print(voltage);                  // Voltage
  Serial.print(",");
  Serial.print(currentReadingTotal);      // Itotal
  Serial.print(",");
  Serial.print(currentReading1);          // I1
  Serial.print(",");
  Serial.print(currentReading2);          // I2
  Serial.println(">");

  // Display values on LCD
  static bool showValues = true;          // Toggle display flag
  lcd.clear();

  if (showValues) {
    // Display voltage, temperature, and humidity
    lcd.setCursor(0, 0);
    lcd.print("V:");
    lcd.print(voltage, 2);
    lcd.setCursor(8, 0);
    lcd.print(",T:");
    lcd.print(temperatureC, 1);

    lcd.setCursor(0, 1);
    lcd.print("H:");
    lcd.print(humidity, 1);
  } else {
    // Display Itotal, I1, and I2
    lcd.setCursor(0, 0);
    lcd.print("Itot:");
    lcd.print(currentReadingTotal, 2);

    lcd.setCursor(0, 1);
    lcd.print("I1:");
    lcd.print(currentReading1, 2);
    lcd.setCursor(8, 1);
    lcd.print("I2:");
    lcd.print(currentReading2, 2);
  }

  // Add delay between changing values on LCD
  delay(2000);                // 2 seconds delay
  showValues = !showValues;   // Toggle display flag

  // Overload detection with smoothing and hysteresis
  if (currentReadingTotal > overloadThreshold && !isOverloaded) {
    if (currentMillis - lastCheck > debounceDelay) { // Check if debounce delay has passed
      isOverloaded = true;       // Latch the overloaded state
      digitalWrite(3, LOW);      // Turn off relay
      lcd.setCursor(14, 1);      // Display "OL" on the LCD
      lcd.print("OL");
      lastCheck = currentMillis; // Reset debounce timer
    }
  }

  // Reset overload condition if current falls below reset threshold
  if (currentReadingTotal <= resetThreshold && isOverloaded) {
    if (currentMillis - lastCheck > debounceDelay) { // Apply debounce delay
      isOverloaded = false;      // Reset overload state
      digitalWrite(3, HIGH);     // Turn relay back on
      lastCheck = currentMillis; // Reset debounce timer
    }
  }

  delay(500); // Slow down the loop to reduce fluctuation
}
