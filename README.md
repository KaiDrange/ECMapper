# EigenharpMapper

 A pair of applications for Eigenharps. "Core" is a console app that uses the EigenLite API (https://github.com/TheTechnobear/EigenLite) to communicate with the hardware and then sends that data via OSC.
 
The other application, "Mapper" is a standalone/vst/au that receives the Eigenharp OSC from Core and converts it to midi. How physical keys should map to midi is configured per key in the UI.

Made with Juce 6 and Projucer.
