// Copyright (c) 2017 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.

/// @brief Command parsing helpers
#pragma once
#include <book/types.h>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <univalue.h>
#include <time.h>

std::string addressToStr(const struct sockaddr *sockaddr, socklen_t socklen);
std::string formatTime(const std::string& fmt, time_t t);
bool readJsonFile(const std::string& filename, UniValue& jval);
std::string isoTimeStr(time_t t);
int write_pid_file(const std::string& pidFn);

template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces=false)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(fSpaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }

    return rv;
}

template<typename T>
inline std::string HexStr(const T& vch, bool fSpaces=false)
{
    return HexStr(vch.begin(), vch.end(), fSpaces);
}

namespace orderentry
{
static const uint32_t INVALID_UINT32 = UINT32_MAX;
static const int32_t INVALID_INT32 = INT32_MAX;
}

