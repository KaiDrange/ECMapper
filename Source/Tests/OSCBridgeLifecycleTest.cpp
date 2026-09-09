#include <JuceHeader.h>

#include "Core/HardwareService.h"
#include "Core/Logger.h"
#include "Core/OSCBridge.h"

namespace {

int findAvailablePort() {
    constexpr int startPort = 24000;
    constexpr int endPort = 24100;

    for (int port = startPort; port < endPort; ++port) {
        if (!ecm::OSCBridge::isPortOccupied(port)) {
            return port;
        }
    }

    return 0;
}

bool expect(bool condition, const juce::String& message) {
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

bool waitUntil(const std::function<bool()>& condition, int timeoutMs) {
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(timeoutMs);

    while (!condition()) {
        if (juce::Time::getMillisecondCounter() >= deadline)
            return condition();

        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    return true;
}

bool sendResetMessage(int port) {
    juce::OSCSender sender;
    if (!sender.connect("127.0.0.1", port))
        return false;

    const bool sent = sender.send("/ECMapper/reset", static_cast<int>(ecm::InstrumentType::None), juce::String("test-device"));
    sender.disconnect();
    return sent;
}

bool sendDiscoveryMessage(ecm::InstrumentType type, int port, const juce::String& devId, const juce::String& senderId) {
    juce::OSCSender sender;
    if (!sender.connect("127.0.0.1", 12121))
        return false;

    const bool sent = sender.send("/EigenCore/device", static_cast<int>(type), juce::String("127.0.0.1"), port, senderId, devId);
    sender.disconnect();
    return sent;
}

class CountingListener : public ecm::HardwareService::Listener {
public:
    void deviceListChanged() override {}

    void deviceNeedsLEDSync(const std::string& devId, ecm::InstrumentType, bool) override {
        const juce::ScopedLock lock(lock_);
        ++syncCount_;
        lastDevId_ = devId;
    }

    int getSyncCount() const {
        const juce::ScopedLock lock(lock_);
        return syncCount_;
    }

    std::string getLastDevId() const {
        const juce::ScopedLock lock(lock_);
        return lastDevId_;
    }

private:
    mutable juce::CriticalSection lock_;
    int syncCount_ = 0;
    std::string lastDevId_;
};

int countRemoteDevices(ecm::HardwareService& hardwareService, const std::string& remoteDevId) {
    int count = 0;

    for (const auto& device : hardwareService.getConnectedDevices()) {
        if (device.isRemote && device.remoteOriginalDevId == remoteDevId)
            ++count;
    }

    return count;
}

int drainQueue(ecm::osc::MessageFifo& queue) {
    int messageCount = 0;
    ecm::osc::Message message;

    while (queue.read(message))
        ++messageCount;

    return messageCount;
}

bool runPortReleaseScenario(int port) {
    ecm::osc::MessageFifo hardwareToMapperQueue;
    ecm::osc::MessageFifo mapperToHardwareQueue;
    ecm::osc::MessageFifo outgoingOscQueue;
    ecm::Logger logger(false, false);
    ecm::HardwareService hardwareService(hardwareToMapperQueue, mapperToHardwareQueue);
    hardwareService.setClientListenSettings("127.0.0.1", port);

    {
        ecm::OSCBridge bridge(hardwareService, hardwareToMapperQueue, mapperToHardwareQueue, outgoingOscQueue, logger);

        if (!expect(!ecm::OSCBridge::isPortOccupied(port), "Expected an unused port before enabling the receiver."))
            return false;

        bridge.setReceiverEnabled(true);
        if (!expect(ecm::OSCBridge::isPortOccupied(port), "Expected the client receiver to bind the listen port when enabled."))
            return false;

        bridge.setReceiverEnabled(false);
        if (!expect(!ecm::OSCBridge::isPortOccupied(port), "Expected the client receiver to release the listen port when disabled."))
            return false;
    }

    if (!expect(!ecm::OSCBridge::isPortOccupied(port), "Expected the listen port to stay free after the first bridge is torn down."))
        return false;

    ecm::OSCBridge secondBridge(hardwareService, hardwareToMapperQueue, mapperToHardwareQueue, outgoingOscQueue, logger);
    secondBridge.setReceiverEnabled(true);

    if (!expect(ecm::OSCBridge::isPortOccupied(port), "Expected a new bridge instance to reacquire the released listen port."))
        return false;

    secondBridge.setReceiverEnabled(false);
    return expect(!ecm::OSCBridge::isPortOccupied(port), "Expected the second bridge to release the listen port when disabled.");
}

bool runOverlappingActivationScenario(int port) {
    ecm::osc::MessageFifo firstHardwareToMapperQueue;
    ecm::osc::MessageFifo firstMapperToHardwareQueue;
    ecm::osc::MessageFifo firstOutgoingOscQueue;
    ecm::osc::MessageFifo secondHardwareToMapperQueue;
    ecm::osc::MessageFifo secondMapperToHardwareQueue;
    ecm::osc::MessageFifo secondOutgoingOscQueue;
    ecm::Logger logger(false, false);
    ecm::HardwareService firstHardwareService(firstHardwareToMapperQueue, firstMapperToHardwareQueue);
    ecm::HardwareService secondHardwareService(secondHardwareToMapperQueue, secondMapperToHardwareQueue);
    firstHardwareService.setClientListenSettings("127.0.0.1", port);
    secondHardwareService.setClientListenSettings("127.0.0.1", port);

    ecm::OSCBridge firstBridge(firstHardwareService, firstHardwareToMapperQueue, firstMapperToHardwareQueue, firstOutgoingOscQueue, logger);
    ecm::OSCBridge secondBridge(secondHardwareService, secondHardwareToMapperQueue, secondMapperToHardwareQueue, secondOutgoingOscQueue, logger);

    firstBridge.setReceiverEnabled(true);
    if (!expect(ecm::OSCBridge::isPortOccupied(port), "Expected the first bridge to acquire the listen port."))
        return false;

    secondBridge.setReceiverEnabled(true);

    if (!expect(sendResetMessage(port), "Expected to send a reset message to the shared listen port.")) {
        firstBridge.setReceiverEnabled(false);
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    if (!expect(waitUntil([&firstMapperToHardwareQueue, &secondMapperToHardwareQueue]() {
                    return firstMapperToHardwareQueue.getMessageCount() == 1
                           && secondMapperToHardwareQueue.getMessageCount() == 1;
                }, 500),
                "Expected both overlapping bridge instances to receive OSC messages while sharing the same listener.")) {
        firstBridge.setReceiverEnabled(false);
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    firstBridge.setReceiverEnabled(false);

    if (!expect(waitUntil([port]() { return ecm::OSCBridge::isPortOccupied(port); }, 1500),
                "Expected the second bridge to acquire the listen port after the first bridge releases it.")) {
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    drainQueue(firstMapperToHardwareQueue);
    drainQueue(secondMapperToHardwareQueue);

    if (!expect(sendResetMessage(port), "Expected to send a reset message after disabling the first bridge.")) {
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    if (!expect(waitUntil([&secondMapperToHardwareQueue]() {
                    return secondMapperToHardwareQueue.getMessageCount() == 1;
                }, 500),
                "Expected the second bridge to keep receiving OSC messages after the first bridge is disabled.")) {
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    if (!expect(firstMapperToHardwareQueue.getMessageCount() == 0,
                "Expected the disabled first bridge to stop receiving OSC messages immediately.")) {
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    secondBridge.setReceiverEnabled(false);
    return expect(waitUntil([port]() { return !ecm::OSCBridge::isPortOccupied(port); }, 500),
                  "Expected the overlapping activation scenario to release the listen port when the second bridge is disabled.");
}

bool runSharedDiscoveryScenario(int port) {
    ecm::osc::MessageFifo firstHardwareToMapperQueue;
    ecm::osc::MessageFifo firstMapperToHardwareQueue;
    ecm::osc::MessageFifo firstOutgoingOscQueue;
    ecm::osc::MessageFifo secondHardwareToMapperQueue;
    ecm::osc::MessageFifo secondMapperToHardwareQueue;
    ecm::osc::MessageFifo secondOutgoingOscQueue;
    ecm::Logger logger(false, false);
    ecm::HardwareService firstHardwareService(firstHardwareToMapperQueue, firstMapperToHardwareQueue);
    ecm::HardwareService secondHardwareService(secondHardwareToMapperQueue, secondMapperToHardwareQueue);
    firstHardwareService.setClientListenSettings("127.0.0.1", port);
    secondHardwareService.setClientListenSettings("127.0.0.1", port);

    ecm::OSCBridge firstBridge(firstHardwareService, firstHardwareToMapperQueue, firstMapperToHardwareQueue, firstOutgoingOscQueue, logger);
    ecm::OSCBridge secondBridge(secondHardwareService, secondHardwareToMapperQueue, secondMapperToHardwareQueue, secondOutgoingOscQueue, logger);

    firstBridge.setReceiverEnabled(true);
    secondBridge.setReceiverEnabled(true);

    if (!expect(sendDiscoveryMessage(ecm::InstrumentType::Pico, port - 1, "test-device", "host-instance"),
                "Expected to send a discovery message to the OSC discovery port.")) {
        firstBridge.setReceiverEnabled(false);
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    if (!expect(waitUntil([&firstHardwareService]() {
                    return countRemoteDevices(firstHardwareService, "test-device") == 1;
                }, 500),
                "Expected the first bridge instance to discover the remote host.")) {
        firstBridge.setReceiverEnabled(false);
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    if (!expect(waitUntil([&secondHardwareService]() {
                    return countRemoteDevices(secondHardwareService, "test-device") == 1;
                }, 500),
                "Expected the overlapping second bridge instance to discover the remote host without toggling modes.")) {
        firstBridge.setReceiverEnabled(false);
        secondBridge.setReceiverEnabled(false);
        return false;
    }

    firstBridge.setReceiverEnabled(false);
    secondBridge.setReceiverEnabled(false);
    return true;
}

bool runLedHandoverScenario(int port) {
    ecm::osc::MessageFifo firstHardwareToMapperQueue;
    ecm::osc::MessageFifo firstMapperToHardwareQueue;
    ecm::osc::MessageFifo firstOutgoingOscQueue;
    ecm::osc::MessageFifo secondHardwareToMapperQueue;
    ecm::osc::MessageFifo secondMapperToHardwareQueue;
    ecm::osc::MessageFifo secondOutgoingOscQueue;
    ecm::Logger logger(false, false);
    ecm::HardwareService firstHardwareService(firstHardwareToMapperQueue, firstMapperToHardwareQueue);
    ecm::HardwareService secondHardwareService(secondHardwareToMapperQueue, secondMapperToHardwareQueue);
    CountingListener firstListener;
    CountingListener secondListener;
    firstHardwareService.addListener(&firstListener);
    secondHardwareService.addListener(&secondListener);
    firstHardwareService.setClientListenSettings("127.0.0.1", port);
    secondHardwareService.setClientListenSettings("127.0.0.1", port);

    ecm::OSCBridge firstBridge(firstHardwareService, firstHardwareToMapperQueue, firstMapperToHardwareQueue, firstOutgoingOscQueue, logger);
    ecm::OSCBridge secondBridge(secondHardwareService, secondHardwareToMapperQueue, secondMapperToHardwareQueue, secondOutgoingOscQueue, logger);

    firstBridge.setReceiverEnabled(true);
    secondBridge.setReceiverEnabled(true);

    if (!expect(sendDiscoveryMessage(ecm::InstrumentType::Pico, port - 1, "test-device", "host-instance"),
                "Expected to send a discovery message for the LED handover scenario.")) {
        firstBridge.setReceiverEnabled(false);
        secondBridge.setReceiverEnabled(false);
        firstHardwareService.removeListener(&firstListener);
        secondHardwareService.removeListener(&secondListener);
        return false;
    }

    if (!expect(waitUntil([&firstListener, &secondListener]() {
                    return firstListener.getSyncCount() == 1 && secondListener.getSyncCount() == 1;
                }, 500),
                "Expected both overlapping client instances to request an initial LED sync after discovery.")) {
        firstBridge.setReceiverEnabled(false);
        secondBridge.setReceiverEnabled(false);
        firstHardwareService.removeListener(&firstListener);
        secondHardwareService.removeListener(&secondListener);
        return false;
    }

    firstBridge.setReceiverEnabled(false);

    if (!expect(waitUntil([&secondListener]() {
                    return secondListener.getSyncCount() >= 2;
                }, 500),
                "Expected the remaining client instance to request another LED sync after takeover.")) {
        secondBridge.setReceiverEnabled(false);
        firstHardwareService.removeListener(&firstListener);
        secondHardwareService.removeListener(&secondListener);
        return false;
    }

    if (!expect(juce::String(secondListener.getLastDevId()).startsWith("Remote-test-device@127.0.0.1"),
                "Expected the LED handover sync to target the discovered remote device.")) {
        secondBridge.setReceiverEnabled(false);
        firstHardwareService.removeListener(&firstListener);
        secondHardwareService.removeListener(&secondListener);
        return false;
    }

    secondBridge.setReceiverEnabled(false);
    firstHardwareService.removeListener(&firstListener);
    secondHardwareService.removeListener(&secondListener);
    return true;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const int port = findAvailablePort();
    if (!expect(port != 0, "Could not find a free UDP port for the OSC bridge lifecycle test."))
        return 1;

    if (!runPortReleaseScenario(port))
        return 1;

    if (!runOverlappingActivationScenario(port))
        return 1;

    if (!runSharedDiscoveryScenario(port + 1))
        return 1;

    if (!runLedHandoverScenario(port + 2))
        return 1;

    std::cout << "OSCBridgeLifecycleTest passed on port " << port << std::endl;
    return 0;
}