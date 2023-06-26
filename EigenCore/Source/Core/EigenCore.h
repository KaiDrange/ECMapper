#pragma once
#include <JuceHeader.h>
#include <unistd.h>
//#include <signal.h>
#include <thread>
#include "eigenapi_ec.h"
#include "APICallback.h"
#include "OSCCommunication.h"
#include "OSCMessageQueue.h"
#include "Enums.h"
#include "Common.h"
#include "FirmwareReader.h"

#define PROCESS_MICROSEC_SLEEP 100
//#define MEASURE_EIGENAPIPROCESSTIME
#define EIGENAPI_POLLTIME 100


class EigenCore {
public:
    EigenCore();
    ~EigenCore();
//    const juce::String getApplicationName() { return "EigenCore"; }
//    const juce::String getApplicationVersion() { return "1.0.3"; }
    void initialiseCore(juce::String ipString = "");
    void shutdownCore();
    
    static void turnOffAllLEDs(EigenApi::Eigenharp *api);
    static void turnOffAllLEDsForDevice(ConnectedDevice &device, EigenApi::Eigenharp *api);

private:
    FWR_InMem fwReader;
//    EigenApi::FWR_Posix fwReader;
    EigenApi::Eigenharp eigenApi;
    OSCCommunication osc;
    std::thread eigenApiProcessThread;
//    static void intHandler(int dummy);
    static void* eigenharpProcess(OSC::OSCMessageFifo *msgQueue, void* arg);
    void splitString(const juce::String &text, const juce::String &separator, juce::StringArray &tokens);
//    void makeThreadRealtime(std::thread& thread);
    
    const juce::String defaultIP = "127.0.0.1:12121";
    
    APICallback *apiCallback = nullptr;
    OSC::OSCMessageFifo oscSendQueue;
    OSC::OSCMessageFifo oscReceiveQueue;
    
//    void showHelpTextAndQuit();
};

static EigenCore *coreInstance = nullptr;
//volatile std::atomic<bool> exitThreads;
//std::atomic<bool> mapperConnected;
//std::list<ConnectedDevice> connectedDevices;



//START_JUCE_APPLICATION (EigenCore)
