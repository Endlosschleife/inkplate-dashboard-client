#include "Inkplate.h"
#include "WiFi.h"
#include "config.h"
#include "Dashboard.h"

Inkplate display(INKPLATE_1BIT);


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
            Serial.println("Could still not connect to wifi. Restart ESP...");
            ESP.restart();
        }
        else
        {
            Serial.println("Connected to Wifi successfully.");
        }
    } else {
        Serial.println("Connected to Wifi.");
    }
}

void setup()
{
    Serial.begin(115200); // fixme deep sleep does not work properly when this is activated...
    display.begin();

    // enable RTC
    display.rtcClearAlarmFlag(); // Clear alarm flag from any previous alarm
    if (!display.rtcIsSet())     // Check if RTC is already is set. If ts not, set time and date
    {
        display.rtcSetTime(0, 0, 0);
        display.rtcSetDate(6, 16, 5, 2020);
    }

    // todo check if WIFI is really needed here
    connectWifi();
   
   Dashboard dashboard = Dashboard(display, SERVER_URL);
   dashboard.show();
    
}

void loop()
{
}
