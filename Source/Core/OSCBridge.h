#pragma once
#include <JuceHeader.h>
#include "OSCMessage.h"
#include "Logger.h"

#include "HardwareService.h"

namespace ecm {

class OSCBridge : public HardwareService::Listener,
                  private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>, 
                  private juce::Timer {
public:
    OSCBridge(HardwareService& hardwareService,
              osc::MessageFifo& hardwareToMapperQueue, 
              osc::MessageFifo& mapperToHardwareQueue, 
              osc::MessageFifo& outgoingOSCQueue,
              Logger& logger);
    ~OSCBridge() override;
    
    // HardwareService::Listener overrides
    void deviceListChanged() override;

    void setSenderEnabled(bool enabled);
    void setReceiverEnabled(bool enabled);
    
    void setSenderTarget(const juce::String& ip, int port);
    void setReceiverPort(int port);

    bool isSenderConnected() const { return hostEnabled_; }
    bool isReceiverConnected() const { return hostEnabled_; }

private:
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void oscBundleReceived(const juce::OSCBundle& bundle) override {}
    void timerCallback() override;
    
    void updateConnections();
    void updateClientReceiver();

    HardwareService& hardwareService_;
    osc::MessageFifo& hardwareToMapperQueue_;
    osc::MessageFifo& mapperToHardwareQueue_;
    osc::MessageFifo& outgoingOSCQueue_;
    Logger& logger_;
    
    juce::OSCSender discoverySender_;
    juce::OSCReceiver discoveryReceiver_;
    
    struct Connection {
        std::string dev;
        ecm::InstrumentType type;
        ecm::DeviceMode mode;
        juce::String ip;
        int sendPort;
        int receivePort;
        std::unique_ptr<juce::OSCSender> sender;
        std::unique_ptr<juce::OSCReceiver> receiver;
    };
    
    std::vector<std::unique_ptr<Connection>> connections_;
    juce::CriticalSection connectionsLock_;
    bool hostEnabled_ = false;
    juce::String instanceId_;
    
    std::unique_ptr<juce::OSCReceiver> globalClientReceiver_;

    void sendOutgoingMessages();
    void sendPing(Connection* conn);
    
    // We need another queue to mirror hardwareToMapperQueue_ without consuming it
    // Actually, OSCBridge should probably be another consumer of a separate "broadcast" queue
    // Or we just peak. But MessageFifo is a FIFO.
    // I'll add a method to MessageFifo to support multiple consumers or just have 
    // the producer push to both.
};

} // namespace ecm
