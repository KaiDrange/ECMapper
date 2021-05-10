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
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    void initialise(const juce::String &) override;
    void shutdown() override;
private:
    EigenApi::Eigenharp eigenApi;
    OSCCommunication osc;
    std::thread eigenApiProcessThread;
};

START_JUCE_APPLICATION (EigenharpCore)
