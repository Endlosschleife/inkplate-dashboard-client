#include "HTTPClient.h"
#include "Inkplate.h"
#include "WiFi.h"
#include "config.h"
#include "ArduinoJson.h"

#define ENABLE_RTC // fime

uint64_t MS_TO_S_FACTOR = 1000000;

Inkplate display(INKPLATE_1BIT);
RTC_DATA_ATTR int wakeupCounter = 0;
DynamicJsonDocument doc(ESP.getMaxAllocHeap());

void sendToDeepSleep(int seconds)
{
    Serial.println("Going to deep sleep for " + String(seconds) + " seconds now.");

#ifdef ENABLE_RTC
    // rtc sleep
    Serial.println("Use RTC for deep sleep");
    Serial.println(display.rtcGetEpoch());
    display.rtcSetAlarmEpoch(display.rtcGetEpoch() + seconds, RTC_ALARM_MATCH_DHHMMSS);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0);
    esp_deep_sleep_start();
#else
    // non rtc sleep
    Serial.println("Use timer for deep sleep.");
    uint64_t msToWakeup = 10 * MS_TO_S_FACTOR;
    Serial.print("Sleep in ms: ");
    Serial.println(msToWakeup);
    esp_sleep_enable_timer_wakeup(msToWakeup);
    esp_deep_sleep_start();
#endif
}

void connectWifi()
{
    Serial.println("Connect to wifi...");

    // Connect to the WiFi network.
    int attempts = 0;
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
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

void drawImage()
{
    // Draw the second image from web, this time using a HTTPClient to fetch the response manually.
    // Full color 24 bit images are large and take a long time to load, will take around 20 secs.
    HTTPClient http;
    // Set parameters to speed up the download process.
    http.getStream().setNoDelay(true);
    http.getStream().setTimeout(1);

    // Photo taken by: Roberto Fernandez
    http.begin("https://varipass.org/neowise.bmp");

    // Check response code.
    int httpCode = http.GET();
    if (httpCode == 200)
    {
        // Get the response length and make sure it is not 0.
        int32_t len = http.getSize();
        if (len > 0)
        {
            if (!display.drawBitmapFromWeb(http.getStreamPtr(), 0, 0, len))
            {
                // If is something failed (wrong filename or wrong bitmap format), write error message on the screen.
                // REMEMBER! You can only use Windows Bitmap file with color depth of 1, 4, 8 or 24 bits with no
                // compression!
                display.println("Image open error");
                display.display();
            }
            display.display();
        }
        else
        {
            display.println("Invalid response length");
            display.display();
        }
    }
    else
    {
        display.println("HTTP error");
        display.display();
    }
}

void setup()
{
    // Serial.begin(9600); fixme deep sleep does not work properly when this is activated...
    display.begin();

#ifdef ENABLE_RTC
    display.rtcClearAlarmFlag(); // Clear alarm flag from any previous alarm
    if (!display.rtcIsSet())     // Check if RTC is already is set. If ts not, set time and date
    {
        display.rtcSetTime(0, 0, 0);
        display.rtcSetDate(6, 16, 5, 2020);
    }
#endif

    connectWifi();

    // load config
    HTTPClient http;
    http.begin(String(SERVER_URL) + "/dashboard/calendar");
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
    boolean shouldFullyUpdatedScreen = (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER && wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) 
        || (wakeupCounter % 6) == 0; // always if not woken up by timer

    Serial.print("Should fully update: ");
    Serial.println(shouldFullyUpdatedScreen);

    if (!shouldFullyUpdatedScreen)
    {
        Serial.println("rebuild previous screen");
        if (!display.drawImage(archivedImageUrl, 0, 0, false, false))
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
    if (!display.drawImage(imageUrl, 0, 0, false, false))
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

    sendToDeepSleep(timeToSleep * 60);
}

void loop()
{
}
