# EigenCore and ECMapper

EigenCore and ECMapper  are a pair of applications for the EigenLabs Eigenharp instruments intended to be an easy, but flexible alternative to the official EigenD software. "EigenCore" is a console app that uses the EigenLite API (https://github.com/TheTechnobear/EigenLite) to communicate with the hardware and then sends that data via OSC.
 
At the receiving end is "ECMapper" (EigenCore Mapper), a standalone/vst/au application that receives the Eigenharp OSC from EigenCore and converts it to midi. How physical keys should map to midi is configured per key in the UI.

Made with Juce 6 and Projucer.

# Credits:
This project is made possible only because of the open-source work of TheTechnoBear. EigenCore is completely dependant on his EigenLite API, and his work on "MEC", another alternative to EigenD, has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that community projects like these are even possible. ...and for making such fantastic instruments in the first place. :)

# Build instructions

MacOS for intel macs:
Requirements:
    - Juce 6
    - XCode

Open jucer files for both projects  in Juce Projucer, click the "Save and Open in IDE" button and compile.

Note: EigenCore needs files from the /EigenCore/EigenLite/resources to start and expects the folder to be in the same location as the executable. So copy that entire resources folder over to where your EigenCore executable is.

EigenLite is included as a prebuilt dynamic library build for Intel macs. For other builds, you will have to first build EigenLite and then replace the eigenapi dylib in EigenCore/EigenLite/resources with the newly built one. You will also need to download the appropriate libpicodecoder from somewhere. You can find them in MEC resources (https://github.com/TheTechnobear/MEC).
