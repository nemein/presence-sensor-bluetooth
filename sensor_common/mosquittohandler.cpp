/*
    Office presence sensors' common parts
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#include "mosquittohandler.h"

#include <stdio.h>
#include <string.h>

// how long connection ack is waited, in sec
const double CONNECT_TIMEOUT = 5.0;

const std::string ERROR_STRINGS[] = {"Success", "Out of memory", "Protocol error", "Invalid parameters", "Not connected", "Connection refused",
                                    "Not found", "Connection lost", "SSL error", "Invalid payload size", "Not supported", "Authentication error",
                                    "ACL denied", "Unknown error", "System call error"};

int MosquittoHandler::numOfInstances = 0;

MosquittoHandler::MosquittoHandler() :
    m_mosquittoStruct(NULL), m_libInit(false), m_connected(false)
{
}

MosquittoHandler::~MosquittoHandler()
{
    if(m_mosquittoStruct) {
        mosquitto_destroy(m_mosquittoStruct);
        m_mosquittoStruct = NULL;
    }

    numOfInstances--;
    if(numOfInstances == 0 && m_libInit) {
        mosquitto_lib_cleanup();
    }
    //std::cout << "Mosquitto cleaned" << std::endl;
}

bool MosquittoHandler::init(std::string id)
{
    int minor, major, revision;
    mosquitto_lib_version(&major, &minor, &revision);

    if(numOfInstances == 0) {
        mosquitto_lib_init();
    }
    m_libInit = true;
    numOfInstances++;

    void* obj = (void*) this;

    m_mosquittoStruct = mosquitto_new(id.c_str(), obj);
    if(!m_mosquittoStruct) {
        m_lastErrorString = "Cannot create new Mosquitto instance";
        return false;
    }

    mosquitto_log_init(m_mosquittoStruct, MOSQ_LOG_ERR || MOSQ_LOG_WARNING, MOSQ_LOG_STDOUT);
    mosquitto_connect_callback_set(m_mosquittoStruct, MosquittoHandler::onConnectWrapper);
    mosquitto_disconnect_callback_set(m_mosquittoStruct, MosquittoHandler::onDisconnectWrapper);
    mosquitto_message_callback_set(m_mosquittoStruct, MosquittoHandler::onMessageWrapper);

    return true;
}

bool MosquittoHandler::getSocket(int &socket)
{
    if(!m_mosquittoStruct) {
        m_lastErrorString = "Mosquitto not initialized";
        return false;
    }
    int sock = mosquitto_socket(m_mosquittoStruct);
    if(sock == -1) {
        m_lastErrorString = "Socket error";
        return false;
    }
    socket = sock;
    return true;
}

bool MosquittoHandler::connectToBroker(const char *host, int port)
{
    if(!m_mosquittoStruct) {
        m_lastErrorString = "Mosquitto not initialized";
        return false;
    }
    int errorNum = mosquitto_connect(m_mosquittoStruct, host, port, 60, true);
    if( errorNum != MOSQ_ERR_SUCCESS) {
        m_lastErrorString = errorByNum(errorNum);
        return false;
    }

    return true;
}

bool MosquittoHandler::waitForConnect()
{
    const clock_t start = clock();

    while (!m_connected)
    {
        loop();

        clock_t now = clock();
        clock_t delta = now - start;
        double seconds_elapsed = static_cast<double>(delta) / CLOCKS_PER_SEC;

        if (seconds_elapsed > CONNECT_TIMEOUT)
        {
            m_lastErrorString = "Cannot connect to broker";
            return false;
        }
    }
    m_lastErrorString = "";
    return true;
}

bool MosquittoHandler::reconnect()
{
    if(!m_mosquittoStruct) {
        m_lastErrorString = "Mosquitto not initialized.";
        return false;
    }
    int errorNum = mosquitto_reconnect(m_mosquittoStruct);
    if( errorNum != MOSQ_ERR_SUCCESS) {
        m_lastErrorString = errorByNum(errorNum);
        return false;
    }

    const clock_t start = clock();

    while (!m_connected)
    {
        loop();

        clock_t now = clock();
        clock_t delta = now - start;
        double seconds_elapsed = static_cast<double>(delta) / CLOCKS_PER_SEC;

        if (seconds_elapsed > CONNECT_TIMEOUT)
        {
            m_lastErrorString = "Cannot connect to broker";
            return false;
        }
    }

    m_lastErrorString = "";
    return true;
}

bool MosquittoHandler::subscribe(const char* subTopic)
{
    uint16_t mid = 1;

    int errorNum = mosquitto_subscribe(m_mosquittoStruct, &mid, subTopic, 0);

    if(errorNum != MOSQ_ERR_SUCCESS) {
        m_lastErrorString = errorByNum(errorNum);
        return false;
    }
    return true;
}

bool MosquittoHandler::publish(const char *pubTopic, const char *text)
{
    std::string content(text);
    if (content.size() == 0)
    {
        content = "<NO CONTENT>";
    }
    //std::cout << "Publishing " << content << " to topic " << pubTopic << std::endl;

    uint16_t mid = 1;

    int errorNum = mosquitto_publish(m_mosquittoStruct, &mid, pubTopic, strlen(text), (const uint8_t*)text, 0, 0);

    if(errorNum != MOSQ_ERR_SUCCESS) {
        m_lastErrorString = errorByNum(errorNum);
        return false;
    }
    return true;

}

bool MosquittoHandler::loop()
{
    if(!m_mosquittoStruct)
    {
        m_lastErrorString = "Mosquitto not initialized";
        return false;
    }

    int errorNum = mosquitto_loop(m_mosquittoStruct, 0);
    if(errorNum != MOSQ_ERR_SUCCESS) {
        m_lastErrorString = errorByNum(errorNum);
        return false;
    }
    m_lastErrorString = "";
    return true;
}

bool MosquittoHandler::loopWrite()
{
    if(!m_mosquittoStruct)
    {
        m_lastErrorString = "Mosquitto not initialized";
        return false;
    }

    int errorNum = mosquitto_loop_write(m_mosquittoStruct);
    if(errorNum != MOSQ_ERR_SUCCESS) {
        m_lastErrorString = errorByNum(errorNum);
        return false;
    }
    m_lastErrorString = "";
    return true;
}

bool MosquittoHandler::loopRead()
{
    if(!m_mosquittoStruct)
    {
        m_lastErrorString = "Mosquitto not initialized";
        return false;
    }

    int errorNum = mosquitto_loop_read(m_mosquittoStruct);
    if(errorNum != MOSQ_ERR_SUCCESS) {
        m_lastErrorString = errorByNum(errorNum);
        return false;
    }
    m_lastErrorString = "";
    return true;
}

bool MosquittoHandler::isConnected()
{
    return m_connected;
}

std::vector<mqttMessage> MosquittoHandler::getArrivedMessages()
{
    std::vector<mqttMessage> messages = m_arrivedMessages;
    m_arrivedMessages.clear();
    return messages;
}

std::string MosquittoHandler::getLastErrorString()
{
    return m_lastErrorString;
}

std::string MosquittoHandler::errorByNum(int errorNum)
{
    if (errorNum >= 0 && errorNum < 15) return ERROR_STRINGS[errorNum];
    return m_lastErrorString = "Unknown error";
}


void MosquittoHandler::onConnect(int rc)
{
    rc = 7; // prevent warning

    m_connected = true;
    //std::cout << "Mosquitto connected." << std::endl;
}

void MosquittoHandler::onDisconnect()
{
    m_connected = false;
    //std::cout << "Mosquitto disconnected." << std::endl;
}

void MosquittoHandler::onMessage(const struct mosquitto_message *message)
{
    //std::cout << "MosquittoHandler Message received." << std::endl;
    //std::cout << "Topic: " << message->topic << std::endl;
    //std::cout << "Content: " << message->payload << std::endl;
    //std::string destination = (char*) message->payload;
    //emit incomingMessage(std::string((char*)message->topic), std::string((char*)message->payload));

    mqttMessage newMessage;
    newMessage.topic = message->topic;
    if (message->payload == 0)
    {
        newMessage.content = "";
    }
    else
    {
        newMessage.content = (char*)message->payload;
    }
    std::string content;
    if (newMessage.content.size() == 0)
    {
        content = "<NO CONTENT>";
    }
    else
    {
        content = newMessage.content;
    }
    //std::cout << "Received mqtt message " << content << " in topic " << newMessage.topic << std::endl;

    m_arrivedMessages.push_back(newMessage);
}

void MosquittoHandler::onConnectWrapper(void *obj, int rc)
{
    MosquittoHandler* mh = (MosquittoHandler*) obj;
    mh->onConnect(rc);
}

void MosquittoHandler::onDisconnectWrapper(void *obj)
{
    MosquittoHandler* mh = (MosquittoHandler*) obj;
    mh->onDisconnect();
}

void MosquittoHandler::onMessageWrapper(void *obj, const struct mosquitto_message *message)
{
    MosquittoHandler* mh = (MosquittoHandler*) obj;
    mh->onMessage(message);
}

