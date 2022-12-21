
#include "Inkplate.h"
#include "HTTPClient.h"
#include "ArduinoJson.h"



class Dashboard
{
private:
    Inkplate &display;
    String serverUrl;
    String imageUrl;
    String archivedImageUrl;
    int timeToSleep;

    void loadConfig();
    void sendToDeepSleep(int seconds);
    void handleImage();

public:
    Dashboard(Inkplate &d, String serverUrl): display(d) {
        this->serverUrl = serverUrl;
    }
    void show();
};