# FX8010-Emulator-VST
Emulate an E-mu DSP (Soundblaster Audigy Cards)

The goal is to emulate a powerful interpreter for DSP assembler instructions within a 
DSP environment based on the FX8010 architecture, which was found on Soundblaster Live!
and Audigy cards, as VST effect plugin. 

The core functionality is given, but not unit tested. Syntax checker, register &
instructions mapper and VST integration still need to be done.

My testing has shown that it is possible to emulate a full DSP of this type (50MIPS) within a typical 
Audio block size of 512 samples / ~50ms latency. To put it a little more clearly: that corresponds 
to about 25 different effect plugins. Interestingly, in this implementation state the hardware
DSP is still around 50X faster than the CPU emulation (i5/3.2GHz) assuming the DSP latency is around 1ms.

2023 / klangraum
