#include "EigenharpCore.h"

EigenharpCore::EigenharpCore() : eigenApi("./") {
    jassert(coreInstance == nullptr);
    coreInstance = this;
}

void EigenharpCore::initialise(const juce::String &) {
    signal(SIGINT, EigenharpCore::intHandler);
    osc.addListener(this);
    osc.connectSender("127.0.0.1", senderPort);
    osc.connectReceiver(receiverPort);

    eigenApi.setPollTime(100);
    apiCallback = new APICallback(eigenApi, &osc);
    eigenApi.addCallback(apiCallback);
    if(!eigenApi.start()) {
        std::cout  << "unable to start EigenLite" << std::endl;
    }

    eigenApiProcessThread = std::thread(coreInstance->process, &eigenApi);
}

void EigenharpCore::shutdown() {
    std::cout << "Trying to quit gracefully..." << std::endl;
    if(keepRunning==0) {
        sleep(1);
        exit(-1);
    }
    keepRunning = 0;
    eigenApiProcessThread.join();
    eigenApi.stop();
    osc.disconnectReceiver();
    osc.disconnectSender();
    
    delete apiCallback;
}

void EigenharpCore::intHandler(int dummy) {
    std::cerr  << "int handler called" << std::endl;
    coreInstance->systemRequestedQuit();
}

void* EigenharpCore::process(void* arg) {
    EigenApi::Eigenharp *pE = static_cast<EigenApi::Eigenharp*>(arg);
    while(keepRunning) {
        pE->process();
    }
    return nullptr;
}

void EigenharpCore::makeThreadRealtime(std::thread& thread) {

    int policy;
    struct sched_param param;

    pthread_getschedparam(thread.native_handle(), &policy, &param);
    param.sched_priority = 95;
    pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &param);

}

void EigenharpCore::keyLEDChanged(OSCCommunication*, int course, int key, int colour) {
    if (apiCallback->modelName.isNotEmpty())
        eigenApi.setLED(apiCallback->dev, course, key, colour);
}


