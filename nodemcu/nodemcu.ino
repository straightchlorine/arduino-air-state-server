#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <MQUnifiedsensor.h>

#define BOARD              ("ESP8266")
#define PIN                (A0)
#define TYPE               ("MQ-135")
#define VOLTAGE_RESOLUTION (3.3)
#define ADC_BIT_RESOLUTION (10)
#define RATIOMQ135CLEANAIR (3.6) 

Adafruit_SSD1306 display(128, 64, &Wire);
MQUnifiedsensor MQ135(BOARD, VOLTAGE_RESOLUTION, ADC_BIT_RESOLUTION, PIN, TYPE);

void setup() {
    // serial connection
    Serial.begin(9600);

    // set up the OLED display
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setTextSize(2);
    display.setTextColor(WHITE);

    // set up the MQ-135 sensor
    MQ135.setRegressionMethod(1);
    MQ135.setA(110.47); MQ135.setB(-2.862); // a and b for C02 calculation
    MQ135.init(); 
    calibrate();
}

/*
    Calibrates the MQ-135 sensor. It will calculate the R0 value and store it in the object.
*/
void calibrate() {
    Serial.print("calibration...");
    float calcR0 = 0;

    for(int i = 1; i <= 10; i ++)
    {
        MQ135.update();
        calcR0 += MQ135.calibrate(RATIOMQ135CLEANAIR);
        Serial.print(".");
    }
    MQ135.setR0(calcR0/10);
    Serial.println("  done!.");
    
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
    displayData(MQ135.readSensor() + 400);
    delay(500); //Sampling frequency
}

void displayData(float read) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(read);
    display.display();
}

// vim: filetype=arduino:expandtab:shiftwidth=4:tabstop=4:softtabstop=2:textwidth=80:expandtab
