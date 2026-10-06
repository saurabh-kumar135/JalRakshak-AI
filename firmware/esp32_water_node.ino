/**
 * JalRakshak AI (जल रक्षक AI) - Edge Sensing Node Firmware
 * Platform: ESP32 NodeMCU-32S
 * Sensors: Analog pH, TS-300B Optical Turbidity, Analog TDS, DS18B20 Temp
 * Author: Saurabh Kumar (RVIT Bijnor)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* SERVER_ENDPOINT = "http://192.168.1.100:5000/api/telemetry";

// Hardware Pin Definitions
#define PH_PIN 34
#define TURBIDITY_PIN 35
#define TDS_PIN 32
#define ONE_WIRE_BUS 4

// Sensor Drivers
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

// Device Metadata
const char* DEVICE_ID = "JAL-RAKSHAK-UP-001";
const char* LOCATION_ID = "Bijnor_Ward_4";

// Calibration Constants
const float PH_SLOPE = -5.70;
const float PH_INTERCEPT = 21.34;
const float VREF = 3.3;
const int SAMPLES_COUNT = 30;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- JalRakshak AI Edge Node Initializing ---");

  tempSensors.begin();
  analogReadResolution(12);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi Failed. Running in standalone measurement mode.");
  }
}

float readMedianVoltage(int pin) {
  int samples[SAMPLES_COUNT];
  for (int i = 0; i < SAMPLES_COUNT; i++) {
    samples[i] = analogRead(pin);
    delay(10);
  }
  // Bubble sort for median filter
  for (int i = 0; i < SAMPLES_COUNT - 1; i++) {
    for (int j = i + 1; j < SAMPLES_COUNT; j++) {
      if (samples[i] > samples[j]) {
        int temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }
  int medianRaw = samples[SAMPLES_COUNT / 2];
  return (float)medianRaw * (VREF / 4095.0);
}

float calculateTemperature() {
  tempSensors.requestTemperatures();
  float tempC = tempSensors.getTempCByIndex(0);
  if (tempC == -127.0) tempC = 25.0; // Fallback to standard room temperature
  return tempC;
}

float calculatePH(float voltage) {
  return (PH_SLOPE * voltage) + PH_INTERCEPT;
}

float calculateTurbidity(float voltage) {
  // Transfer curve for TS-300B optical sensor
  float turbidity = -1120.4 * (voltage * voltage) + 5742.3 * voltage - 4352.9;
  if (turbidity < 0) turbidity = 0;
  return turbidity;
}

float calculateTDS(float voltage, float tempC) {
  // Temperature compensation coefficient: 2% per degree C
  float compensationCoefficient = 1.0 + 0.02 * (tempC - 25.0);
  float compensationVoltage = voltage / compensationCoefficient;
  
  // Polynomial curve conversion to ppm
  float tdsValue = (133.42 * pow(compensationVoltage, 3) 
                    - 255.86 * pow(compensationVoltage, 2) 
                    + 857.39 * compensationVoltage) * 0.5;
  if (tdsValue < 0) tdsValue = 0;
  return tdsValue;
}

void loop() {
  float waterTemp = calculateTemperature();
  float phVoltage = readMedianVoltage(PH_PIN);
  float turbVoltage = readMedianVoltage(TURBIDITY_PIN);
  float tdsVoltage = readMedianVoltage(TDS_PIN);

  float phVal = calculatePH(phVoltage);
  float turbVal = calculateTurbidity(turbVoltage);
  float tdsVal = calculateTDS(tdsVoltage, waterTemp);

  Serial.println("\n--- Telemetry Snapshot ---");
  Serial.printf("Temperature: %.2f °C\n", waterTemp);
  Serial.printf("pH Level:    %.2f\n", phVal);
  Serial.printf("Turbidity:   %.2f NTU\n", turbVal);
  Serial.printf("TDS Level:   %.2f ppm\n", tdsVal);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["location_id"] = LOCATION_ID;
    doc["temperature_c"] = waterTemp;
    doc["ph"] = phVal;
    doc["turbidity_ntu"] = turbVal;
    doc["tds_ppm"] = tdsVal;

    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponseCode = http.POST(jsonString);
    Serial.printf("Server Response: %d\n", httpResponseCode);
    http.end();
  }

  delay(60000); // 1-minute telemetry cycle
}
