/*
    Office presence sensor monitoring Bluetooth devices
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#ifndef BLUETOOTHSENSOR_H
#define BLUETOOTHSENSOR_H

#include <iostream>
#include <curl/curl.h>

#include "json/json.h"
#include "iniparser.h"

#include "mosquittohandler.h"
#include "datagetter.h"

#include "bluetoothpoller.h"

class BluetoothSensor
{
public:
    BluetoothSensor();
    ~BluetoothSensor();

    bool initAll(std::string configFileName);
    void run();

private:

    // gets device info json from server and updates the local device database
    bool updateDeviceData();

    // scans given device and sends availability status using mqtt
    bool checkDevice(unsigned int deviceIndex);

    // checks incoming messages if they contain request for database update or device discovery
    void processIncomingMessages(bool& updateDB, bool& scan);

    // discovers bt devices in the range and sends bt addresses using mqtt
    bool discoverDevices();

    // tries to connect mosquitto when it didn't succeed normally or connection was lost
    bool connectMosquitto(bool reconnect = true);

    // sends hello message using mqtt
    void sendHello();

    void print(std::string str, bool endl = true);
    void printError(std::string str);

    BluetoothPoller* m_bluetoothPoller;
    DataGetter* m_dataGetter;
    MosquittoHandler* m_mosquitto;

    std::string m_sensorID;

    std::vector<std::string> m_devices;

    std::string m_brokerAddress;
    uint16_t m_brokerPort;
    std::string m_dataFetchUrl;
    int16_t m_connectAttemptInterval;

    bool m_updateDBNeeded;
};

#endif // BLUETOOTHSENSOR_H
