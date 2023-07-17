# FX8010-Emulator-VST
Emulate an E-mu DSP (Soundblaster Audigy Cards)

The goal is to emulate a powerful interpreter for DSP assembler instructions within a 
DSP environment based on the FX8010 architecture, which was found on Soundblaster Live!
and Audigy cards, as VST effect plugin. 

The core functionality is given, but not unit tested. Syntax checker, register &
instructions mapper and VST integration still need to be done.

My testing has shown that it actually can run 10000 instructions / ms or 10 MIPS on a single core. 
To put it a little more clearly: that corresponds to about 10 different effect plugins. 
Interestingly, in this implementation state the hardware DSP is still around 5X faster (50 MIPS) than the CPU emulation (i5/3.2GHz).
However, if we factor in the multi-core processing, we're pretty close to the hardware.

A special shout out to ChatGPT which provides an excellent assistant and talks about every topic in great detail.

[3rd Party Docs/FX8010 - A DSP Chip Architecture for Audio Effects (1998).pdf](https://github.com/kxproject/kX-Audio-driver-Documentation/blob/master/3rd%20Party%20Docs/FX8010%20-%20A%20DSP%20Chip%20Architecture%20for%20Audio%20Effects%20(1998).pdf)

[A Beginner's Guide to DSP Programming.pdf](https://github.com/kxproject/kX-Audio-driver-Documentation/blob/master/A%20Beginner's%20Guide%20to%20DSP%20Programming.pdf)

![EMU10K1](https://upload.wikimedia.org/wikipedia/en/thumb/c/ca/EMU10K1-SEFbySpc.jpg/615px-EMU10K1-SEFbySpc.jpg)

2023 / klangraum
