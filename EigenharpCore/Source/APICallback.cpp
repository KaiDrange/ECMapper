#include "APICallback.h"

APICallback::APICallback(EigenApi::Eigenharp& eh) : eh_(eh), led_(false) {
    for(int i = 0;i<3;i++) { max_[i]=0; min_[i]=4096;}
}

void APICallback::device(const char* dev, DeviceType dt, int rows, int cols, int ribbons, int pedals) {
    std::cout << "device " << dev << " (" << dt << ") " << rows << " x " << cols << " strips " << ribbons << " pedals " << pedals << std::endl;
}

void APICallback::key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, unsigned p, int r, int y) {
    std::cout  << "key " << dev << " @ " << t << " - " << course << ":" << key << ' ' << a << ' ' << p << ' ' << r << ' ' << y << std::endl;
//        if(p >max_[0]) max_[0]=p;
//        if(r >max_[1]) max_[1]=r;
//        if(y >max_[2]) max_[2]=y;
//        if(p <min_[0]) min_[0]=p;
//        if(r <min_[1]) min_[1]=r;
//        if(y <min_[2]) min_[2]=y;
    
    
    bool led = course > 0;
    if (led_ != led) {
        led_ = led;
        eh_.setLED(dev,course, key,led_);
//            std::cout  << min_[0] << " to " << max_[0] << ", " << min_[1] << " to " << max_[1] << ", " << min_[2] << " to " << max_[2] << std::endl;
    }
}
void APICallback::breath(const char* dev, unsigned long long t, unsigned val) {
    std::cout  << "breath " << dev << " @ " << t << " - "  << val << std::endl;
}

void APICallback::strip(const char* dev, unsigned long long t, unsigned strip, unsigned val) {
    std::cout  << "strip " << dev << " @ " << t << " - " << strip << " = " << val << std::endl;
}

void APICallback::pedal(const char* dev, unsigned long long t, unsigned pedal, unsigned val) {
    std::cout  << "pedal " << dev << " @ " << t << " - " << pedal << " = " << val << std::endl;
}

