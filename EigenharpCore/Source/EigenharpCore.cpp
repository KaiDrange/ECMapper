#include "EigenharpCore.h"

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    std::cerr  << "int handler called";
    if(keepRunning==0) {
        sleep(1);
        exit(-1);
    }
    keepRunning = 0;
}

void* process(void* arg) {
    EigenApi::Eigenharp *pE = static_cast<EigenApi::Eigenharp*>(arg);
    while(keepRunning) {
        pE->process();
    }
    return nullptr;
}

void makeThreadRealtime(std::thread& thread) {

    int policy;
    struct sched_param param;

    pthread_getschedparam(thread.native_handle(), &policy, &param);
    param.sched_priority = 95;
    pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &param);

}

const int senderPort = 7001;
const int receiverPort = 7000;


//==============================================================================
EigenharpCore::EigenharpCore() : eigenApi("./") {
}

const juce::String EigenharpCore:: getApplicationName() { return "EigenharpCore"; }
const juce::String EigenharpCore::getApplicationVersion() { return "0.0.1"; }


void EigenharpCore::initialise(const juce::String &) {
    signal(SIGINT, intHandler);
    osc.connectSender("127.0.0.1", senderPort);
    osc.connectReceiver(receiverPort);

    eigenApi.setPollTime(100);
    eigenApi.addCallback(new APICallback(eigenApi, &osc));
    if(!eigenApi.start()) {
        std::cout  << "unable to start EigenLite";
    }

    eigenApiProcessThread = std::thread(process, &eigenApi);
}
void EigenharpCore::shutdown() {
    std::cout << "Trying to quit gracefully";
    keepRunning = 0;
    sleep(1);
    eigenApiProcessThread.join();
    eigenApi.stop();
    osc.disconnectReceiver();
    osc.disconnectSender();
}
