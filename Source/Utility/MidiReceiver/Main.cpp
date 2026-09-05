#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <iostream>
#include <iomanip>

class MidiReceiverApp : public juce::universal_midi_packets::EndpointsListener,
                        public juce::universal_midi_packets::Consumer
{
public:
    MidiReceiverApp()
    {
        session = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ReceiverSession");
        juce::universal_midi_packets::Endpoints::getInstance()->addListener(*this);
    }

    ~MidiReceiverApp() override
    {
        juce::universal_midi_packets::Endpoints::getInstance()->removeListener(*this);
        if (input.isAlive())
            input.removeConsumer(*this);
    }

    void endpointsChanged() override
    {
        std::cout << "Endpoints changed, checking connection..." << std::endl;
        findAndConnect();
    }

    void findAndConnect()
    {
        if (input.isAlive())
            return;

        auto endpointsList = juce::universal_midi_packets::Endpoints::getInstance()->getEndpoints();
        for (const auto& epId : endpointsList)
        {
            auto ep = juce::universal_midi_packets::Endpoints::getInstance()->getEndpoint(epId);
            if (ep.has_value() && ep->getName().contains("ECMapper Direct"))
            {
                std::cout << "Found ECMapper Direct source. Connecting..." << std::endl;
                
                input = session->connectInput(epId, juce::universal_midi_packets::PacketProtocol::MIDI_2_0);
                
                if (input.isAlive())
                {
                    input.addConsumer(*this);
                    std::cout << "Connected to: " << ep->getName() << " [" << epId.src << " / " << epId.dst << "]" << std::endl;
                    std::cout << "Waiting for MIDI data..." << std::endl;
                }
                else
                {
                    std::cout << "Failed to connect to " << ep->getName() << std::endl;
                }
                break;
            }
        }
    }

    void consume(juce::universal_midi_packets::Iterator begin, juce::universal_midi_packets::Iterator end, double time) override
    {
        for (auto it = begin; it != end; ++it)
        {
            const auto packet = *it;
            const auto type = juce::universal_midi_packets::Utils::getMessageType(packet[0]);
            
            std::cout << "[" << std::fixed << std::setprecision(3) << time << "] ";
            std::cout << "UMP Type " << std::hex << (int)type << std::dec << " (";
            
            if (type == juce::universal_midi_packets::Utils::MessageKind::channelVoice1)      std::cout << "MIDI 1.0";
            else if (type == juce::universal_midi_packets::Utils::MessageKind::channelVoice2) std::cout << "MIDI 2.0";
            else if (type == juce::universal_midi_packets::Utils::MessageKind::utility)       std::cout << "Utility";
            else if (type == juce::universal_midi_packets::Utils::MessageKind::commonRealtime) std::cout << "Common/Realtime";
            else if (type == juce::universal_midi_packets::Utils::MessageKind::sysex7)        std::cout << "SysEx7";
            else if (type == juce::universal_midi_packets::Utils::MessageKind::sysex8)        std::cout << "SysEx8";
            else if (type == juce::universal_midi_packets::Utils::MessageKind::stream)        std::cout << "Stream";
            else std::cout << "Other (" << std::hex << (int)type << std::dec << ")";
            
            std::cout << "): ";
            
            for (size_t i = 0; i < packet.size(); ++i)
                std::cout << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << packet[i] << (i == packet.size() - 1 ? "" : " ");
            
            std::cout << std::dec << std::endl;
        }
    }

    bool isConnected() const { return input.isAlive(); }

private:
    std::optional<juce::universal_midi_packets::Session> session;
    juce::universal_midi_packets::Input input;
};

int main(int /*argc*/, char* /*argv*/[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    
    std::cout << "========================================" << std::endl;
    std::cout << "   ECMapper MIDI Receiver Utility       " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Press Ctrl+C to exit." << std::endl;
    
    MidiReceiverApp app;
    app.findAndConnect();
    
    auto lastHeartbeat = juce::Time::getMillisecondCounter();
    auto lastDiscovery = juce::Time::getMillisecondCounter();
    
    while (! juce::MessageManager::getInstance()->hasStopMessageBeenSent())
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        
        auto now = juce::Time::getMillisecondCounter();
        
        // Heartbeat every 10 seconds
        if (now - lastHeartbeat > 10000)
        {
            std::cout << "[Heartbeat] Utility is running. Connected: " << (app.isConnected() ? "Yes" : "No") << std::endl;
            lastHeartbeat = now;
        }
        
        // Try discovery every 2 seconds if not connected
        if (! app.isConnected() && now - lastDiscovery > 2000)
        {
            app.findAndConnect();
            lastDiscovery = now;
        }
    }
    
    return 0;
}
