/*
    Office presence sensors' common parts
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#include "datagetter.h"

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

DataGetter::DataGetter() : m_handle(0)
{

}

DataGetter::~DataGetter()
{
    shutdown();
}

bool DataGetter::init()
{
    if (m_handle) return true;

    m_handle = curl_easy_init();
    if (!m_handle) return false;

    return true;
}

void DataGetter::shutdown()
{
    if (m_handle)
    {
        curl_easy_cleanup(m_handle);
    }
}

bool DataGetter::get(std::string url, std::string& data)
{
    if (!m_handle) return false;

    CURLcode response;
    curl_easy_setopt(m_handle, CURLOPT_URL, url.c_str());

    curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, &data);

    response = curl_easy_perform(m_handle);

    if (response != CURLE_OK)
    {
        m_lastErrorString = curl_easy_strerror(response);
        return false;
    }
    return true;
}

std::string DataGetter::getLastErrorString()
{
    return m_lastErrorString;
}
