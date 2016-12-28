#include <ESP8266WiFi.h>
#include "DHT.h"

#include "roomclimatesensor.h"

DHT dht(DHTPIN, DHTTYPE);
ADC_MODE(ADC_VCC);

void setup(void) {

    float temperature;
    float humidity;
    float heatindex;
    float vcc;

    Serial.begin(115200);
    Serial.println();

    WiFi.begin(SSID, PASS);

    dht.begin();
    // wait a bit before read values, otherwise it will fail often.
    // TODO: figure out minimum sleep time
    delay(500);
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    // currenlty it says always 2.77 and I'm not sure if this is working
    // correclty. I never tried with batterie power yet.
    // I saw somwhere, that for precice measurements a capacitator is needed
    // somewhere, not sure if NodeMCU and WeMos D1 mini have this already and
    // if it's really needed.
    vcc = ESP.getVcc() / 1000.0;

    if (!isnan(temperature) && !isnan(humidity)) {
        heatindex = dht.computeHeatIndex(temperature, humidity, false);

        Serial.println("temperature: " + String(temperature) + " C");
        Serial.println("humidity: " + String(humidity) + " %");
        Serial.println("heat index: " + String(heatindex) + " C");
        Serial.println("vcc: " + String(vcc) + " V");

        Serial.print("waiting for wifi");
        int waitloops = 0;
        while (WiFi.status() != WL_CONNECTED && waitloops < 20) {
            Serial.print(".");
            delay(500);
            waitloops++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            // I have the suspicion that WL_CONNECTED doesn't mean that we already got
            // a IP from the DHCP server. Better wait a bit here.
            // TODO: check if I right with my suspicion and implement a wait-loop
            //       for the ip address if needed.
            delay(3000);

            Serial.print(" address is ");
            Serial.println(WiFi.localIP());

            WiFiClient client;
            if (client.connect(PRTG_HOST, PRTG_PORT)) {
                Serial.print("connected to prtg, sending data ... ");

                String url = "/" + String(PRTG_SENSOR) + "?content=" +
                    "<prtg>" +
                    "<result>" +
                      "<channel>Temperature</channel>" +
                      "<unit>Celsius</unit>" +
                      "<float>1</float>" +
                      "<value>" + String(temperature) + "</value>" +
                    "</result>" +
                    "<result>" +
                      "<channel>Humidity</channel>" +
                      "<unit>Percent</unit>" +
                      "<float>1</float>" +
                      "<value>" + String(humidity) + "</value>" +
                    "</result>" +
                    "<result>" +
                      "<channel>HeatIndex</channel>" +
                      "<unit>Celsius</unit>" +
                      "<float>1</float>" +
                      "<value>" + String(heatindex) + "</value>" +
                    "</result>" +
                    "<result>" +
                      "<channel>Voltage</channel>" +
                      "<unit>Volt</unit>" +
                      "<float>1</float>" +
                      "<value>" + String(vcc) + "</value>" +
                    "</result>" +
                    "</prtg>";

                client.print("GET " + url + " HTTP/1.1\r\n" +
                    "Host: " + PRTG_HOST + "\r\n" +
                    "Connection: close\r\n\r\n");

                while(client.available()) {
                    client.readStringUntil('\r');
                }

                client.stop();

                Serial.println("done");
            } else {
                Serial.println("connection to prtg failed");
            }
        } else {
            Serial.println("unable to connect to wifi");
        }
    } else {
        Serial.println("reading DHT failed");
    }

    // don't know if this is needed. need to check the power consumption.
    Serial.println("turning off WiFi");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);

    // TODO: what is the difference betwee WAKE_RF_DISABLED and without it?
    Serial.println("going to deep sleep for " + String(SLEEP) + " seconds");
    //ESP.deepSleep(SLEEP * 1000000);
    ESP.deepSleep(SLEEP * 1000000, WAKE_RF_DISABLED);
}

void loop(void) {
    // not used
}
