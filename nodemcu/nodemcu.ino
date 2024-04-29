#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <MQUnifiedsensor.h>
#include <Adafruit_BMP085.h>

#define BOARD              ("ESP8266")
#define PIN                (A0)
#define TYPE               ("MQ-135")
#define VOLTAGE_RESOLUTION (3.3)
#define ADC_BIT_RESOLUTION (10)
#define RATIOMQ135CLEANAIR (3.6) 

#define NETSSID ("eps-iot")
#define NETPASS ("15u`YN2_^hV>")

Adafruit_SSD1306 display(128, 64, &Wire);
MQUnifiedsensor MQ135(BOARD, VOLTAGE_RESOLUTION, ADC_BIT_RESOLUTION, PIN, TYPE);
Adafruit_BMP085 bmp;

// wifi setup
const char *ssid = NETSSID;
const char *password = NETPASS;

AsyncWebServer server(80);

IPAddress localIP(192, 168, 1, 107);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

String ppm;
String temperature;
String pressure;
String altitude;

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;

void setup() {
    // serial connection
    Serial.begin(9600);

    // set up the OLED display
    Serial.println();
    Serial.println();
    Serial.println("<...> Setting up OLED display...");
    initOLED();

    // set up the MQ-135 gas sensor
    Serial.println("<...> Setting up MQ-135 gas sensor...");
    initMQ135();

    // set up BMP180 sensor
    Serial.println("<...> Setting up BMP180 temperature and pressure sensor...");
    initBMP180();

    // set up ESP8266 as an access point
    Serial.println("<...> Setting up the access point...");
    initAP();

    // set up the web server
    Serial.println("<...> Setting up the web server...");
    initWebServer();
}

void initOLED() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setTextSize(1);
    display.setTextColor(WHITE);
}

void initMQ135() {
    MQ135.setRegressionMethod(1);
    MQ135.setA(110.47); MQ135.setB(-2.862);
    MQ135.init(); 
    calibrate();
}

void initBMP180() {
    if (!bmp.begin()) {
        Serial.println("<!> Could not find a valid sensor, check wiring!");
        while (1) {}
    }
}

void initAP() {
    WiFi.softAP(ssid, password);
    Serial.print("<.> Access point running on address: ");
    Serial.println(WiFi.softAPIP());
}

void initWebServer() {
    server.on("/circumstances", HTTP_GET, [](AsyncWebServerRequest *request){
        String json_response = 
        "{\"nodemcu\":{\"mq135\":{\"ppm\":\"" + String(readPPM().c_str()) 
        + "\"},\"bmp180\":{\"temperature\":\"" + String(readTemperature().c_str()) +
        + "\",\"pressure\":\""+ String(readPressure().c_str()) 
        + "\",\"altitude\":\"" + String(readAltitude().c_str()) + "\"}}}";

        request->send(200, "application/json", json_response);
        Serial.println("<^> GET /circumstance");
    });

    // Start server
    server.begin();
    Serial.println("<.> HTTP server started.");
}

/*
    Read the MQ-135 sensor and returns the PPM value.
*/
String readPPM() {
    return ppm;
}

/*
    Read BMP180 sensor and returns the temperature value.
*/
String readTemperature() {
    return temperature;
}


/*
    Read BMP180 sensor and returns the pressure value.
*/
String readPressure() {
    return pressure;
}

/*
    Read BMP180 sensor and returns the altitude value
    for standard sea level pressure 1013.25hPa.
*/
String readAltitude() {
    return altitude;
}

/*
    Calibrates the MQ-135 sensor. It will calculate the R0 value and store it in the object.
*/
void calibrate() {
    float calcR0 = 0;

    for(int i = 1; i <= 10; i ++)
    {
        MQ135.update();
        calcR0 += MQ135.calibrate(RATIOMQ135CLEANAIR);
    }
    MQ135.setR0(calcR0/10);
    
    if(isinf(calcR0)) {
        Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); 
        while(1);
    }
    if(calcR0 == 0) {
        Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply");
        while(1);
    }
}

void loop() {
    MQ135.update(); // Update data, the arduino will read the voltage from the analog pin

    ppm = MQ135.readSensor() + 400;
    temperature = bmp.readTemperature();
    pressure = bmp.readPressure() / 100;
    altitude = bmp.readAltitude(101325);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("\nCO2: ");
    display.print(ppm);
    display.print(" ppm");
    display.print("\n\nTemperatura: ");
    display.print(temperature);
    display.print(" *C");
    display.print("\n\nCisinienie: ");
    display.print(pressure);
    display.print(" hPa");
    display.display();

    delay(500); //Sampling frequency
}

// vim: filetype=arduino:expandtab:shiftwidth=4:tabstop=4:softtabstop=2:textwidth=80:expandtab
