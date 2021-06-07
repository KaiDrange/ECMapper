# EigenCore and ECMapper

EigenCore and ECMapper  are a pair of applications for the EigenLabs Eigenharp instruments intended to be an easy, but flexible alternative to the official EigenD software. "EigenCore" is a console app that uses the EigenLite API (https://github.com/TheTechnobear/EigenLite) to communicate with the hardware and then sends that data via OSC.
 
At the receiving end is "ECMapper" (EigenCore Mapper), a standalone/vst/au application that receives the Eigenharp OSC from EigenCore and converts it to midi. How physical keys should map to midi is configured per key in the UI.

Made with Juce 6 and Projucer.

Credits:
This project is made possible only because of the open-source work of TheTechnoBear. EigenCore is completely dependant on his EigenLite API, and his work on "MEC", another alternative to EigenD, has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that these kinds of community projects are even possible. ...and for making my favourite instruments in the first place. :)
