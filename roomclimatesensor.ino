// TODO:
// - wait until IP address was assigned before try to connect to prtg server
// - protect to run into endless-loop - force deep-sleep, timelimt for wait for
//   WiFi and IP adress

#include <ESP8266WiFi.h>
#include "DHT.h"

#include "roomclimatesensor.h"

DHT dht(DHTPIN, DHT22, 30);


void setup(void) {

    float temperature;
    float humidity;
    float heatindex;

    Serial.begin(115200);
    Serial.println();

    WiFi.begin(SSID, PASS);

    dht.begin();

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
        heatindex = dht.computeHeatIndex(temperature, humidity, false);

        Serial.println("temperature: " + String(temperature) + " C");
        Serial.println("humidity: " + String(humidity) + " %");
        Serial.println("heat index: " + String(heatindex) + " C");

        Serial.print("waiting for wifi");
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        delay(1000);

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
                "</prtg>";

            client.print("GET " + url + " HTTP/1.1\r\n" +
                "Host: " + PRTG_HOST + "\r\n" +
                "Connection: close\r\n\r\n");

            while(client.available()) {
                client.readStringUntil('\r');
            }
            Serial.println("done");
        } else {
            Serial.println("connection to prtg failed");
            delay(2000);
        }
    } else {
        Serial.println("reading DHT failed");
        delay(2000);
    }
    Serial.println("going to deep sleep for " + String(SLEEP) + " seconds");
    ESP.deepSleep(SLEEP * 1000000);
}

void loop(void) {
    // not used
}
