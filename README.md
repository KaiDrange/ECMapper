# EigenCore and ECMapper

EigenCore and ECMapper are a pair of applications for the EigenLabs Eigenharp instruments intended to be an alternative to the official EigenD software. "EigenCore" is standalone/VST3 application that uses the EigenLite API (https://github.com/TheTechnobear/EigenLite) to communicate with the hardware instruments and then pass all data from keypresses, etc. as OSC data.
 
At the receiving end is "ECMapper" (EigenCore Mapper), a standalone/VST3 application that receives the Eigenharp OSC data from EigenCore and converts it to midi. How physical keys should map to midi is configured per key in the UI.

For now, only MacOS is supported. Made with Juce 7.

## Credits:
This project is made possible only because of the open-source work of TheTechnoBear. EigenCore is completely dependant on his EigenLite API, and his work on "MEC", another alternative to EigenD has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that community projects like these are even possible.

## CMake Build

```
git submodule update --init --recursive
mkdir build
cd build
cmake ..
cmake --build .
```
