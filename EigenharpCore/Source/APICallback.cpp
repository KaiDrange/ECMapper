#include "APICallback.h"

APICallback::APICallback(EigenApi::Eigenharp& eh, OSCCommunication *osc) : eh_(eh) {
    this->osc = osc;
}

void APICallback::device(const char* dev, DeviceType dt, int rows, int cols, int ribbons, int pedals) {
    this->dev = dev;
    switch(cols) {
        case 2:
            modelName = "Pico";
            break;
        case 4:
            modelName = "Tau";
            break;
        case 5:
            modelName = "Alpha";
            break;
        default:
            modelName = "";
            break;
    }
//    
//    std::cout << "device " << dev << " (" << dt << ") " << rows << " x " << cols << " strips " << ribbons << " pedals " << pedals << std::endl;
}

void APICallback::key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, unsigned p, int r, int y) {
    std::cout  << "key " << dev << " @ " << t << " - " << course << ":" << key << ' ' << a << ' ' << p << ' ' << r << ' ' << y << std::endl;
    
    osc->sendKey(course, key, a, p, r, y);
//    bool led = course > 0;
//    if (led_ != led) {
//        led_ = led;
//        eh_.setLED(dev,course, key,led_);
//    }
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

