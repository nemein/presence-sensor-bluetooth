/*
    Bluetooth sensor for Nemein's Smart Workplace
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#include "bluetoothsensor.h"

#include <sstream>
#include <signal.h>

const std::string DEFAULT_BROKER_ADDRESS = "localhost";
const uint16_t DEFAULT_BROKER_PORT = 1883;
const std::string DEFAULT_DATA_FETCH_URL = "localhost:8181/api/connection";
const int16_t DEFAULT_CONNECT_ATTEMPT_INTERVAL = 5;

static bool quit = false;

// handler for Ctrl+C, quits program
void siginthandler(int param)
{
    (void)param; //prevent warning

    std::cout << " *** QUITTING... *** " << std::endl;
    quit = true;
}

BluetoothSensor::BluetoothSensor() :
    m_bluetoothPoller(0),
    m_dataGetter(0),
    m_mosquitto(0),
    m_sensorID("<NO NAME>"),
    m_brokerAddress(DEFAULT_BROKER_ADDRESS),
    m_brokerPort(DEFAULT_BROKER_PORT),
    m_dataFetchUrl(DEFAULT_DATA_FETCH_URL),
    m_connectAttemptInterval(DEFAULT_CONNECT_ATTEMPT_INTERVAL),
    m_updateDBNeeded(true)
{
    signal(SIGINT, siginthandler);
}

BluetoothSensor::~BluetoothSensor()
{
    if (m_bluetoothPoller)
    {
        delete m_bluetoothPoller;
        m_bluetoothPoller = 0;
    }
    if (m_dataGetter)
    {
        delete m_dataGetter;
        m_dataGetter = 0;
    }
    if (m_mosquitto)
    {
        delete m_mosquitto;
        m_mosquitto = 0;
    }
}

bool BluetoothSensor::initAll(std::string configFileName)
{
    bool manualSensorID = false;

    print("Reading config file...");

    dictionary* ini ;
    ini = iniparser_load(configFileName.c_str());
    if (!ini) {
        print("Cannot parse config file. Using default values");
    }
    else
    {
        m_brokerAddress = iniparser_getstring(ini, ":broker_address",
                                              (char*)DEFAULT_BROKER_ADDRESS.c_str());
        m_brokerPort = iniparser_getint(ini, ":broker_port",
                                        DEFAULT_BROKER_PORT);
        m_dataFetchUrl = iniparser_getstring(ini, ":data_fetch_url",
                                              (char*)DEFAULT_DATA_FETCH_URL.c_str());
        m_connectAttemptInterval = iniparser_getint(ini, ":connect_attempt_interval",
                                        DEFAULT_CONNECT_ATTEMPT_INTERVAL);

        if (iniparser_find_entry(ini, ":sensor_id"))
        {
            m_sensorID = iniparser_getstring(ini, ":sensor_id", 0);
            manualSensorID = true;
        }
    }
    iniparser_freedict(ini);

    if (!m_bluetoothPoller)
    {
        print("Initializing Bluetooth... ");

        std::string btAddress;
        m_bluetoothPoller = new BluetoothPoller();
        if (!m_bluetoothPoller->init(btAddress))
        {
            printError(m_bluetoothPoller->getLastErrorString());
            return false;
        }
        if (!manualSensorID) m_sensorID = "bt-sensor_" + btAddress;
    }

    if (!m_dataGetter)
    {
        print("Initializing Curl...");

        m_dataGetter = new DataGetter();
        if (!m_dataGetter->init())
        {
            printError(m_dataGetter->getLastErrorString());
            return false;
        }
    }

    if (!m_mosquitto)
    {
        print("Initializing Mosquitto... ");

        m_mosquitto = new MosquittoHandler;
        std::string mosqID = m_sensorID;

        if (!m_mosquitto->init(mosqID.c_str()) || !m_mosquitto->connectToBroker(m_brokerAddress.c_str(), m_brokerPort))
        {
            printError(m_mosquitto->getLastErrorString());
            return false;
        }
        m_mosquitto->subscribe("command/fetch_device_database");
        m_mosquitto->subscribe(std::string("command/scan/bluetooth/" + m_sensorID).c_str());
        m_mosquitto->subscribe("command/scan/bluetooth");

        print("Connecting to broker... ");
        if (!m_mosquitto->waitForConnect())
        {
            // first connection attempt failed, go into retry loop
            printError(m_mosquitto->getLastErrorString());
            while (!connectMosquitto(false))
            {
                if (quit) return false;
            }
        }
    }

    m_updateDBNeeded = !updateDeviceData();

    sendHello();

    print("Sensor ID: " + m_sensorID);

    return true;
}

void BluetoothSensor::run()
{
    bool updateDB = false;
    bool scan = false;

    print("Running, CTRL+C to quit...");

    // index of the device currently scanned
    unsigned int deviceIndex = 0;

    while(!quit)
    {
        // update device db if previous update didn't succeed
        if (m_updateDBNeeded) m_updateDBNeeded = !updateDeviceData();

        // make sure that current device index is valid.
        // this also guarantees that no scanning is made when there are no devices.
        if (deviceIndex < m_devices.size())
        {
            checkDevice(deviceIndex);
        }

        // move to next device, start from beginning when at the end
        deviceIndex++;
        if (deviceIndex >= m_devices.size()) deviceIndex = 0;

        // check if there are arrived mqtt messages (commands)
        do
        {
            processIncomingMessages(updateDB, scan);
            if (updateDB)
            {
                m_updateDBNeeded = !updateDeviceData();
            }
            if (scan)
            {
                discoverDevices();
            }
        }

        // check arrived messages again after database update or device discovery,
        // so that a possible request for those is processed before continuing
        while ((updateDB || scan) && !quit);

        // check that Mosquitto is still connected.
        // if Mosquitto disconnects there can be messages from this iteration
        // which aren't sent at all, but missing some outgoing messages
        // shouldn't be too big of a problem.
        if (!m_mosquitto->isConnected())
        {
            printError("Mosquitto disconnected");
            if (connectMosquitto())
            {
                // update device db and send hello after mosquitto reconnect
                m_updateDBNeeded = !updateDeviceData();
                sendHello();
            }
        }
    }
}

// scans given device and sends availability status using mqtt
bool BluetoothSensor::checkDevice(unsigned int deviceIndex)
{
    if (deviceIndex >= m_devices.size())
    {
        return false;
    }
    std::stringstream ss;
    ss << "Checking device " << m_devices.at(deviceIndex) << "... ";
    print(ss.str(), false);

    bool available = m_bluetoothPoller->scanDevice(m_devices.at(deviceIndex));

    if (quit) return false;

    std::string availableTopic = "sensor/" + m_sensorID + "/bluetooth/available";
    std::string unavailableTopic = "sensor/" + m_sensorID + "/bluetooth/unavailable";

    std::string topic;

    if (available)
    {
        topic = availableTopic;
        print("AVAILABLE");
    }
    else
    {
        topic = unavailableTopic;
        print("unavailable");
    }

    m_mosquitto->publish(topic.c_str(), m_devices.at(deviceIndex).c_str());
    m_mosquitto->loop();
    return available;
}

// gets device info json from server and updates local device database
bool BluetoothSensor::updateDeviceData()
{
    print("Fetching device database...");

    std::string data;
    if (!m_dataGetter->get(m_dataFetchUrl.c_str(), data))
    {
        printError(m_dataGetter->getLastErrorString());
        return false;
    }

    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(data, root);
    if (!parsingSuccessful)
    {
        printError("Failed to parse device data\n" + reader.getFormatedErrorMessages());
        return false;
    }

    m_devices.clear();

    // go through root elements and search for "bluetooth"
    for (unsigned int i = 0; i < root.size(); i++ )
    {
        if (root[i].get("type", "") == "bluetooth")
        {
            // "identifier" field contains the bt address
            std::string newDevice = root[i].get("identifier", "").asString();
            m_devices.push_back(newDevice);
        }
    }

    if (m_devices.size() > 0)
    {
        print("Devices:");
        for (unsigned int i = 0; i < m_devices.size(); i++)
        {
            print(m_devices.at(i));
        }
    }
    else
    {
        print("No devices");
    }

    return true;
}

// checks incoming messages if they contain request for database update or device discovery
void BluetoothSensor::processIncomingMessages(bool& updateDB, bool& scan)
{
    // loop many times to receive multiple messages.
    // can be done smarter with mosquitto 1.0 ->
    for (unsigned int i = 0; i < 10; i++) m_mosquitto->loop();

    std::vector<mqttMessage> messages = m_mosquitto->getArrivedMessages();

    updateDB = false;
    scan = false;

    for (unsigned int i = 0; i < messages.size(); i++)
    {
        if (messages[i].topic == "command/fetch_device_database")
        {
            updateDB = true;
        }
        else if (messages[i].topic.substr(0, 22) == "command/scan/bluetooth")
        {
            scan = true;
        }
    }
}


// discovers bt devices in the range and sends bt addresses using mqtt
bool BluetoothSensor::discoverDevices()
{
    print("Discovering devices...");

    std::vector<DiscoveredDevice> discoveredDevices;
    if (!m_bluetoothPoller->discoverDevices(discoveredDevices))
    {
        printError(m_bluetoothPoller->getLastErrorString());
        std::string scanCompleteTopic = "sensor/" + m_sensorID + "/bluetooth/scan_complete";
        m_mosquitto->publish(scanCompleteTopic.c_str(), "");
        m_mosquitto->loop();

        return false;
    }

    if (discoveredDevices.size() > 0) print("Found:");

    std::string newDeviceTopic = "sensor/" + m_sensorID + "/bluetooth/new_device";
    for (unsigned int i = 0; i < discoveredDevices.size(); i++)
    {
        print(discoveredDevices.at(i).btAddress + " " + discoveredDevices.at(i).name);

        Json::Value root;
        root["name"] = discoveredDevices.at(i).name;
        root["mac"] = discoveredDevices.at(i).btAddress;

        Json::StyledWriter writer;
        std::string JSONstring = writer.write(root);
        m_mosquitto->publish(newDeviceTopic.c_str(), JSONstring.c_str());
        m_mosquitto->loop();
    }

    std::string scanCompleteTopic = "sensor/" + m_sensorID + "/bluetooth/scan_complete";
    m_mosquitto->publish(scanCompleteTopic.c_str(), "");
    m_mosquitto->loop();

    return true;
}

// tries to connect mosquitto when it didn't succeed normally or connection was lost
bool BluetoothSensor::connectMosquitto(bool reconnect)
{
    // when reconnecting the first attempt occures right here at the start before waiting
    if (reconnect)
    {
        print("Reconnecting Mosquitto attempt #1...");
        if (m_mosquitto->reconnect())
        {
            print("Mosquitto reconnected");
            return true;
        }
        if (quit) return false;
    }

    // repeat until connection established or quitted
    int attempts = 1;
    do
    {
        if (m_connectAttemptInterval > 0)
        {
            std::stringstream ss;
            ss <<  "Waiting connect attempt interval (" << m_connectAttemptInterval << " sec)...";
            print(ss.str());

            // wait for given interval before next attempt, check once a second
            // if quit is requested
            for (int timer = 0; timer < m_connectAttemptInterval; timer++)
            {
                if (quit) return false;
                sleep(1);
            }
        }

        attempts++;
        std::stringstream ss;
        ss << (reconnect ? "Reconnecting " : "Connecting ");
        ss << "Mosquitto attempt #" << attempts << "...";
        print(ss.str());

        if (m_mosquitto->reconnect()) break;
        if (quit) return false;
    } while (1);

    reconnect ? print("Mosquitto reconnected") : print("Mosquitto connected");
    return true;
}

// sends hello message using mqtt
void BluetoothSensor::sendHello()
{
    std::string helloTopic = "sensor/" + m_sensorID + "/bluetooth/hello";
    m_mosquitto->publish(helloTopic.c_str(), "");
    m_mosquitto->loop();
}

void BluetoothSensor::print(std::string str, bool endl)
{
    std::cout << str;
    endl ? std::cout << std::endl : std::cout << std::flush;
}

void BluetoothSensor::printError(std::string str)
{
    std::cerr << "ERROR: " << str << std::endl;
}



