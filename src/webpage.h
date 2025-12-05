#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <Preferences.h>

class WebPage {
public:
    WebPage(WebServer* server);

    void setNetworkInfo(IPAddress ip, String mac);
    void setStatus(bool lampOn);
    void onToggle(std::function<void(void)> cb);

    void setupRoutes();

private:
    WebServer* _server;
    IPAddress _ip;
    String _mac;
    bool _lampOn = false;

    String _historico = "";
    std::function<void(void)> _callback;

    String getWifiBars(int rssi);
};

#endif
