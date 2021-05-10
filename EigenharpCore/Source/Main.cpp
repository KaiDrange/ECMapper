/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <eigenapi.h>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include "APICallback.h"

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



//==============================================================================
int main (int argc, char* argv[])
{

    signal(SIGINT, intHandler);
    EigenApi::Eigenharp myD("./");
    myD.setPollTime(100);
    myD.addCallback(new APICallback(myD));
    if(!myD.start())
    {
        std::cout  << "unable to start EigenLite";
        return -1;
    }

    std::thread t=std::thread(process, &myD);
//    makeThreadRealtime(t);
    t.join();

    myD.stop();
    return 0;


}
