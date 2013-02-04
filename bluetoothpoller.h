/*
    Office presence sensor monitoring Bluetooth devices
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#ifndef BLUETOOTHPOLLER_H
#define BLUETOOTHPOLLER_H

#include <vector>
#include <iostream>

struct DiscoveredDevice
{
    std::string btAddress;
    std::string name;
};

class BluetoothPoller
{
public:
    BluetoothPoller();
    ~BluetoothPoller();

    bool init(std::string& address);
    void shutdown();

    bool scanDevice(std::string btAddress);
    bool discoverDevices(std::vector<DiscoveredDevice>& discoveredDevices);

    std::string getLastErrorString();

private:
    int m_socket;
    std::string m_lastErrorString;
};

#endif // BLUETOOTHPOLLER_H
