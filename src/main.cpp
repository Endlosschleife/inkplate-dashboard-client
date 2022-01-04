#include "HTTPClient.h"
#include "Inkplate.h"
#include "WiFi.h"
#include "config.h"
#include "ArduinoJson.h"

uint64_t MS_TO_S_FACTOR = 1000000;

Inkplate display(INKPLATE_1BIT);
RTC_DATA_ATTR int wakeupCounter = 0;
DynamicJsonDocument doc(ESP.getMaxAllocHeap());

void sendToDeepSleep(int seconds)
{
    //Isolate/disable GPIO12 on ESP32 (only to reduce power consumption in sleep)
    //rtc_gpio_isolate(GPIO_NUM_12);
    // go to sleep
    Serial.println("Going to deep sleep for " + String(seconds) + " seconds now.");
    uint64_t msToWakeup = seconds * MS_TO_S_FACTOR;
    Serial.print("Sleep in ms: ");
    Serial.println(msToWakeup);
    esp_sleep_enable_timer_wakeup(msToWakeup);
    esp_deep_sleep_start();
}

void connectWifi()
{
    Serial.println("Connect to wifi...");

    //Connect to the WiFi network.
    int attempts = 0;
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED && attempts < 5)
    {
        attempts++;
        delay(500);
    }

    // still not connected: reset wifi see issue: https://github.com/espressif/arduino-esp32/issues/2501
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi Connection could not be established. Will reset Wifi and try again.");
        WiFi.persistent(false);
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_MODE_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        delay(3000);

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Could still not connect to wifi.");
        }
        else
        {
            Serial.println("Connected to Wifi successfully.");
        }
    }

    Serial.println("done.");
}

void setup()
{
    Serial.begin(9600);
    display.begin();

    connectWifi();

    // load config
    HTTPClient http;
    http.begin(String(SERVER_URL) + "/dashboard/livingroom");
    int httpCode = http.GET();
    if (httpCode != 200)
    {
        Serial.println("Could not load data from API");
        Serial.println("Response code is " + httpCode);

        display.clearDisplay();
        display.setCursor(300, 290);
        display.setTextSize(4);
        display.print("FEHLER");
        display.display();
        sendToDeepSleep(30 * 60);
        return;
    }
    // Parse JSON object and load into global doc
    doc.clear();
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }
    String imageUrl = String(SERVER_URL) + "/" + doc["imageUrl"].as<String>();
    String archivedImageUrl = String(SERVER_URL) + "/" + doc["archivedImageUrl"].as<String>();
    int timeToSleep = doc["timeToSleep"].as<int>();

    Serial.print("Image Url: ");
    Serial.println(imageUrl);
    Serial.print("Archived Image Url: ");
    Serial.println(archivedImageUrl);

    wakeupCounter++;

    // handle behaviour using wake up reason
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    // whether the screen should be fully updated (true) or partial update is enough (false)
    boolean shouldFullyUpdatedScreen = wakeup_reason != ESP_SLEEP_WAKEUP_TIMER || (wakeupCounter % 6) == 0; // always if not woken up by timer

    Serial.print("Should fully update: ");
    Serial.println(shouldFullyUpdatedScreen);

    if (!shouldFullyUpdatedScreen)
    {
        Serial.println("rebuild previous screen");
        if (!display.drawImage(archivedImageUrl, display.PNG, 0, 0, false, false))
        {
            shouldFullyUpdatedScreen = true;
        }
        else
        {
            display.preloadScreen();
        }
    }
    display.clearDisplay();

    Serial.println("load new screen");
    if (!display.drawImage(imageUrl, display.PNG, 0, 0, false, false))
    {
        display.println("Image open error");
        display.display();
    }

    if (!shouldFullyUpdatedScreen)
    {
        Serial.println("partial update");
        display.partialUpdate(INKPLATE_FORCE_PARTIAL);
    }
    else
    {
        Serial.println("full update");
        display.display();
    }

    Serial.print("Time to sleep: ");
    Serial.println(timeToSleep);
    sendToDeepSleep(timeToSleep * 60);
}

void loop()
{
}
