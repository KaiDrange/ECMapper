#pragma once

#include <unistd.h>
#include <signal.h>
#include <eigenapi.h>
#include <iostream>
#include <thread>

class APICallback: public EigenApi::Callback {
public:
    APICallback(EigenApi::Eigenharp& eh);
    virtual void device(const char* dev, DeviceType dt, int rows, int cols, int ribbons, int pedals);
    virtual void key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, unsigned p, int r, int y);
    virtual void breath(const char* dev, unsigned long long t, unsigned val);
    virtual void strip(const char* dev, unsigned long long t, unsigned strip, unsigned val);
    virtual void pedal(const char* dev, unsigned long long t, unsigned pedal, unsigned val);
    
    EigenApi::Eigenharp& eh_;
    bool led_;
    int max_[3], min_[3];
};
