# EigenCore and ECMapper

EigenCore and ECMapper  are a pair of applications for the EigenLabs Eigenharp instruments intended to be an easy, but flexible alternative to the official EigenD software. "EigenCore" is a console app that uses the EigenLite API (https://github.com/TheTechnobear/EigenLite) to communicate with the hardware and then sends that data via OSC.
 
The other application, "EigenMapper" is a standalone/vst/au that receives the Eigenharp OSC from Core and converts it to midi. How physical keys should map to midi is configured per key in the UI.

Made with Juce 6 and Projucer.
