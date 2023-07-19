# EigenCore and ECMapper

EigenCore and ECMapper  are a pair of applications for the EigenLabs Eigenharp instruments intended to be an easy, but flexible alternative to the official EigenD software. "EigenCore" is standalone/VST3 application that uses the EigenLite API (https://github.com/TheTechnobear/EigenLite) to communicate with the hardware instruments and then pass all data from keypresses, etc. as OSC data.
 
At the receiving end is "ECMapper" (EigenCore Mapper), a standalone/vst/au application that receives the Eigenharp OSC data from EigenCore and converts it to midi. How physical keys should map to midi is configured per key in the UI.

For now, only MacOS is supported. Made with Juce 7, Projucer and XCode.

## Credits:
This project is made possible only because of the open-source work of TheTechnoBear. EigenCore is completely dependant on his EigenLite API, and his work on "MEC", another alternative to EigenD, has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that community projects like these are even possible. ...and for making such fantastic instruments in the first place. :)

## Build instructions

MacOS:
Requirements:
    - Juce 7
    - XCode

## Note on Projucer and EigenCore
EigenCore expects picodecoder.dylib to be located in the app.bundle Frameworks folder. Whenever a new XCode project file is generated from Projucer, that means it is necessary to navigate to the build phases in EigenCore.xcodeproj and then:
- add the dylib (in the EigenLite folder) to the "Link Binary With Libraries" section
- add a new "Copy Files" build step and configure picodecoder to be copied to Frameworks

In Standalone mode, Juce automatically shows a "Audio input is muted to avoid feedback loop" notification and an options button until options are set. EigenCore doesn't actually need any options at the moment, and muted audio input is perfectly fine. So, a quick fix for hiding this is to set JUCE Modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h class NotificationArea height = 0 (at line 967).




## CMake Build

```
git submodule update --init --recursive
mkdir build
cd build
cmake ..
cmake --build .
```



