/**
 * @Author: Daniel Murrieta M.Sc. Student FH Aachen - CIDESI <daniel>
 * @Date:   2021-01-03T00:56:38+01:00
 * @Email:  daniel.murrieta-alvarez@alumni.fh-aachen.de
 * @Blynk_App_Asistent: Rafael Aranda M.Sc Student FH Aachen- CIDESI <rafa>
 * @Date:   2021-27-04
 * @Email:  rafael.aranda@alumni.fh-aachen.de
 * @Filename: main.cpp
 * @Last modified by:   rafa
 * @Last modified time: 2021-27-04
 * @License: CC by-sa
 *
 */
/////////////################# headers #####################////////////////

#include <Arduino.h>
//#include <RTClib.h>
#include <Wire.h>
// #include <Adafruit_Sensor.h>
// #include "Adafruit_BME680.h"
#include <EEPROM.h>
#include "bsec.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include <stdlib.h> //For future use in weather forecasting
#include <stdio.h> //For future use in weather forecasting
#include <BlynkSimpleEsp32.h>

//#include "esp_task_wdt.h"
//#include "soc/timer_group_struct.h"
//#include "soc/timer_group_reg.h"
#include "WeatherStat_NTP.h"
#include "WeatherStat_BlynkApp.h"
#include "CO2_sensor.h"
//#include <DS3231.h>
/////////////################# directives #####################////////////////
#define Number_susc_sens 11
#define STATE_SAVE_PERIOD UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day
/////////////################# variables #####################////////////////
//char daysOfTheWeek[7][12] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const byte led_gpio = 32; // the PWM pin the LED is attached to
int PWMchannel = 0;
int i = 0;
char flag=0;
boolean flag_inter_loop=false;
//const char * intro = "Time[ms],r_t[°C],p[hPa],r_hum[%],gas[Ohm],IAQ,IAQacc,temp[°C],h[%],S_IAQ,CO2_equ,bre_VOC,Gas%";
const char * intro = "Time[ms],p[hPa],gas[Ohm],IAQ,IAQacc,temp[°C],h[%],S_IAQ,CO2_equ,bre_VOC,Gas%";
uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;
String output;
String tempe_web;
String iaq_web;
String press_web;
String hum_web;
/////////////################# functions #####################////////////////
void loop2(void *parameter);
void loop1(void *parameter);
void feedTheDog(void);
void hw_wdt_disable(void);
void checkIaqSensorStatus(void);
void loadState(void);
void updateState(void);
void myTimerEvent(void);
/////////////################# handlers definition #####################///////
/////////////################# Web Specific #####################///////
char *ssid = "FRITZ!Box 6591 Cable BE";          // replace with your SSID
char *password = "07225443701792235194";  // replace with your Password
//char *ssid = "Vodafone-FC6F = FRITZ!Box 6591 Cable BE";          // replace with your SSID/char *password = "07225443701792235194";  // replace with your Password";          // replace with your SSID
//char *password = "pW6298625330626571";  // replace with your Password
AsyncWebServer server(80);
/////////////################# NTP Client Specific #####################///////
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;  //Germany is GMT +1, expressed in seconds
const int daylightOffset_sec = 3600;   // There is one extra hour in DST
/////////////################# Blynk  config ###################///////
char auth[] = "DZJ5YStEBLYrOid9YuJH-Qm3QDPuy-Oe";    // token from the app
BlynkTimer timer;
                             // data request on app time
/////////////################# Blynk  variables ###################///////
iaq_level property; // define the structure property for iaq level of the app
/////////////################# CO2_sensor  config ###################///////

/////////////################# CO2_sensor ###################///////

/////////////################# Arduino sh%& #####################///////////////
TaskHandle_t Task2;
TaskHandle_t Task1;
//RTC_DS3231 rtc;
Bsec iaqSensor;
/////////////################# SETUP #####################////////////////
void setup() {
        Serial.begin(115200);
        Wire.begin();
/////RTC begin and check
        // if (!rtc.begin()) {
        //         Serial.println("Couldn't find RTC");
        //         while (1);
        // }
        // else{
        //         rtc.adjust(DateTime(__DATE__, __TIME__));
        //         Serial.println("RTC Found");
        // }

///SPIFSSS begin and check
        if(!SPIFFS.begin()) {
                Serial.println("An Error has occurred while mounting SPIFFS");
                return;
        }
        else Serial.println("SPIFFS Mounted correctly");

/////PWM Settings
        ledcAttachPin(led_gpio, PWMchannel); // assign a led pins to a channel
        ledcSetup(PWMchannel, 10000, 10); // 12 kHz PWM, 8-bit resolution

////// Wifi Configs
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED)
        {
                delay(1000);
                Serial.print("Connecting to WiFi..");
                Serial.print(".");
        }
        Serial.print("\nConnected to the WiFi network: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP address:");
        Serial.print(WiFi.localIP());
////// Blink begin config
        Blynk.begin(auth, ssid, password);
        timer.setInterval(1000L, myTimerEvent);
////// Config for readding CO2_sensor
        pinMode(MHZ19_PWM_PIN, INPUT);        //MHZ19 PWM Pin als Eingang konfigurieren
/////Initialize BME680 Sensor
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

/////Initialize BME680 Sensor
        iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
        output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
        Serial.println(output);
        //checkIaqSensorStatus();
        iaqSensor.setState(bsec_config_iaq);
        //checkIaqSensorStatus();
        loadState();

        bsec_virtual_sensor_t sensorList[Number_susc_sens] = {
                BSEC_OUTPUT_RAW_TEMPERATURE,
                BSEC_OUTPUT_RAW_PRESSURE,
                BSEC_OUTPUT_RAW_HUMIDITY,
                BSEC_OUTPUT_RAW_GAS,
                BSEC_OUTPUT_IAQ,
                BSEC_OUTPUT_STATIC_IAQ,
                BSEC_OUTPUT_CO2_EQUIVALENT,
                BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
                BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
                BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
                BSEC_OUTPUT_GAS_PERCENTAGE
        };
        iaqSensor.updateSubscription(sensorList, Number_susc_sens, BSEC_SAMPLE_RATE_LP);
        checkIaqSensorStatus();
        //Serial.println("------### Entrando en loops");
/////Enabling Multicore program execution
        xTaskCreatePinnedToCore(loop1,"Task_1",10000,NULL,1,&Task1,0);
        //delay(500);
        xTaskCreatePinnedToCore(loop2,"Task_2",10000,NULL,1,&Task2,1);
        delay(500);
        //hw_wdt_disable();

/////Handlers
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send(SPIFFS, "/index.html");
        });
        // send style.css file from SPIFFS
        server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send(SPIFFS, "/style.css", "text/css");
        });
        // send script.js file from SPIFFS
        server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send(SPIFFS, "/script.js", "text/javascript");
        });
        server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send_P(200, "text/plain", tempe_web.c_str());
        });
        // send BME680 humidity
        server.on("/iaq", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send_P(200, "text/plain", iaq_web.c_str());
        });
        server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send_P(200, "text/plain", press_web.c_str());
        });

        server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send_P(200, "text/plain", hum_web.c_str());
        });
/////Initialize Server
        server.begin(); // begin server at port 80
}
/////////////################# LOOP Executed in Core 0 #####################////////////////
void loop2(void *parameter) {
  #define IN_NUMBERS 1
  #define IN_LETTERS 2
        while(1) {
                yield();
                Blynk.run(); // run code of the app
                timer.run();  // establishes comunication to the app in a time interval to load sensor data
                //Serial.print("---### LOOP2");
                //Serial.println(i);//
                //unsigned long time_trigger = millis();
                if (iaqSensor.run()) {   // If new data is available
                        // tm timeinfo;
                        // if(!getLocalTime(&timeinfo)) {
                        //         Serial.println("Failed to obtain time");
                        //         output = String(time_trigger);
                        // }
                        // else
                        // {
                        //   output = String(timeinfo.tm_hour)+":";
                        //   output += String(timeinfo.tm_min)+":";
                        //   output += String(timeinfo.tm_sec);
                        // }
                        //output += ", " + String(iaqSensor.rawTemperature);
                        output = getZeit();
                        press_web = String(iaqSensor.pressure/100);
                        output += ", " + press_web;
                        //output += ", " + String(iaqSensor.rawHumidity);
                        output += ", " + String(iaqSensor.gasResistance);
                        iaq_web = String(iaqSensor.iaq);
                        output += ", " + iaq_web;
                        output += ", " + String(iaqSensor.iaqAccuracy);
                        tempe_web = String(iaqSensor.temperature);
                        output += ", " + tempe_web;
                        hum_web = String(iaqSensor.humidity);
                        output += ", " + hum_web;
                        output += ", " + String(iaqSensor.staticIaq);
                        output += ", " + String(iaqSensor.co2Equivalent);
                        output += ", " + String(iaqSensor.breathVocEquivalent);
                        output += ", " + String(iaqSensor.gasPercentage);

                        Serial.println( getDatum(IN_NUMBERS) );
                        //Serial.println(getMonat());//Check the month
                        Serial.println(intro);
                        Serial.println(output);
                        Serial.println(' ');
                        updateState();

                } else {
                        checkIaqSensorStatus();
                }

                if(flag_inter_loop) {
                        //Serial.println("Loop2 says: Core "+String(xPortGetCoreID()));
                        flag_inter_loop=false;
                        //vTaskDelay(10);///funktioniert nicht
                }
        }


}
/////////////################# LOOP Executed in Core 1 #####################////////////////
void loop1(void *parameter) {
        //Serial.print("---------### LOOP1");
        //Serial.println(i);//
        while(1) {
                yield();
                ledcWrite(PWMchannel, i);
                //Serial.println(BSEC_MAX_STATE_BLOB_SIZE);
                //delay(100);
                if (!flag) {
                        i++;
                        delay(1);
                }

                else{
                        i--;
                        delay(1);
                }

                if(i==1023) {
                        flag=1;
                        delay(75);
                }
                if(i==0) {
                        flag=0;
                        ledcWrite(PWMchannel, 0);
                        yield();
                        delay(600);
                        //Serial.println("Loop1 says: Core "+String(xPortGetCoreID()));
                        flag_inter_loop=true;
                }
        }
        //Serial.println("####Saliendo loop()");//
}
/////////////################# funciton  #####################////////////////
void loop(){
}
/////////////################# funciton  #####################////////////////
void feedTheDog(){
        // feed dog 0
        // TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        // TIMERG0.wdt_feed=1;                       // feed dog
        // TIMERG0.wdt_wprotect=0;                   // write protect
        // // feed dog 1
        // TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        // TIMERG1.wdt_feed=1;                       // feed dog
        // TIMERG1.wdt_wprotect=0;                   // write protect
}
/////////////################# funciton  #####################////////////////
void hw_wdt_disable(){
        *((volatile uint32_t*) 0x60000900) &= ~(1); // Hardware WDT OFF
}
/////////////################# funciton  #####################////////////////
void checkIaqSensorStatus(void)
{
        if (iaqSensor.status != BSEC_OK) {
                if (iaqSensor.status < BSEC_OK) {
                        output = "BSEC error code : " + String(iaqSensor.status);
                        Serial.println(output);
                        //for (;;);
                        //errLeds(); /* Halt in case of failure */
                } else {
                        output = "BSEC warning code : " + String(iaqSensor.status);
                        Serial.println(output);
                }
        }

        if (iaqSensor.bme680Status != BME680_OK) {
                if (iaqSensor.bme680Status < BME680_OK) {
                        output = "BME680 error code : " + String(iaqSensor.bme680Status);
                        Serial.println(output);
                } else {
                        output = "BME680 warning code : " + String(iaqSensor.bme680Status);
                        Serial.println(output);
                }
        }
        //iaqSensor.status = BSEC_OK;
}
/////////////################# funciton  #####################////////////////
void loadState(void)
{
        if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
                // Existing state in EEPROM
                Serial.println("Reading state from EEPROM");

                for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
                        bsecState[i] = EEPROM.read(i + 1);
                        Serial.println(bsecState[i], HEX);
                }

                iaqSensor.setState(bsecState);
                checkIaqSensorStatus();
        } else {
                // Erase the EEPROM with zeroes
                Serial.println("Erasing EEPROM");

                for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
                        EEPROM.write(i, 0);

                EEPROM.commit();
        }
}
/////////////################# funciton  #####################////////////////
void updateState(void)
{
        bool update = false;
        /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
        if (stateUpdateCounter == 0) {
                if (iaqSensor.iaqAccuracy >= 3) {
                        update = true;
                        stateUpdateCounter++;
                }
        }
        else {
                /* Update every STATE_SAVE_PERIOD milliseconds */
                if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
                        update = true;
                        stateUpdateCounter++;
                }
        }

        if (update) {
                iaqSensor.getState(bsecState);
                checkIaqSensorStatus();

                Serial.println("Writing state to EEPROM");

                for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
                        EEPROM.write(i + 1, bsecState[i]);
                        Serial.println(bsecState[i], HEX);
                }

                EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
                EEPROM.commit();
        }

}
/////////////################# Load variables to the app  #####################////////////////
void myTimerEvent()
{

        property = get_properties_for_iaq_level(iaqSensor.iaq);               // gets correspondig led color, led intensity and iaq description
        Blynk.virtualWrite(V34,iaqSensor.temperature);                        // sends temperature to chart of the app
        Blynk.virtualWrite(V25,iaqSensor.temperature);                        //sends temperature to numerical display of the app
        Blynk.virtualWrite(V33,press_web);                                    //sends pressure to the Blynk_App_Asistent
        Blynk.virtualWrite(V32,iaqSensor.humidity);                           //sends humidity
        Blynk.virtualWrite(V31,get_CO2_measure());                            // sends co2 estimate or meassuerement
        Blynk.virtualWrite(V30,iaqSensor.gasPercentageAcccuracy);             // sents gas percentage
        Blynk.virtualWrite(V28,iaqSensor.iaq);                                //sends iaq vale
        Blynk.virtualWrite(V29,iaq_to_percentage(iaqSensor.iaqAccuracy));     //sends iaq accuracy
        Blynk.setProperty(V27,"color",property.LED_color);                    //sends led color
        Blynk.virtualWrite(V27,property.LED_intensity);                       //sends led intensity
        Blynk.virtualWrite(V26,property.iaq_level_description);               //sends iaq description
        Blynk.virtualWrite(V24,getZeit());                                    // sends time
        Blynk.virtualWrite(V23,getDatum(IN_NUMBERS));                         //sends date

        // and the engineer saw that the code..
        //                                                ... was good...
}//
//
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
/////////////################# funciton  #####################////////////////
