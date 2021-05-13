#pragma once
#include <JuceHeader.h>
#include <stdio.h>

namespace OSC {

enum MessageType {
    Device = 1,
    Key = 2,
    Breath = 3,
    Strip = 4,
    Pedal = 5,
    LED = 6,
    Ping = 7
};

struct Message {
    MessageType type;
    unsigned int course = 0;
    unsigned int key = 0;
    int active = 0;
    unsigned int pressure = 0;
    int roll = 0;
    int yaw = 0;
    unsigned int strip = 0;
    unsigned int pedal = 0;
    unsigned int breath = 0;
    int colour = 0;
};

const int MessageSize = sizeof(Message)/sizeof(int);
const int queueSize = 1024;

class OSCMessageFifo {
public:
    void add(const Message *message);
    void read(Message *message);
    int getMessageCount() const;
private:
    juce::AbstractFifo fifo { queueSize * MessageSize };
    int buffer[queueSize * MessageSize];
};


}
