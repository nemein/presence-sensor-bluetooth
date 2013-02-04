/*
    Common sensor code for Nemein's Smart Workplace
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#ifndef MOSQUITTOHANDLER_H
#define MOSQUITTOHANDLER_H

#include <iostream>
#include <vector>
#include <mosquitto.h>

struct mqttMessage
{
    std::string topic;
    std::string content;
};

class MosquittoHandler
{
public:

    MosquittoHandler();
    ~MosquittoHandler();

    bool init(std::string id);
    bool getSocket(int &socket);
    bool connectToBroker(const char* host, int port);
    bool waitForConnect();
    bool reconnect();
    bool subscribe(const char* subTopic);
    bool publish(const char* pubTopic, const char* text);
    bool loop();
    bool loopWrite();
    bool loopRead();
    bool isConnected();
    std::vector<mqttMessage> getArrivedMessages();
    std::string getLastErrorString();


    // callback functions
    void onConnect(int rc);
    void onDisconnect();
    void onMessage(const struct mosquitto_message *message);

private:

    static int numOfInstances;

    // static wrappers are needed to make member function callbacks work
    static void onConnectWrapper(void *obj, int rc);
    static void onDisconnectWrapper(void *obj);
    static void onMessageWrapper(void *obj, const struct mosquitto_message *message);

    /*
    void printError(int errorNum);
    void printError(std::string);
    */
    std::string errorByNum(int errorNum);

    struct mosquitto* m_mosquittoStruct;
    bool m_libInit;
    bool m_connected;

    std::vector<mqttMessage> m_arrivedMessages;

    std::string m_lastErrorString;
};

#endif // MOSQUITTOHANDLER_H
