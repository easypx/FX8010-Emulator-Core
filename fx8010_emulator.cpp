// E-mu FX8010 Emulator (VST)
// 2023 / klangraum
// TODO: Syntaxchecker / Mapper
// - keine doppelten Deklarationen!
// - Deklarationsformat checken
// - Instruktionsformat checken
// - Schlüsseworte identifizieren und entsprechend if/else if Verarbeitung
// - GPR Counter, wenn registers.push_back() (Variablen) und instructions.push_back() (Zahlenwerte)
// - 2D - unordered map (zeilenweise) anlegen, um Variablen und Konstranten einem GPR- Index zuzuweisen, Dopplungen checken?
// - Hardware-Konstanten brauchen wir erstmal nicht
// - Metadaten im Header (zweitrangig)
// - VST-Parameter ID hinzufügen bei control-Variable
// https://github.com/kxproject/kX-Audio-driver-Documentation/blob/master/A%20Beginner's%20Guide%20to%20DSP%20Programming.pdf

#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <math.h>
using namespace std;

#pragma warning(disable : 4244 4305)

#define E 2.71828
#define PI 3.141592

// Enum for FX8010 opcodes
enum Opcode {
	MAC,
	MACINT,
	MACINTW,
	ACC3,
	MACMV,
	MACW, // only KX?
	MACWN, // only KX?
	SKIP,
	ANDXOR,
	TSTNEG,
	LIMIT,
	LIMITN,
	LOG,
	EXP,
	INTERP,
	END
};

// Enum for FX8010 register types
enum RegisterType {
	STATIC,
	TEMP,
	CONTROL,
	INPUT_,
	OUTPUT_,
	CONST
};

struct DSP {
	double accumulator; // 63 Bit, 4 Guard Bits, Long type?
	int accunGuardBits = 0b0000; // 
	// Delay lines here
	// CCR here
	int ccr = 0x00000000;
	// ...
}dsp;

struct Register
{
	int registerType;		// Typ des Registers (z.B. STATIC, TEMP, CONTROL, INPUT_, OUTPUT_)
	std::string registerName;	// Name des Registers
	float registerValue;	// Wert des Registers 
	bool isSaturated = false;
};

// Vector, der die Register enthaelt
std::vector < Register > registers =
{
  {STATIC, "a", 0}, //0
  {TEMP, "volume", 2}, //1
  {TEMP, "in", 0.1}, //2
  {TEMP, "out", 1}, //3
  {STATIC, "b", 3} //4
};

// Funktion zum Erstellen der Lookup-Tabelle für den natürlichen Logarithmus
std::vector<double> createLogLookupTable(double x_min, double x_max, int numEntries) {
	std::vector<double> lookupTable;
	double step = (x_max - x_min) / (numEntries - 1);

	for (int i = 0; i < numEntries; ++i) {
		double x = x_min + i * step;
		lookupTable.push_back(std::log(x));
	}

	return lookupTable;
}

// Funktion zur linearen Interpolation des natürlichen Logarithmus mit der Lookup-Tabelle
double linearInterpolateLog(double x, const std::vector<double>& lookupTable, double x_min, double x_max) {
	double step = (x_max - x_min) / (lookupTable.size() - 1);
	int index = static_cast<int>((x - x_min) / step);
	double x1 = x_min + index * step;
	double x2 = x_min + (index + 1) * step;
	double y1 = lookupTable[index];
	double y2 = lookupTable[index + 1];

	// Lineare Interpolation
	double y = (y2 - y1) / (x2 - x1) * (x - x1) + y1;

	return y;
}

// Funktion zum Erstellen der Lookup-Tabelle für den natürlichen Logarithmus
std::vector<double> createExpLookupTable(double x_min, double x_max, int numEntries) {
	std::vector<double> lookupTable;
	double step = (x_max - x_min) / (numEntries - 1);

	for (int i = 0; i < numEntries; ++i) {
		double x = x_min + i * step;
		lookupTable.push_back(std::exp(x * 7 - 7));
	}

	return lookupTable;
}

// Funktion zur linearen Interpolation des natürlichen Logarithmus mit der Lookup-Tabelle
double linearInterpolateExp(double x, const std::vector<double>& lookupTable, double x_min, double x_max) {
	double step = (x_max - x_min) / (lookupTable.size() - 1);
	int index = static_cast<int>((x - x_min) / step);
	double x1 = x_min + index * step;
	double x2 = x_min + (index + 1) * step;
	double y1 = lookupTable[index];
	double y2 = lookupTable[index + 1];

	// Lineare Interpolation
	double y = (y2 - y1) / (x2 - x1) * (x - x1) + y1;

	return y;
}

// Number of steps in the lookup table
const int NumSteps = 100;

// Lookup table for tanh values
float tanhLookup[NumSteps + 1];

// Function to initialize the lookup table
void initializeTanhLookup() {
	for (int i = 0; i <= NumSteps; ++i) {
		float x = static_cast<float>(i) / NumSteps;
		tanhLookup[i] = std::tanh(x);
	}
}

// Linear interpolation function
float lerp(float a, float b, float t) {
	return a + t * (b - a);
}

// Linearly interpolate the tanh(x) function using the lookup table
float lerpTanh(float x) {
	if (x <= 0.0f) {
		return tanhLookup[0];
	}
	else if (x >= 1.0f) {
		return tanhLookup[NumSteps];
	}

	float scaledX = x * NumSteps;
	int lowerIndex = static_cast<int>(scaledX);
	int upperIndex = lowerIndex + 1;

	float t = scaledX - lowerIndex;
	return lerp(tanhLookup[lowerIndex], tanhLookup[upperIndex], t);
}

float wrapAround(float a) {
	if (a >= 1.0f) {
		a -= 2.0f;
	}
	else if (a < -1.0f) {
		a += 2.0f;
	}
	return a;
}

void showRegisters() {
	// Beispiel: Ausgabe des Namens und Werts des ersten Registers
	//std::cout << operand1Register.registerName << " | " << operand1Register.registerValue << " | ";
	//std::cout << operand2Register.registerName << " | " << operand2Register.registerValue << " | ";
	//std::cout << operand3Register.registerName << " | " << operand3Register.registerValue << " | ";
	//std::cout << operand4Register.registerName << " | " << operand4Register.registerValue << std::endl;
	//std::cout << "Accumulator: " << dsp.accumulator << std::endl;
	//std::cout << "R: " << R << std::endl;
}

void setCCR(float result, bool isSaturated) {
	if (result == 0) dsp.ccr = 0x8;
	else if (result != 0) dsp.ccr = 0x100;
	else if (result < 0) dsp.ccr = 0x4;
	else if (result <= 0) dsp.ccr = 0x1008;
	else if (result > 0) dsp.ccr = 0x180;
	else if (result >= 0) dsp.ccr = 0x80;
	else if (isSaturated) dsp.ccr = 0x10;
}

int getCCR(DSP& dsp) {
	return dsp.ccr;
}

// TODO: setRegisterValue(string key, float value), getRegisterValue(string key)
int setRegisterValue(const std::string& key, float value)
{
	bool found = false;
	float newValue = 0;
	for (auto& element : registers)
	{
		if (element.registerName == key) {
			element.registerValue = value;
			newValue = element.registerValue;
			found = true;
			break;
		}
	}
	if (found) {
		//std::cout << "Value von " << key << " ist jetzt: " << newValue << std::endl;
		return 0;
	}
	else {
		//std::cout << "Element nicht gefunden." << std::endl;
		return 1;
	}
};

float getRegisterValue(const std::string& key)
{
	for (const auto& element : registers)
	{
		if (element.registerName == key) {
			return element.registerValue;
		}
	}
	// std::cout << "Element nicht gefunden." << std::endl;
	return 1; // Or any other default value you want to return if the element is not found
}

int main()
{
	double x_min = 0;  // Startwert für x
	double x_max = 1; // Endwert für x
	int numEntries = 31; // Anzahl der Einträge in der Lookup-Tabelle

	// Es werden wahrscheinlich 31 verschiedene Wertetabellen berechnet jeweils für log und exp
	std::vector<double> lookupTableLog = createLogLookupTable(x_min, x_max, numEntries);
	std::vector<double> lookupTableExp = createExpLookupTable(x_min, x_max, numEntries);

	// Initialize the lookup table
	initializeTanhLookup();

	registers.push_back({ STATIC, "0.8", 0.8 });//5
	registers.push_back({ STATIC, "d", 0.5 });//6
	registers.push_back({ STATIC, "0.1", 0.1 });//7
	registers.push_back({ CONST, "0", 0 });//8 alle CONST sind vorgeladen/muss nicht zwingend implementiert werden
	//registers[4].registerValue = 3.3;

	// Struct, die eine Instruktion repraesentiert
	struct Instruction
	{
		int opcode;		// Opcode-Nummer
		int operand1;		// Erster Operand (Index des Registers im vector)
		int operand2;		// Zweiter Operand (Index des Registers im vector)
		int operand3;		// Dritter Operand (Index des Registers im vector)
		int operand4;		// Vierter Operand (Index des Registers im vector)
	};

	// Vector, der die Instruktionen enthaelt
	std::vector < Instruction > instructions =
	{
		{MAC, 0, 0, 2, 3},
		{MACINT, 3, 0, 6, 1},
		{MAC, 3, 4, 6, 7},
		{ACC3, 3, 0, 2, 4},
		{ACC3, 3, 0, 2, 4},
		{ACC3, 3, 0, 2, 4},
		{ACC3, 3, 0, 2, 4},
		{MACINT, 3, 0, 2, 1},
		{MAC, 3, 0, 6, 7},
		{LOG, 3, 5, 4, 8},
		{EXP, 3, 5, 4, 8},
		{MACINTW, 0, 0, 0, 0},
		{MACMV, 0, 1, 4, 3}
	};

	// Push some instructions into the code memory on the fly
	instructions.push_back({ INTERP, 1, 0, 6, 4 });
	instructions.push_back({ END, 0, 0, 0, 0 });

	setRegisterValue("b", 10.0); // Test the functions
	//std::cout << "b: " << getRegisterValue("b") << endl;

	// Startzeitpunkt speichern
	auto startTime = std::chrono::high_resolution_clock::now();

	// Durchlaufen der Instruktionen und Ausfuehren des Emulators
	for (const auto& instruction : instructions)
	{
		// Zugriff auf die Operanden und Registerinformationen
		int opcode = instruction.opcode; // nur lesbar
		int operand1Index = instruction.operand1;
		int operand2Index = instruction.operand2;
		int operand3Index = instruction.operand3;
		int operand4Index = instruction.operand4;

		// Zugriff auf die Register und deren Daten
		Register& operand1Register = registers[operand1Index]; // wiederbeschreibbar
		Register& operand2Register = registers[operand2Index]; // wiederbeschreibbar
		Register& operand3Register = registers[operand3Index]; // wiederbeschreibbar
		Register& operand4Register = registers[operand4Index]; // wiederbeschreibbar

		// Hier können Sie die Instruktionen ausfC<hren, basierend auf den Registern und Opcodes
		// ...
		float R = operand1Register.registerValue;
		float A = operand2Register.registerValue;
		float X = operand3Register.registerValue;
		float Y = operand4Register.registerValue;
		switch (opcode)
		{
		case MAC:
			R = A + X * Y;
			dsp.accumulator = R; // Copy unsaturated value into accumulator
			// Calculate the linearly interpolated tanh(x) value, (DIN 4095) saturation, TODO: Check, Trace 
			R = lerpTanh(R);
			operand1Register.registerValue = R; // Update current result register
			// Set saturation flag
			operand1Register.isSaturated = true;
			// Set CCR register based on R, TODO: ...
			setCCR(R, operand1Register.isSaturated);
			break;
		case MACINT:
			R = A + X * Y;
			dsp.accumulator = R;
			R = lerpTanh(R);
			operand1Register.registerValue = R;
			break;
		case ACC3:
			R = A + X + Y;
			dsp.accumulator = R;
			// TODO: Saturation?
			// ...
			operand1Register.registerValue = R;
			break;
		case LOG:
			//R = log(A)/X+1; // Math.h Implementierung, 4 Mikrosekunden
			R = linearInterpolateLog(A, lookupTableLog, 0, 1) / X + 1; // unter 1 Mikrosekunden
			dsp.accumulator = R;
			operand1Register.registerValue = R;
			break;
		case EXP:
			//R = exp(A*X-X); // <cmath> Implementierung, 4 Mikrosekunden
			//R = pow(E,A*X-X); // 4 x schneller als exp(x)
			// unter 1 Mikrosekunden TODO: Verarbeitung der Exponenten
			// lookupTableExp ist auf Exponent 7 voreingestellt, siehe Berechnung
			R = linearInterpolateExp(A, lookupTableExp, 0, 1);
			dsp.accumulator = R;
			operand1Register.registerValue = R;
			break;
		case MACW:
			R = wrapAround(A + X * Y); // TODO: Check
			dsp.accumulator = R;
			operand1Register.registerValue = R;
			break;
		case MACMV:
			dsp.accumulator = dsp.accumulator + X * Y;
			R = A;
			operand1Register.registerValue = R;
			break;
		case ANDXOR:
			// TODO: ...
			break;
		case TSTNEG:
			R = A >= Y ? X : -X; // TODO: Check
			dsp.accumulator = R;
			operand1Register.registerValue = R;
			break;
		case LIMIT:
			R = A >= Y ? X : Y; // TODO: Check
			dsp.accumulator = R;
			operand1Register.registerValue = R;
			break;
		case LIMITN:
			R = A < Y ? X : Y; // TODO: Check
			dsp.accumulator = R;
			operand1Register.registerValue = R;
			break;
		case SKIP:
			// TODO: ...
			// Need indexed for-loop for jumps?
			// Oder Zähler setzen und runterzählen, am Loop-Anfang prüfen, ob Sprung notwendig.
			break;
		case INTERP:
			R = (1.0 - X) * A + (X * Y);
			dsp.accumulator = R;
			operand1Register.registerValue = R;
			//std::cout << "here" << std::endl;
			break;
		case END:
			// End of sample cycle
			// TODO: Breakout from loop?
			break;

		default:
			//std::cout << "Opcode: " << opcode << " ist nicht implementiert." << std::endl;
			break;
		}
		// Reset temp registers, TODO: Need this?
		if (operand1Register.registerType == TEMP) operand1Register.registerValue = 0;
		if (operand2Register.registerType == TEMP) operand2Register.registerValue = 0;
		if (operand3Register.registerType == TEMP) operand3Register.registerValue = 0;
		if (operand4Register.registerType == TEMP) operand4Register.registerValue = 0;

		//showRegisters();
		// Beispiel: Ausgabe des Namens und Werts des ersten Registers
		/*std::cout << operand1Register.registerName << " | " << operand1Register.registerValue << " | ";
		std::cout << operand2Register.registerName << " | " << operand2Register.registerValue << " | ";
		std::cout << operand3Register.registerName << " | " << operand3Register.registerValue << " | ";
		std::cout << operand4Register.registerName << " | " << operand4Register.registerValue << std::endl;
		std::cout << "Accumulator: " << dsp.accumulator << std::endl;
		std::cout << "R: " << R << std::endl;*/
	}
	// Endzeitpunkt speichern
	auto endTime = std::chrono::high_resolution_clock::now();

	// Berechnen der Differenz zwischen Start- und Endzeitpunkt
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

	// Ausgabe der gemessenen Zeit in Mikrosekunden
	std::cout << "Ausfuehrungszeit: " << duration << " Mikrosekunden" << std::endl;

	return 0;
}
