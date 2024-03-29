#include "EigenCore.h"

EigenCore::EigenCore() : eigenApi(fwReader), osc(&oscSendQueue, &oscReceiveQueue) {
    jassert(coreInstance == nullptr);
    coreInstance = this;
    std::cout << "EigenCore v1.0.3" << std::endl;
}

EigenCore::~EigenCore() {
    if (apiCallback != nullptr)
        delete apiCallback;
}

void EigenCore::initialiseCore(juce::String ipString) {
    if (running)
        return;
    
    running = true;
    exitThreads = false;
    if (!fwReader.confirmResources()) {
        std::cout << "IHX files not properly configured." << std::endl;
        return;
    }
    
    if (ipString == "")
        ipString = defaultIP;
    
    if (!exitThreads) {
        juce::StringArray ipAndPortNo;
        splitString(ipString, ":", ipAndPortNo);
        if (ipAndPortNo.size() == 2) {
            osc.connectSender(ipAndPortNo[0], ipAndPortNo[1].getIntValue());
            osc.connectReceiver(ipAndPortNo[1].getIntValue() - 1);
        }
    }
    
    if (!exitThreads) {
        eigenApi.setPollTime(EIGENAPI_POLLTIME);
        apiCallback = new APICallback(eigenApi, &oscSendQueue);
        eigenApi.addCallback(apiCallback);
        if(!eigenApi.start()) {
            std::cout << "Unable to start EigenLite" << std::endl;
        }
        
        eigenApiProcessThread = std::thread(coreInstance->eigenharpProcess, &oscReceiveQueue, &eigenApi);
    }
}

void EigenCore::shutdownCore() {
    if (!running)
        return;
    
    running = false;
    std::cout << "Shutting down..." << std::endl;
    turnOffAllLEDs(&eigenApi);
    osc.disconnectReceiver();
    osc.disconnectSender();
    sleep(1);
    exitThreads = true;
    sleep(1);
    if (eigenApiProcessThread.joinable())
        eigenApiProcessThread.join();
    eigenApi.stop();
    
    if (apiCallback != nullptr) {
        delete apiCallback;
        apiCallback = nullptr;
    }
}

void EigenCore::turnOffAllLEDs() {
    turnOffAllLEDs(&eigenApi);
}

void EigenCore::turnOffAllLEDs(EigenApi::Eigenharp *api) {
    for (auto i = begin(connectedDevices); i != end(connectedDevices); i++) {
        turnOffAllLEDsForDevice(*i, api);
    }
}

void EigenCore::turnOffAllLEDsForDevice(ConnectedDevice &device, EigenApi::Eigenharp *api) {
    int course0Length = 0;
    int course1Start = 0;
    int course1Length = 0;
    switch (device.type) {
        case EHDeviceType::Pico:
            course0Length = 18;
            course1Length = 4;
            break;
        case EHDeviceType::Tau:
            course0Length = 72 + 12;
            course1Length = 8;
            course1Start = 5;
            break;
        case EHDeviceType::Alpha:
            course0Length = 120;
            course1Length = 12;
            break;
        default:
            break;
    }

    for (int i = 0; i < course0Length; i++) {
        try {
            api->setLED(device.dev, 0, i, 0);
        } catch (...) {
            std::cout << "Set LED failed: device " << device.dev << " course 0 key " << i << std::endl;
        }
        device.assignedLEDColours[0][i] = 0;
        device.activeKeys[0][i] = false;
    }
    for (int i = course1Start; i < course1Start + course1Length; i++) {
        try {
            api->setLED(device.dev, 1, i, 0);
        } catch (...) {
            std::cout << "Set LED failed: device " << device.dev << " course 1 key " << i << std::endl;
        }
        device.assignedLEDColours[1][i] = 0;
        device.activeKeys[1][i] = false;
    }
}

void* EigenCore::eigenharpProcess(OSC::OSCMessageFifo *msgQueue, void* arg) {
    EigenApi::Eigenharp *pE = static_cast<EigenApi::Eigenharp*>(arg);
    while(!exitThreads) {

#ifdef MEASURE_EIGENAPIPROCESSTIME
        static int counter = 0;
        auto begin = std::chrono::high_resolution_clock::now();
#endif
        static bool prevMapperConnectedState = false;
        if (mapperConnected != prevMapperConnectedState) {
            if (mapperConnected == false)
                turnOffAllLEDs(pE);
            prevMapperConnectedState = mapperConnected;
        }
        
//        if (mapperConnected) {
            try {
                pE->process();
            }
            catch (...) {
                std::cout << "EigenAPI Process threw an exception." << std::endl;
            }
            static OSC::Message msg;
            while (msgQueue->getMessageCount() > 0) {
                msgQueue->read(&msg);
                if (msg.type == OSC::MessageType::LED) {
                    for (auto i = begin(connectedDevices); i != end(connectedDevices); i++) {
                        if (msg.device == i->type) {
                            i->assignedLEDColours[msg.course][msg.key] = msg.value;
                            try {
                                pE->setLED(i->dev, msg.course, msg.key, msg.value);
                            }
                            catch (...) {
                                std::cout << "Tried to SetLED because of LED OSC msg, but got an exception." << std::endl;
                            }
                            break;
                        }
                    }
                }
                else if (msg.type == OSC::MessageType::Reset) {
                    for (auto i = begin(connectedDevices); i != end(connectedDevices); i++) {
                        if (msg.device == i->type) {
                            turnOffAllLEDsForDevice(*i, pE);
                            break;
                        }
                    }
                }
            }
//        }
        
#ifdef MEASURE_EIGENAPIPROCESSTIME
        auto end = std::chrono::high_resolution_clock::now();
        if (counter%1000 == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin);
            std::cout << "EigenharpProcess time: " << elapsed.count() << std::endl;
        }
#endif
        
        std::this_thread::sleep_for(std::chrono::microseconds(PROCESS_MICROSEC_SLEEP));
//        std::this_thread::sleep_for(std::chrono::microseconds(PROCESS_MICROSEC_SLEEP + 100000*!mapperConnected));
    }
    return nullptr;
}

void EigenCore::splitString(const juce::String &text, const juce::String &separator, juce::StringArray &tokens) {
    tokens.addTokens (text, separator, "\"");
}

bool EigenCore::isRunning() {
    return running;
}
