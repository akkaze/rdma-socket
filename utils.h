#pragma once
#include <string>
#include <string.h>
#include <algorithm>
#include <tuple>
const int kPrintBuffer = 1 << 10U;
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

inline std::string SPrintf(const char *fmt, ...) {
    std::string msg(kPrintBuffer, '\0');
    va_list args;
    va_start(args, fmt);
    vsnprintf(&msg[0], kPrintBuffer, fmt, args);
    va_end(args);
    trim(msg);
    return msg;
}


inline std::tuple<int, int> ParseIndex(std::string msg) {
    std::string client_index, msg_index;
    int i = 0;
    int k = 0;
    while (i < msg.size()) {
        if (k == 0) {
            while (isdigit(msg[i])) {
                client_index += msg[i];
                i++;
                k = 1;
            }
        } else {
            while (isdigit(msg[i])) {
                msg_index += msg[i];
                i++;
            }
        }
        i++;
    }
    return std::make_tuple(atoi(client_index.c_str()),
        atoi(msg_index.c_str()));
}

