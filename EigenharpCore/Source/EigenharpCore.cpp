#include "EigenharpCore.h"

EigenharpCore::EigenharpCore() : eigenApi("./") {
    jassert(coreInstance == nullptr);
    coreInstance = this;
}

void EigenharpCore::initialise(const juce::String &) {
    signal(SIGINT, EigenharpCore::intHandler);
    osc.connectSender("127.0.0.1", senderPort);
    osc.connectReceiver(receiverPort);

    eigenApi.setPollTime(100);
    eigenApi.addCallback(new APICallback(eigenApi, &osc));
    if(!eigenApi.start()) {
        std::cout  << "unable to start EigenLite";
    }

    eigenApiProcessThread = std::thread(coreInstance->process, &eigenApi);
}

void EigenharpCore::shutdown() {
    std::cout << "Trying to quit gracefully";
    if(keepRunning==0) {
        sleep(1);
        exit(-1);
    }
    keepRunning = 0;
    eigenApiProcessThread.join();
    eigenApi.stop();
    osc.disconnectReceiver();
    osc.disconnectSender();
}

void EigenharpCore::intHandler(int dummy) {
    std::cerr  << "int handler called";
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

