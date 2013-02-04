/*
    Office presence sensors' common parts
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#ifndef DATAGETTER_H
#define DATAGETTER_H

#include <iostream>
#include <string>
#include "curl/curl.h"

class DataGetter
{
public:
    DataGetter();
    ~DataGetter();

    bool init();
    void shutdown();

    bool get(std::string url, std::string &data);
    std::string getLastErrorString();

private:
    CURL* m_handle;

    std::string m_lastErrorString;
    std::string m_responseStr;
};

#endif // DATAGETTER_H
