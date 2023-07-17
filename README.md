# FX8010-Emulator-VST
Emulate an E-mu DSP (Soundblaster Audigy Cards)

The goal is to emulate a powerful interpreter for DSP assembler instructions within a 
DSP environment based on the FX8010 architecture, which was found on Soundblaster Live!
and Audigy cards, as VST effect plugin. 

The core functionality is given, but not unit tested. Syntax checker, register &
instructions mapper and VST integration still need to be done.

My testing has shown that we can emulate a full DSP of this type (50MIPS) within a typical 
Audio block size of 512 samples / ~50ms latency. To put it a little more clearly: that corresponds 
to about 25 different plugins. Interestingly, in this state of implementation, the hardware 
DSP is still about 50x faster than the CPU emulation (i5 / 3.2GhZ). 

2023 / klangraum
