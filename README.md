# FX8010-Emulator-VST
Emulate an E-mu DSP (Soundblaster Audigy Cards)

The goal is to emulate a powerful interpreter for DSP assembler instructions based on the FX8010 architecture, which was found on Soundblaster Live!
and Audigy cards, as VST effect plugin. 

The core functionality is given, but partially unit tested. Syntax checker, register &
instructions mapper is done. Instructions, given in the testcode.da, 
should work. VST integration still needs to be tested. 

My testing has shown that the optimized release build actually can run 200 instructions / us (200 MIPS) on a single core. 
Interestingly, in this implementation state the hardware DSP has been surpassed 2X (100 MIPS) on a i5/3.2GHz.
However, if we factor in the multi-core processing, we're able to emulate much more DSP's.

A special shout out to ChatGPT which provides an excellent assistant and talks about every topic (CPU architecture, Compiler work, Memory organisation ...) in great detail! 😃

Usage:  
- For console debug output change DEBUG and PRINT_REGISTERS and other defines. (You will find them.)
- Not all instructions are 100 % tested, but should not crash. 
- Sourcecode syntax is same as DANE (KX-Project, 2. Link below) with a few exceptions, like INPUT/OUTPUT, delay address operation, variable declaration.
- Read-/Writeaddresses of delaylines can be modified simply by its indexes. (for now no 11 Bit shift/not testet) Hope it works!

```cpp
static a
itramsize 100
input in_l 0 ; NOTE: Hier gibt es einen Unterschied zu KX-DSP! 0 - Links, 1 - Rechts
control volume = 0.5
control filter_cutoff = 0.1
output out_l 0 
static rd
static wr

; LOG (Vacuum Tube Transferfunktion)
log a, in_l, 3, 0
macs out_l, 0, a, 1.0

; Delayline mit Feedback
; macs a, in_l, rd, 0.1
; idelay write, a, at, 0
; idelay read, rd, at, 17; max. read index (tramsize-1) !
; macs out_l, in_l, rd, 0.5

; 1-Pol Tiefpassfilter
; interp out_l, out_l, filter_cutoff, in_l

end
```

[3rd Party Docs/FX8010 - A DSP Chip Architecture for Audio Effects (1998).pdf](https://github.com/kxproject/kX-Audio-driver-Documentation/blob/master/3rd%20Party%20Docs/FX8010%20-%20A%20DSP%20Chip%20Architecture%20for%20Audio%20Effects%20(1998).pdf)

[A Beginner's Guide to DSP Programming.pdf](https://github.com/kxproject/kX-Audio-driver-Documentation/blob/master/A%20Beginner's%20Guide%20to%20DSP%20Programming.pdf)

![EMU10K1](https://upload.wikimedia.org/wikipedia/en/thumb/c/ca/EMU10K1-SEFbySpc.jpg/615px-EMU10K1-SEFbySpc.jpg)

https://sites.google.com/site/kxmodular

2023 / Klangraum
