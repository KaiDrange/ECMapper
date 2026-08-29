#pragma once
#include <JuceHeader.h>
#include "OSCMessage.h"
#include "Logger.h"

namespace ecm {

class OSCBridge : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>, 
                  private juce::Timer {
public:
    OSCBridge(osc::MessageFifo& hardwareToMapperQueue, 
              osc::MessageFifo& mapperToHardwareQueue, 
              osc::MessageFifo& outgoingOSCQueue,
              Logger& logger);
    ~OSCBridge() override;

    void setSenderEnabled(bool enabled);
    void setReceiverEnabled(bool enabled);
    
    void setSenderTarget(const juce::String& ip, int port);
    void setReceiverPort(int port);

    bool isSenderConnected() const { return senderConnected_; }
    bool isReceiverConnected() const { return receiverConnected_; }

private:
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void timerCallback() override;

    osc::MessageFifo& hardwareToMapperQueue_;
    osc::MessageFifo& mapperToHardwareQueue_;
    osc::MessageFifo& outgoingOSCQueue_;
    Logger& logger_;

    juce::OSCSender sender_;
    juce::OSCReceiver receiver_;
    
    bool senderEnabled_ = false;
    bool receiverEnabled_ = false;
    bool senderConnected_ = false;
    bool receiverConnected_ = false;

    juce::String senderIP_ = "127.0.0.1";
    int senderPort_ = 12120;
    int receiverPort_ = 12121;

    void sendOutgoingMessages();
    void sendPing();
    
    // We need another queue to mirror hardwareToMapperQueue_ without consuming it
    // Actually, OSCBridge should probably be another consumer of a separate "broadcast" queue
    // Or we just peak. But MessageFifo is a FIFO.
    // I'll add a method to MessageFifo to support multiple consumers or just have 
    // the producer push to both.
};

} // namespace ecm
