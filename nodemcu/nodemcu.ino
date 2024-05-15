#include <Wire.h>
#include <OneWire.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <MQUnifiedsensor.h>
#include <Adafruit_BMP085.h>
#include <DallasTemperature.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// definitions for MQ-135 gas sensor
#define BOARD              ("ESP8266")
#define PIN                (A0)
#define TYPE               ("MQ-135")
#define VOLTAGE_RESOLUTION (3.3)
#define ADC_BIT_RESOLUTION (10)
#define RATIOMQ135CLEANAIR (3.6)

// pin for the DS18B20 temperature sensor
#define ONE_WIRE_BUS 2

// DHT22 temperature and humidity sensor
#define DHTPIN 14
#define DHTTYPE DHT22

// header contains ssid and password to the wireless network
#include "secret.h"

Adafruit_SSD1306 display(128, 64, &Wire);
MQUnifiedsensor MQ135(BOARD, VOLTAGE_RESOLUTION, ADC_BIT_RESOLUTION, PIN, TYPE);
Adafruit_BMP085 bmp;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int numberOfDevices;

DHT_Unified dht(DHTPIN, DHTTYPE);

// wifi network setup
AsyncWebServer server(80);
const char *ssid = NETSSID;
const char *password = NETPASS;

// MQ-135 gas sensor variables
String ppmCO;
String ppmCO2;
String ppmAlcohol;
String ppmNH4;
String ppmAceton;
String ppmToulen;

// BMP180 temperature and pressure sensor variables
String temperature;
String pressure;
String seaLevelPressure;
String altitude;

// connection status
String connectionPrompt;

// TODO: if free add flags which will indicate if the sensor is connected
// and assign appropriate variables to the screen
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(9600);
    Serial.println("<.> Initializing the peripherals:");

    initOLED();
    initMQ135();
    initBMP180();

    // into separate function

    sensors.begin();
    // Grab a count of devices on the wire
    numberOfDevices = sensors.getDeviceCount();
    
    // locate devices on the bus
    Serial.print("<.> Locating DS18B20 sensors...");
    Serial.print("<.> Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");

    dht.begin();

    // web server starts only if the connection is established
    while (initConnection() == false) {
        Serial.println("<!> Retrying connection in 1 second...");
        delay(1000);
    }

    initWebServer();
}

void loop() {

    // into separate function
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        Serial.println(F("Error reading temperature!"));
    }
    else {
        Serial.print(F("Temperature: "));
        Serial.print(event.temperature);
        Serial.println(F("°C"));
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
        Serial.println(F("Error reading humidity!"));
    }
    else {
        Serial.print(F("Humidity: "));
        Serial.print(event.relative_humidity);
        Serial.println(F("%"));
    }

    readBMP180();
    readMQ135();
    displayData();
    delay(500);
}

void readBMP180() {
    // Read pressure and temperature directly into variables
    pressure = bmp.readPressure() / 100.0F;
    temperature = bmp.readTemperature();

    // Save the pressure at the sea level and convert to hPa
    float seaLevelPressurePascals = bmp.readSealevelPressure(150);
    seaLevelPressure = seaLevelPressurePascals / 100.0F;

    // Calculate altitude using previously read sea level pressure
    altitude = bmp.readAltitude(seaLevelPressurePascals);
}

void readMQ135() {
    MQ135.update();

    MQ135.setA(605.18); MQ135.setB(-3.937);
    ppmCO = MQ135.readSensor();

    MQ135.setA(110.47); MQ135.setB(-2.862);
    ppmCO2 = MQ135.readSensor() + 400;

    MQ135.setA(102.2 ); MQ135.setB(-2.473);
    ppmNH4 = MQ135.readSensor();

    MQ135.setA(77.255); MQ135.setB(-3.18);
    ppmAlcohol = MQ135.readSensor();

    MQ135.setA(44.947); MQ135.setB(-3.445);
    ppmToulen = MQ135.readSensor();

    MQ135.setA(34.668); MQ135.setB(-3.369);
    ppmAceton = MQ135.readSensor();
}

void displayData() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println();
    display.print(temperature);
    display.print("*C, ");
    display.print(pressure);
    display.println(" hPa");
    display.print("\nC02: ");
    display.print(ppmCO2);
    display.println();
    display.println();
    display.println(connectionPrompt);
    display.display();
}

bool initOLED() {
    Serial.println("<.> Setting up OLED display...");
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        Serial.println("<-> success.");
        return true;
    } else {
        Serial.println("<!> failure.");
        return false; 
    } 
}

void initMQ135() {
    Serial.println("<.> Setting up MQ-135 gas sensor...");
    MQ135.setRegressionMethod(1);
    MQ135.init();
    calibrateMQ135();
    Serial.println("<-> success.");
}

void calibrateMQ135() {
    float calcR0 = 0;

    for(int i = 1; i <= 10; i ++)
    {
        MQ135.update();
        calcR0 += MQ135.calibrate(RATIOMQ135CLEANAIR);
    }
    MQ135.setR0(calcR0/10);

    if(isinf(calcR0)) {
        // open circuit
        Serial.println("<!> failure.");
    }
    if(calcR0 == 0) {
        // analog to ground
        Serial.println("<!> failure.");
    }
}

bool initBMP180() {
    Serial.println("<.> Setting up BMP180 temperature and pressure sensor...");
    if (bmp.begin()) {
        Serial.println("<-> success.");
        return true;
    }
    else {
        Serial.println("<!> failure.");
        return false;
    }
}

bool initConnection() {
    Serial.println("<.> Setting up the wireless connection...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    Serial.print("<.> Connecting to " + WiFi.SSID() + " WiFi network");
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        // timeout after 10 seconds
        if (millis() - startTime >= 10000) {
            Serial.println();
            Serial.println("<!> failure - connection timed out.");
            connectionPrompt = "\nbrak polaczenia!";
            return false;
        }
        Serial.print(".");
        delay(500);
    }

    blink();
    Serial.println();
    Serial.print("<-> Connected to " + WiFi.SSID() + " at address: ");
    Serial.println(WiFi.localIP());
    connectionPrompt = "\n" + WiFi.SSID() + ": " + WiFi.localIP().toString();

    Serial.println("<-> success.");
    return true;
}

void initWebServer() {
    Serial.println("<.> Setting up the web server...");
    server.on("/circumstances", HTTP_GET, [](AsyncWebServerRequest *request){
        String json_response = "";
        json_response += "{\"nodemcu\":{";
        json_response += "\"mq135\":{";
        json_response += "\"co\":\"" + readMQ135("co") + "\",";
        json_response += "\"co2\":\"" + readMQ135("co2") + "\",";
        json_response += "\"alcohol\":\"" + readMQ135("alcohol") + "\",";
        json_response += "\"nh4\":\"" + readMQ135("nh4") + "\",";
        json_response += "\"aceton\":\"" + readMQ135("aceton") + "\",";
        json_response += "\"toulen\":\"" + readMQ135("toulen") + "\"";
        json_response += "},";
        json_response += "\"bmp180\":{";
        json_response += "\"temperature\":\"" + readBMP180("temperature") + "\",";
        json_response += "\"pressure\":\"" + readBMP180("pressure") + "\",";
        json_response += "\"seaLevelPressure\":\"" + readBMP180("sealevelpressure") + "\",";
        json_response += "\"altitude\":\"" + readBMP180("altitude") + "\"";
        json_response += "}";
        json_response += "}";
        json_response += "}";

        request->send(200, "application/json", json_response);
    });

    server.begin();
    Serial.println("<-> Setting up finished. HTTP server is now online!");
    blink();
}

String readMQ135(String molecule) {
    if (molecule == "co")
        return ppmCO;
    else if (molecule == "co2")
        return ppmCO2;
    else if (molecule == "alcohol")
        return ppmAlcohol;
    else if (molecule == "nh4")
        return ppmNH4;
    else if (molecule == "aceton")
        return ppmAceton;
    else if (molecule == "toulen")
        return ppmToulen;
    return "no data";
}

String readBMP180(String param) {
    if (param == "temperature")
        return temperature;
    else if (param == "pressure")
        return pressure;
    else if (param == "altitude")
        return altitude;
    else if (param == "sealevelpressure")
        return seaLevelPressure;
    return "no data";
}

/* Generates three quick blinks of the builtin led */
void blink() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(250);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(250);
    }
}

// vim: filetype=arduino:expandtab:shiftwidth=4:tabstop=4:softtabstop=2:textwidth=80:expandtab
