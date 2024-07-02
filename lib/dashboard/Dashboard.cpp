#include "Dashboard.h"

DynamicJsonDocument doc(ESP.getMaxAllocHeap());
RTC_DATA_ATTR int wakeupCounter = 0;

void Dashboard::show()
{
    this->loadConfig();
    this->handleImage();
    this->sendToDeepSleep(this->timeToSleep * 60);
}

void Dashboard::loadConfig()
{
    // load config
    HTTPClient http;
    int httpCode;
    int attempts = 0;
    bool success = false;
    while (!success && attempts < 3)
    {
        http.begin(this->serverUrl + "/dashboard/livingroom");
        httpCode = http.GET();

        if (httpCode == 200)
        {
            Serial.println("Loaded config successfully.");
            success = true;
        }
        else
        {
            Serial.print("Could not load config. Attempt: ");
            Serial.println(attempts);
            attempts++;
            delay(5000);
        }
    }
    // still with error
    if (httpCode != 200)
    {
        Serial.println("Could not load data from API");
        Serial.println("Response code is " + httpCode);

        display.clearDisplay();
        display.setCursor(450, 390);
        display.setTextSize(4);
        display.print("FEHLER :-(");
        display.display();
        sendToDeepSleep(30 * 60); // fixme return an error code and handle error above
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
    this->imageUrl = this->serverUrl + "/" + doc["imageUrl"].as<String>();
    this->archivedImageUrl = this->serverUrl + "/" + doc["archivedImageUrl"].as<String>();
    this->timeToSleep = doc["timeToSleep"].as<int>();

    Serial.print("Image Url: ");
    Serial.println(imageUrl);
    Serial.print("Archived Image Url: ");
    Serial.println(archivedImageUrl);
    Serial.print("Time to sleep: ");
    Serial.println(this->timeToSleep);
}

void Dashboard::handleImage()
{
    wakeupCounter++;

    // handle behaviour using wake up reason
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    // whether the screen should be fully updated (true) or partial update is enough (false)
    boolean shouldFullyUpdatedScreen = (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER && wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) || (wakeupCounter % 6) == 0; // always if not woken up by timer

    Serial.print("Should fully update: ");
    Serial.println(shouldFullyUpdatedScreen);

    if (!shouldFullyUpdatedScreen)
    {
        Serial.println("rebuild previous screen");
        if (display.drawImage(archivedImageUrl, 0, 0, false, false))
        {
            display.preloadScreen();
        }
        else
        {
            shouldFullyUpdatedScreen = true;
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
}

void Dashboard::sendToDeepSleep(int seconds)
{
    Serial.println("Going to deep sleep for " + String(seconds) + " seconds now.");
    display.rtcSetAlarmEpoch(display.rtcGetEpoch() + seconds, RTC_ALARM_MATCH_DHHMMSS);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0);
    esp_deep_sleep_start();
}