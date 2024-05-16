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

#define DS18B20_SENSORS 8

DHT_Unified dht(DHTPIN, DHTTYPE);

// wifi network setup
AsyncWebServer server(80);
const char *ssid = NETSSID;
const char *password = NETPASS;

// MQ-135
String ppmCO;
String ppmCO2;
String ppmAlcohol;
String ppmNH4;
String ppmAceton;
String ppmToulen;

// BMP180
String temperature;
String pressure;
String seaLevelPressure;
String altitude;

// DS18B20
float temperatures[DS18B20_SENSORS];

// DHT22
String dhtTemperature;
String humidity;

// connection status
String connectionPrompt;

void setup() {
    Serial.begin(9600);
    Serial.println(F("<.> Initializing the peripherals:"));

    pinMode(LED_BUILTIN, OUTPUT);

    // initiating the peripherals
    initOLED();
    initMQ135();
    initBMP180();
    initDS18B20();

    // starting the dht22 sensor
    Serial.println(F("<.> Setting up DHT22 temperature and humidity sensor:"));
    dht.begin();

    // web server starts only if the connection is established
    while (initConnection() == false) {
        Serial.println(F("<!> Retrying connection in 1 second..."));
        delay(1000);
    }

    initWebServer();
    blink();
}

bool initOLED() {
    Serial.println(F("<.> Setting up OLED display..."));
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        Serial.println(F("<-> success."));
        return true;
    } else {
        Serial.println(F("<!> failure."));
        return false; 
    } 
}

void initMQ135() {
    Serial.println(F("<.> Setting up MQ-135 gas sensor..."));
    MQ135.setRegressionMethod(1);
    MQ135.init();
    calibrateMQ135();
    Serial.println(F("<-> success."));
}

bool initBMP180() {
    Serial.println(F("<.> Setting up BMP180 temperature and pressure sensor..."));
    if (bmp.begin()) {
        Serial.println(F("<-> success."));
        return true;
    }
    else {
        Serial.println(F("<!> failure."));
        return false;
    }
}

void initDS18B20() {
    Serial.println(F("<.> Setting up DS18B20 temperature sensor..."));
    sensors.begin();
    int numberOfDevices = sensors.getDeviceCount();

    if (numberOfDevices == 0) {
        Serial.print(F("<!> Detected "));
        Serial.print(numberOfDevices);
        Serial.println(F(" DS18B20 sensors."));
        Serial.println(F("<-> failure "));
    } else if (numberOfDevices != 8) {
        Serial.print(F("<?> Detected unexpected number of sensors: "));
        Serial.print(numberOfDevices);
        Serial.println(F("."));
        Serial.println(F("<?> functionality uncertain."));
    } else {
        Serial.print(F("<.> Detected expected number of sensors: "));
        Serial.println(numberOfDevices);
        Serial.println(F("<-> success "));
    }
}

bool initConnection() {
    Serial.println(F("<.> Setting up the wireless connection..."));
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    Serial.print("<.> Connecting to " + WiFi.SSID() + " WiFi network");
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        // timeout after 10 seconds
        if (millis() - startTime >= 10000) {
            Serial.println();
            Serial.println(F("<!> failure - connection timed out."));
            return false;
        }
        Serial.print(F("."));
        delay(500);
    }

    Serial.println();
    Serial.print("<-> Connected to " + WiFi.SSID() + " at address: ");
    Serial.println(WiFi.localIP());
    connectionPrompt = "\n" + WiFi.SSID() + ": " + WiFi.localIP().toString();

    Serial.println(F("<-> success."));
    return true;
}

void initWebServer() {
    Serial.println(F("<.> Setting up the web server..."));
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
        json_response += "},";
        json_response += "\"ds18b20\":{";
        json_response += "\"temperature\":\"" + readAverageDS18B20() + "\"";
        json_response += "},";
        json_response += "\"dht22\":{";
        json_response += "\"temperature\":\"" + readDHT22("temperature") + "\",";
        json_response += "\"humidity\":\"" + readDHT22("humidity") + "\"";
        json_response += "}";
        json_response += "}";
        json_response += "}";

        request->send(200, "application/json", json_response);
    });

    server.begin();
    Serial.println(F("<-> Setting up finished. HTTP server is now online!"));
}

void loop() {

    readBMP180();
    readMQ135();
    readDS18B20();
    readDHT22();

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

void readDS18B20() {
    sensors.requestTemperatures();

    for (int i = 0; i < DS18B20_SENSORS; i++) {
        temperatures[i] = sensors.getTempCByIndex(i);
    }
}

void readDHT22() {
    sensors_event_t event;

    // get temperature from DHT22 sensor
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        Serial.println(F("<!> Error reading temperature using DHT22 sensor."));
    }
    else {
        dhtTemperature = event.temperature;
    }

    // get humidity from DHT22 sensor
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
        Serial.println(F("<!> Error reading humidity using DHT22 sensor."));
    }
    else {
        humidity = event.relative_humidity;
    }
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
        Serial.println(F("<!> failure."));
    }
    if(calcR0 == 0) {
        // analog to ground
        Serial.println(F("<!> failure."));
    }
}

String readDHT22(String param) {
    if (param == "temperature")
        return dhtTemperature;
    else if (param == "humidity")
        return humidity;
    return "no data";
}

String readAverageDS18B20() {
    float average = 0;
    String output = "";
    for (int i = 0; i < DS18B20_SENSORS; i++) {
        average += temperatures[i];
    }
    average = average / DS18B20_SENSORS;
    return output + average;
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
    for (int i = 0; i < 4; i++) {
        digitalWrite(BUILTIN_LED, LOW); // Turn the LED on
        delay(500); // Wait for 500 milliseconds
        digitalWrite(BUILTIN_LED, HIGH); // Turn the LED off
        delay(500); // Wait for 500 milliseconds
    }
}

// vim: filetype=arduino:expandtab:shiftwidth=4:tabstop=4:softtabstop=2:textwidth=80:expandtab
