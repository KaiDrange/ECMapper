#include <JuceHeader.h>
#include <eigenapi.h>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include "APICallback.h"
#include "OSCCommunication.h"
#include "OSCMessageQueue.h"
#include "Enums.h"

#define PROCESS_MICROSEC_SLEEP 100
//#define MEASURE_EIGENAPIPROCESSTIME
#define EIGENAPI_POLLTIME 100


class EigenharpCore : public juce::JUCEApplication {
public:
    EigenharpCore();
    const juce::String getApplicationName() override { return "EigenharpCore"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }
    void initialise(const juce::String &) override;
    void shutdown() override;
    
    static void turnOffAllLEDs(EigenApi::Eigenharp *api);
    
private:
    EigenApi::Eigenharp eigenApi;
    OSCCommunication osc;
    std::thread eigenApiProcessThread;
    static void intHandler(int dummy);
    static void* eigenharpProcess(OSC::OSCMessageFifo *msgQueue, void* arg);
//    void makeThreadRealtime(std::thread& thread);
    
    const int senderPort = 7001;
    const int receiverPort = 7000;
    
    APICallback *apiCallback;
    OSC::OSCMessageFifo oscSendQueue;
    OSC::OSCMessageFifo oscReceiveQueue;
};

static EigenharpCore *coreInstance = nullptr;
volatile std::atomic<bool> exitThreads;
std::atomic<bool> mapperConnected;
std::atomic<DeviceType> currentDevice;

START_JUCE_APPLICATION (EigenharpCore)
