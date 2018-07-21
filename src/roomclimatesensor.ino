#include <ESP8266WiFi.h>
#include "DHT.h"

#include "roomclimatesensor.h"

DHT dht(DHTPIN, DHTTYPE);
ADC_MODE(ADC_VCC);

struct dhtdata {
    float temperature;
    float humidity;
    float heatindex;
    float vcc;
};

dhtdata measurement(void) {
    // wait a bit before read values, otherwise it will fail often.
    // TODO: figure out minimum sleep time
    //delay(2000);

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("reading DHT failed");
    }

    // Currenlty it says always 2.77 V when for VCC and I'm not sure if this
    // is working correclty. I never tried with battery power yet.
    // I saw somwhere that for precice measurements a capacitator is needed
    // somewhere, not sure if NodeMCU and WeMos D1 mini have this already and
    // if it's really needed.
    float vcc = ESP.getVcc() / 1000.0;

    dhtdata m = {
        temperature,
        humidity,
        dht.computeHeatIndex(temperature, humidity, false),
        vcc
    };
    return m;
}

bool waitforwifi(void) {
    Serial.print("waiting for wifi");

    for (int i = 0; i < 20; i++) {
        Serial.print(".");
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("success");
            Serial.print("IP address is ");
            Serial.println(WiFi.localIP());
            return true;
        }
        delay(500);
    }

    // fail if not successfull after couple of seconds
    Serial.println("failed");
    return false;
}

bool turnoffwifi(void) {
    // don't know if this is needed. need to check the power consumption.
    Serial.println("turning off WiFi");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

void showdata(dhtdata* data) {
    Serial.println("temperature: " + String(data->temperature) + " C");
    Serial.println("humidity: " + String(data->humidity) + " %");
    Serial.println("heat index: " + String(data->heatindex) + " C");
    Serial.println("vcc: " + String(data->vcc) + " V");
}

bool submit_pushgateway(dhtdata* data, String deviceid) {
    WiFiClient client;
    if (client.connect(PUSHGATEWAY_HOST, PUSHGATEWAY_PORT)) {
        Serial.print("connected to pushgateway " + String(PUSHGATEWAY_HOST) + ", sending data... ");

        String body = "esp_temperature{device=\"" + deviceid + "\"} " + String(data->temperature) + "\n"
            + "esp_humidity{device=\"" + deviceid + "\"} " + String(data->humidity) + "\n"
            + "esp_heatindex{device=\"" + deviceid + "\"} " + String(data->heatindex) + "\n"
            + "esp_vcc{device=\"" + deviceid + "\"} " + String(data->vcc) + "\n";

        client.println("POST /metrics/job/esp HTTP/1.1\r\n"
            "Host: " + String(PUSHGATEWAY_HOST) + ":" + PUSHGATEWAY_PORT + "\r\n"
            "User-Agent: d1mini/010\r\n"
            "Accept: */*\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-length: " + body.length() + "\r\n"
            "\r\n" + body);

        while(client.available()) {
            client.read();
        }

        client.stop();

        Serial.println("done");
        return true;
    }
    Serial.println("connection to pushgateway failed");
    return false;
}

bool submit_influxdb(dhtdata* data, String deviceid) {
    WiFiClient client;
    if (client.connect(INFLUXDB_HOST, INFLUXDB_PORT)) {
        Serial.print("connected to influxdb " + String(INFLUXDB_HOST) + ", sending data... ");

        String body = "temperature,device=\"" + deviceid + "\" value=" + String(data->temperature) + "\n"
            + "humidity,device=\"" + deviceid + "\" value=" + String(data->humidity) + "\n"
            + "heatindex,device=\"" + deviceid + "\" value=" + String(data->heatindex) + "\n"
            + "vcc,device=\"" + deviceid + "\" value=" + String(data->vcc) + "\n";

        client.println("POST /write?db=" + String(INFLUXDB_DB) + " HTTP/1.1\r\n"
            "Host: " + String(INFLUXDB_HOST) + ":" + INFLUXDB_PORT + "\r\n"
            "User-Agent: d1mini/010\r\n"
            "Accept: */*\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-length: " + body.length() + "\r\n"
            "\r\n" + body);

        while(client.available()) {
            client.read();
        }

        client.stop();

        Serial.println("done");
        return true;
    }
    Serial.println("connection to influxdb failed");
    return false;
}

void deepsleep(void) {
    // TODO: what is the difference betwee WAKE_RF_DISABLED and without it?
    Serial.println("going to deep sleep for " + String(SLEEP) + " seconds");
    ESP.deepSleep(SLEEP * 1000000);
    //ESP.deepSleep(SLEEP * 1000000, WAKE_RF_DISABLED);
}

void setup(void) {
    Serial.begin(115200);
    Serial.println();

    WiFi.begin(SSID, PASS);
    dht.begin();
    waitforwifi();
}


void loop(void) {
    dhtdata data = measurement();
    showdata(&data);

#ifdef DEVICEID
    String deviceid = DEVICEID;
#else
    String deviceid = WiFi.macAddress();
#endif

#if defined(PUSHGATEWAY_HOST) && defined(PUSHGATEWAY_PORT)
    submit_pushgateway(&data, deviceid);
#endif
#if defined(INFLUXDB_HOST) && defined(INFLUXDB_PORT) && defined(INFLUXDB_DB)
    submit_influxdb(&data, deviceid);
#endif

    turnoffwifi();
    deepsleep(); // everything after this won't be executed anymore
    delay(SLEEP * 1000);
}
