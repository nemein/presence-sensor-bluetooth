/*
    Bluetooth sensor for Nemein's Smart Workplace
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#include "bluetoothpoller.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

BluetoothPoller::BluetoothPoller() : m_socket(0)
{

}

BluetoothPoller::~BluetoothPoller()
{
    shutdown();
}

bool BluetoothPoller::init(std::string& address)
{
    // open bt socket
    int dev_id = hci_get_route(NULL);
    m_socket = hci_open_dev( dev_id );
    if (dev_id < 0 || m_socket < 0)
    {
        m_lastErrorString = "Cannot open bluetooth socket";
        return false;
    }

    // get device address
    hci_dev_info di;
    hci_devinfo(dev_id, &di);
    char addr[19] = {0};
    ba2str(&di.bdaddr, addr);
    address = addr;

    m_lastErrorString = "";
    return true;
}

void BluetoothPoller::shutdown()
{
    close(m_socket);
}

bool BluetoothPoller::scanDevice(std::string BTAddress)
{
    char name[248] = {0};

    bdaddr_t ba;
    str2ba(BTAddress.c_str(), &ba);

    int res = hci_read_remote_name(m_socket, &ba, sizeof(name), name, 0);

    if (res == -1)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool BluetoothPoller::discoverDevices(std::vector<DiscoveredDevice>& discoveredDevices)
{
    int num_rsp = 0;
    int dev_id = 0;
    char addr[19] = {0};
    char name[248] = {0};

    int len  = 8;
    int max_rsp = 255;
    int flags = IREQ_CACHE_FLUSH;

    inquiry_info *ii = NULL;
    ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));

    num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
    if( num_rsp < 0 )
    {
        m_lastErrorString = "hci_inquiry error";
        return false;
    }

    discoveredDevices.clear();

    for (int i = 0; i < num_rsp; i++) {
        ba2str(&(ii+i)->bdaddr, addr);
        std::string str(addr);

        if (hci_read_remote_name(m_socket, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
        {
            strcpy(name, "[unknown]");
        }

        DiscoveredDevice newDevice;
        newDevice.btAddress = addr;
        newDevice.name = name;
        discoveredDevices.push_back(newDevice);
    }

    free(ii);

    m_lastErrorString = "";
    return true;
}

std::string BluetoothPoller::getLastErrorString()
{
    return m_lastErrorString;
}




