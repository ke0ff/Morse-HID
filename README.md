# Morse_HID_Keyboard
TIVA C-source and CCS6 Project for the TM4C123GH6PM LaunchPad.
Converts Morse paddle switch-inputs to Morse tones and interprets the characters for key processing
by a USB-HID interface stack.

Code speed, tone, and weight are user adjustable.  Supports iambic A/B, DIT-DAH (non-iambic), or straight key input.
System also supports a 5x4 button matrix for user defined buttons that may be placed as desired (for frequently used
or complex character keys).

musb.zip is a CCS6.2 exported project.  It requires CCS6 version 2+ and TivaWare V2.1.4.178 (both are available at http://www.ti.com/).
The TivaWare package must be installed in the "C:\ti\" folder.  If these files are located differently, CCS6 should locate them, but
if errors are encountered, the file location variables within CCS6 will have to be edited manually.

Use the "Import" command inside CCS6 to import the project, then select the "musb.zip" file.  When finished, the project should
compile without error.  Use the "debug" command to program a connected to a TM4C123GXL Launchpad board.

See http://www.rollanet.org/~joeh/projects/musb/ for project hardware details.
