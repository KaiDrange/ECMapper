#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include "Enums.h"

namespace ecm::osc {

enum class MessageType : int {
    Undefined = 0,
    Device = 1,
    Key = 2,
    Breath = 3,
    Strip = 4,
    Pedal = 5,
    LED = 6,
    Ping = 7,
    Reset = 8,
    RequestLEDs = 9
};

struct Message {
    MessageType type = MessageType::Undefined;
    unsigned int course = 0;
    unsigned int key = 0;
    int active = 0;
    float pressure = 0.0f;
    float roll = 0.0f;
    float yaw = 0.0f;
    unsigned int strip = 0;
    unsigned int pedal = 0;
    float value = 0.0f;
    uint64_t timestamp = 0;
    int isRemote = 0;
    InstrumentType device = InstrumentType::None;
    char devId[64] = {0};
};

// Ensure the struct is safely copyable via int buffer if needed, 
// though we'll use a safer approach in the implementation.
static_assert(sizeof(Message) % sizeof(int) == 0, "Message struct size must be a multiple of sizeof(int)");
const int MessageSize = sizeof(Message) / sizeof(int);
const int QueueSize = 4096;

class MessageFifo {
public:
    void add(const Message& message);
    bool read(Message& message);
    int getMessageCount() const;
private:
    juce::AbstractFifo fifo { QueueSize };
    Message buffer[QueueSize];
};

} // namespace ecm::osc
