#include <JuceHeader.h>
#include <eigenapi.h>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include "APICallback.h"
#include "OSCCommunication.h"

class EigenharpCore : public juce::JUCEApplication {
public:
    EigenharpCore();
    const juce::String getApplicationName() override { return "EigenharpCore"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }
    void initialise(const juce::String &) override;
    void shutdown() override;

private:
    EigenApi::Eigenharp eigenApi;
    OSCCommunication osc;
    std::thread eigenApiProcessThread;
    static void intHandler(int dummy);
    static void* process(void* arg);
    void makeThreadRealtime(std::thread& thread);
    
    const int senderPort = 7001;
    const int receiverPort = 7000;

};

static EigenharpCore *coreInstance;
static volatile int keepRunning = 1;

START_JUCE_APPLICATION (EigenharpCore)
