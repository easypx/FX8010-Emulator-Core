// Copyright 2023 Klangraum

#ifndef FX8010_H
#define FX8010_H

#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <math.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <regex>
#include <map>
#include <array>

using namespace std;

// Unterdrücke Warnungen:
// 4715 - Nicht alle Codepfade geben einen Wert zurück.
// 4244 - A floating point type was converted to an integer type.
// 4305 - Value is converted to a smaller type in an initialization or as a constructor argument, resulting in a loss of information.
#pragma warning(disable : 4244 4305 4715)

// Nicht alle Defines werden genutzt
#define E 2.71828             // schön zu haben
#define PI 3.141592           // schön zu haben
#define SAMPLERATE 48000      // originale Samplerate des DSP
#define AUDIOBLOCKSIZE 128    // Nur zum Testen! Der Block-Loop wird vom VST-Plugin bereitgestellt.
#define DEBUG 0               // Synaxcheck(Verbose) & Errors, 0 oder 1 = mit/ohne Konsoleausgaben
#define PRINT_REGISTERS 0     // Zeige Registerwerte an. Dauert bei großer AUDIOBLOCKSIZE länger.
#define MAX_IDELAY_SIZE 4800  // 100ms max. Gesamtgroesse
#define MAX_XDELAY_SIZE 48000 // 1000ms max. Gesamtgroesse

namespace Klangraum
{

    class FX8010
    {
    public:
        FX8010(int numChannels);
        ~FX8010();

        // Method to initialize lookup tables and other initialization tasks
        void initialize();
        // Der eigentliche Prozess-Loop
        std::vector<float> process(const std::vector<float> &inputSamples);

        // Gibt Anzahl der ausgeführten Instructions zurück
        int getInstructionCounter();
        // Sourcecode laden
        bool loadFile(const string &path);
        struct MyError // Vorwärtsdeklaration notwendig!
        {
            std::string errorDescription = "";
            int errorRow = 1;
        };
        std::vector<FX8010::MyError> getErrorList();
        int setRegisterValue(const std::string &key, float value);
        float getRegisterValue(const std::string &key);
        vector<string> getControlRegisters();

    private:
        // Enum for FX8010 opcodes
        enum Opcode
        {
            MAC = 0,
            MACN,
            MACINT,
            MACINTW,
            ACC3,
            MACMV,
            MACW,  // only KX?
            MACWN, // only KX?
            SKIP,
            ANDXOR,
            TSTNEG,
            LIMIT,
            LIMITN,
            LOG,
            EXP,
            INTERP,
            END,
            IDELAY,
            XDELAY
        };

        // This Map holds Key/Value pairs to assign instructions(strings) to Opcode(enum/int)
        std::map<std::string, Opcode> opcodeMap = {
            {"mac", MAC},
            {"macn", MACN},
            {"macint", MACINT},
            {"macintw", MACINTW},
            {"acc3", ACC3},
            {"macmv", MACMV},
            {"macw", MACW},   // only KX?
            {"macwn", MACWN}, // only KX?
            {"skip", SKIP},
            {"andxor", ANDXOR},
            {"tstneg", TSTNEG},
            {"limit", LIMIT},
            {"limitn", LIMITN},
            {"log", LOG},
            {"exp", EXP},
            {"interp", INTERP},
            {"end", END},
            {"idelay", IDELAY},
            {"xdelay", XDELAY}};

        // Enum for FX8010 register types
        enum RegisterType
        {
            STATIC = 0,
            TEMP,
            CONTROL,
            INPUT,
            OUTPUT,
            CONST,
            ITRAMSIZE,
            XTRAMSIZE,
            READ,
            WRITE,
            AT,
            CCR,
            NOISE
        };

        // This Map holds Key/Value pairs to assign registertypes(strings) to RegisterType(enum/int)
        std::map<std::string, RegisterType> typeMap = {
            {"static", STATIC},
            {"temp", TEMP},
            {"control", CONTROL},
            {"input", INPUT},
            {"output", OUTPUT},
            {"const", CONST},
            {"itramsize", ITRAMSIZE},
            {"xtramsize", XTRAMSIZE},
            {"read", READ},
            {"write", WRITE},
            {"at", AT},
            {"ccr", CCR},
            {"noise", NOISE}};

        // FX8010 global storage
        double accumulator = 0; // 63 Bit, 4 Guard Bits, Long type?
        int instructionCounter = 0;
        int accumGuardBits = 0;     // ?
        float inputBuffer[2] = {0}; // TODO: Stereo I/O Buffers
        float outputBuffer[2] = {0};

        // GPR - General Purpose Register
        struct GPR
        {
            int registerType = 0;          // Typ des Registers (z.B. STATIC, TEMP, CONTROL, INPUT_, OUTPUT_)
            std::string registerName = ""; // Name des Registers
            float registerValue = 0;       // Wert des Registers
            int IOIndex = 0;               // 0 - Links, 1 - Rechts
        };

        // Vector, der die GPR enthaelt
        std::vector<GPR> registers;

        // Struct, die eine Instruktion repraesentiert
        struct Instruction
        {
            int opcode = 0;        // Opcode-Nummer
            int operand1 = 0;      // R (Index des GPR in Vektor "registers")
            int operand2 = 0;      // A (Index des GPR in Vektor "registers")
            int operand3 = 0;      // X (Index des GPR in Vektor "registers")
            int operand4 = 0;      // Y (Index des GPR in Vektor "registers")
            bool hasInput = false; // um nicht alle Instructions auf INPUT testen zu muessen
            // hasOutput nicht sinnvoll, Doppelcheck (Instruktion und R)
            bool hasOutput = false; // um nicht alle Instructions auf OUTPUT testen zu muessen
            bool hasNoise = false;
        };

        // Vector, der die Instruktionen enthaelt
        std::vector<Instruction> instructions;

        // Mehrere Lookup Tables in einem Vector, LOG, EXP jeweils mit 31 Exponenten
        vector<vector<double>> lookupTablesLog;
        vector<vector<double>> lookupTablesExp;

        // TRAM Engine
        //----------------------------------------------------------------

        // Angeforderte Delayline Groesse
        int iTRAMSize = 0;
        int xTRAMSize = 0;

        // Create a circular buffer for each delay line. You can use a std::vector to represent the buffer.
        // std::vector<float> smallDelayBuffer; // ATTENTION: Vektoren scheinen trotz reserve() und resize() Probleme mit zufaelligen Werten zu machen!
        // std::vector<float> largeDelayBuffer; //
        float smallDelayBuffer[MAX_IDELAY_SIZE];
        float largeDelayBuffer[MAX_XDELAY_SIZE];

        // Schreib-/Lesepointer
        int smallDelayWritePos = 0;
        int smallDelayReadPos = 0;

        int largeDelayWritePos = 0;
        int largeDelayReadPos = 0;

        // CCR Register
        inline void setCCR(const float result);
        inline int getCCR();

        // 32Bit Saturation
        inline float saturate(const float input, const float threshold);

        // Lineare Interpolation mit Lookup-Table
        inline double linearInterpolate(double x, const std::vector<double> &lookupTable, double x_min, double x_max);

        // MACINTW ...
        inline float wrapAround(const float a);

        // ANDXOR Instruction
        inline int logicOps(const GPR &A, const GPR &X_, const GPR &Y_);

        // TRAM Engine
        inline void setSmallDelayWritePos(int smallDelayWritePos); //?
        inline void setLargeDelayWritePos(int largeDelayWritePos); //?

        inline float readSmallDelay(int position);
        inline float readLargeDelay(int position);
        inline void writeSmallDelay(float sample, int position_);
        inline void writeLargeDelay(float sample, int position_);

        // Debug Registers
        void printRow(const int instruction, const float value1, const float value2, const float value3, const float value4, const double accumulator);

        // Syntax Check
        bool syntaxCheck(const std::string &input);

        // GPR-Index zurückgeben
        int findRegisterIndexByName(const std::vector<GPR> &registers, const std::string &name);

        // Map registernames from sourcecode instruction to register indexes
        int mapRegisterToIndex(const string &registerName);

        // Definiere die Fehlercodes
        enum ErrorCode
        {
            ERROR_NONE = 0,
            ERROR_INVALID_INPUT,
            ERROR_DIVISION_BY_ZERO,
            ERROR_MULTIPLE_VAR_DECLARE,
            ERROR_VAR_NOT_DECLARED,
            ERROR_INPUT_FOR_R_NOT_ALLOWED,
            ERROR_NO_END_FOUND,
            ERROR_IO_INDEX_OUT_OF_RANGE,
            ERROR_SYNTAX_NOT_VALID,
            ERROR_ITRAMSIZE_TO_LARGE,
            ERROR_XTRAMSIZE_TO_LARGE
            // Weitere Fehlercodes hier...
        };

        std::map<int, std::string> errorMap;
        MyError error;
        vector<MyError> errorList;
        int errorCounter = 1;

        std::vector<double> createLogLookupTable(double x_min, double x_max, int numEntries, int exponent);
        std::vector<double> createExpLookupTable(double x_min, double x_max, int numEntries, int exponent);
        std::vector<double> mirrorYVector(const std::vector<double> &inputVector);
        std::vector<double> concatenateVectors(const std::vector<double> &vector1, const std::vector<double> &vector2);
        std::vector<double> negateVector(const std::vector<double> &inputVector);

        int numChannels;

        vector<string> controlRegisters;

        float outputSamples[2] = {0};

        // Fast White Noise
        // Range (-1.0 ... 1.0)
        // https://www.musicdsp.org/en/latest/Synthesis/216-fast-whitenoise-generator.html
        float g_fScale = 2.0f / 0xffffffff;
        // Seeds
        int g_x1 = 0x70f4f854;
        int g_x2 = 0xe1e9f0a7;
        float FX8010::whitenoise();
    };

} // namespace Klangraum

#endif // FX8010_H