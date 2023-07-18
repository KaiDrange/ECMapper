#pragma once
#include <JuceHeader.h>
#include <unistd.h>
#include <thread>
#include <eigenapi_ec.h>
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
    void initialiseCore(juce::String ipString = "");
    void shutdownCore();
    bool isRunning();
    
    void turnOffAllLEDs();
    static void turnOffAllLEDs(EigenApi::Eigenharp *api);
    static void turnOffAllLEDsForDevice(ConnectedDevice &device, EigenApi::Eigenharp *api);

private:
    bool running = false;
    FWR_InMem fwReader;
    EigenApi::Eigenharp eigenApi;
    OSCCommunication osc;
    std::thread eigenApiProcessThread;
    static void* eigenharpProcess(OSC::OSCMessageFifo *msgQueue, void* arg);
    void splitString(const juce::String &text, const juce::String &separator, juce::StringArray &tokens);
    
    const juce::String defaultIP = "127.0.0.1:12121";
    
    APICallback *apiCallback = nullptr;
    OSC::OSCMessageFifo oscSendQueue;
    OSC::OSCMessageFifo oscReceiveQueue;
};

static EigenCore *coreInstance = nullptr;
