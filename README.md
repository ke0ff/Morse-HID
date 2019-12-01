# Morse_HID_Keyboard
TIVA C-source and CCS6 Project for the TM4C123GH6PM LaunchPad.
Converts Morse paddle switch-inputs to Morse tones and interprets the characters for key processing
by a USB-HID interface stack.

Code speed, tone, and weight are user adjustable.  Supports iambic A/B, DIT-DAH (non-iambic), or straight key input.
System also supports a 5x4 button matrix for user defined buttons that may be placed as desired (for frequently used
or complex character keys).

See http://www.rollanet.org/~joeh/projects/musb/ for project hardware details.
