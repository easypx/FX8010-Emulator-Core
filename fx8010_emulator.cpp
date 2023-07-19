// E-mu FX8010 Emulator (VST)
// 2023 / klangraum

// TODO: 
// - Syntaxcheck/Parser
// - keine doppelten Deklarationen!
// - Deklarationsformat checken
// - Instruktionsformat checken
// - Schlüsseworte identifizieren und entsprechend if/else if Verarbeitung, Mapping der Registerindizes auf die Instructiontemplates
// - (Hardware-Konstanten)
// - (Metadaten im Header)
// - VST-Integration, VST-Parameter ID hinzufügen bei CONTROL-Variable
// - (Parse Hex to Float), Wir rechnen mit float, wenn nötig Integer zu Float umwandeln
// - 11 Bit (2048) Shift der Leseadresse für Delays: Vom Code (mit Shift), wird einfach 11 Bit subtrahiert, um Vectorindex zu erhalten.
// - INTERP für lineare Interpolation beim Delay-Auslesen
// - Code aus File in String laden

#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <math.h>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;

#pragma warning(disable : 4244 4305)

#define E 2.71828
#define PI 3.141592
#define SAMPLERATE 48000
#define AUDIOBLOCKSIZE 32

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
	END,
	IDELAY,
	XDELAY
};

// Enum for FX8010 register types
enum RegisterType {
	STATIC,
	TEMP,
	CONTROL,
	INPUT,
	OUTPUT,
	CONST,
	ITRAMSIZE,
	XTRAUMSIZE,
	READ,
	WRITE,
	AT
};

vector<float> testSample;

// Enum for FX8010 global storage
struct DSP {
	double accumulator = 0; // 63 Bit, 4 Guard Bits, Long type?
	int accumGuardBits = 0x00000000; // ?
	// Delay lines here
	// CCR here
	int ccr = 0x00000000;
	// ...
};

DSP dsp;

// GPR Register
struct Register
{
	int registerType;		// Typ des Registers (z.B. STATIC, TEMP, CONTROL, INPUT_, OUTPUT_)
	std::string registerName;	// Name des Registers
	float registerValue;	// Wert des Registers 
	bool isSaturated = false;// für SKIP
};

// Struct, die eine Instruktion repraesentiert
struct Instruction
{
	int opcode;			// Opcode-Nummer
	int operand1;		// Erster Operand (Index des Registers im vector)
	int operand2;		// Zweiter Operand (Index des Registers im vector)
	int operand3;		// Dritter Operand (Index des Registers im vector)
	int operand4;		// Vierter Operand (Index des Registers im vector)
};

// Vector, der die Register enthaelt
std::vector < Register > registers =
{
  {STATIC, "a", 0x0}, //0
  {STATIC, "exponent", 0x7}, //1
  {INPUT, "in", 0x0}, //2
  {TEMP, "out", 0x0}, //3
  {CONST, "sign", 0x1} //4
};

// CHECKED
// Funktion zum Erstellen der Lookup-Tabelle für die n-te Wurzel
std::vector<double> createLogLookupTable(double x_min, double x_max, int numEntries, int exponent) {
	std::vector<double> lookupTable;
	double step = (x_max - x_min) / (numEntries - 1);
	//push 1. value=0 to avoid nan! only for log(x)! change i to 1!
	//lookupTable.push_back(0.0f);
	for (int i = 0; i < numEntries; ++i) {
		double x = x_min + (i * step);
		//natural log as seen in dane programming pdf, issues with nan!
		//double logValue = std::log(x) / exponent + 1; 
		//another good approximation! maybe this is what is really done in the dsp! 
		//they mention approximation of pow and sqrt.
		double logValue = pow(x, 1.0 / exponent);
		lookupTable.push_back(logValue);
	}
	return lookupTable;
}

// NOT CHECKED
// Funktion zum Erstellen der Lookup-Tabelle für eine Taylorreihe
double logApproximation(double x, int numEntries) {
	// Anzahl der Glieder in der Taylor-Reihe
	int numTerms = 5;

	// Koeffizienten der Taylor-Reihe für den natürlichen Logarithmus
	double coefficients[] = { 1.0, -0.5, 1.0 / 3.0, -0.25, 1.0 / 5.0 };

	// Ausgangswert der Approximation
	double result = 0.0;

	// Berechnung der Approximation
	for (int i = 0; i < numTerms; ++i) {
		result += coefficients[i] * pow(x, i + 1);
	}

	return result;
}

// CHECKED
// Funktion zur linearen Interpolation mit der Lookup-Tabelle (LOG)
double linearInterpolate(double x, const std::vector<double>& lookupTable, double x_min, double x_max) {
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

// NOT CHECKED
// Function to perform cubic interpolation using the lookup table
double cubicInterpolate(double x, const std::vector<double>& lookupTable) {
	int numEntries = lookupTable.size();
	double x_min = 0.0; // The minimum x value in the lookup table
	double x_max = numEntries - 1; // The maximum x value in the lookup table

	// Calculate the index of the lower bound value for cubic interpolation
	int lowerIndex = static_cast<int>(x);
	int upperIndex = lowerIndex + 1;

	// Perform cubic interpolation using the four nearest values in the lookup table
	double x0 = lowerIndex - 1;
	double x1 = lowerIndex;
	double x2 = upperIndex;
	double x3 = upperIndex + 1;

	double y0 = lookupTable[std::max(0, static_cast<int>(x0))];
	double y1 = lookupTable[static_cast<int>(x1)];
	double y2 = lookupTable[std::min(numEntries - 1, static_cast<int>(x2))];
	double y3 = lookupTable[std::min(numEntries - 1, static_cast<int>(x3))];

	double t = x - x1;
	double t2 = t * t;
	double t3 = t2 * t;

	double a0 = y3 - y2 - y0 + y1;
	double a1 = y0 - y1 - a0;
	double a2 = y2 - y0;
	double a3 = y1;

	double interpolatedValue = a0 * t3 + a1 * t2 + a2 * t + a3;

	return interpolatedValue;
}

//CHECKED
//Funktion zum Erstellen der Lookup-Tabelle für n-te Potenz (EXP)
std::vector<double> createExpLookupTable(double x_min, double x_max, int numEntries, int exponent) {
	std::vector<double> lookupTable;
	double step = (x_max - x_min) / (numEntries - 1);
	//lookupTable.push_back(0.0f); // see log for explanation
	for (int i = 0; i < numEntries; ++i) {
		double x = x_min + i * step;
		//double expValue = exp(x * numEntries - numEntries);
		double expValue = pow(x, exponent); // see log for explanation
		lookupTable.push_back(expValue);
	}
	return lookupTable;
}

// Number of steps in the lookup table
const int NumSteps = 100;

// Lookup table for tanh values
float tanhLookup[NumSteps + 1];

//NOT CHECKED
// Function to initialize the lookup table
void initializeTanhLookup() {
	for (int i = 0; i <= NumSteps; ++i) {
		float x = static_cast<float>(i) / NumSteps;
		tanhLookup[i] = std::tanh(x);
	}
}

//NOT CHECKED
// Linear interpolation function
float lerp(float a, float b, float t) {
	return a + t * (b - a);
}

//NOT CHECKED
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

//NOT CHECKED
float wrapAround(float a) {
	if (a >= 1.0f) {
		a -= 2.0f;
	}
	else if (a < -1.0f) {
		a += 2.0f;
	}
	return a;
}

//CHECKED
void showRegisters(vector<Register>& registers, vector<Instruction>& instructions, int instructionCounter) {

	// Zugriff auf die Operanden und Registerinformationen
	int opcode = instructions[instructionCounter].opcode; // read only
	int operand1Index = instructions[instructionCounter].operand1;
	int operand2Index = instructions[instructionCounter].operand2;
	int operand3Index = instructions[instructionCounter].operand3;
	int operand4Index = instructions[instructionCounter].operand4;

	// Zugriff auf die Register und deren Daten
	Register& operand1Register = registers[operand1Index]; // read/write
	Register& operand2Register = registers[operand2Index]; // read/write
	Register& operand3Register = registers[operand3Index]; // read/write
	Register& operand4Register = registers[operand4Index]; // read/write	

	// Beispiel: Ausgabe des Namens und Werts des ersten Registers
	std::cout << opcode << " | ";
	std::cout << operand1Register.registerName << " | " << operand1Register.registerValue << " | ";
	std::cout << operand2Register.registerName << " | " << operand2Register.registerValue << " | ";
	std::cout << operand3Register.registerName << " | " << operand3Register.registerValue << " | ";
	std::cout << operand4Register.registerName << " | " << operand4Register.registerValue << std::endl;
	std::cout << "Accumulator: " << dsp.accumulator << std::endl;
}

//NOT CHECKED
void setCCR(float result, bool isSaturated) {
	if (result == 0) dsp.ccr = 0x8;
	else if (result != 0) dsp.ccr = 0x100;
	else if (result < 0) dsp.ccr = 0x4;
	else if (result <= 0) dsp.ccr = 0x1008;
	else if (result > 0) dsp.ccr = 0x180;
	else if (result >= 0) dsp.ccr = 0x80;
	else if (isSaturated) dsp.ccr = 0x10;
}

//NOT CHECKED
int getCCR(DSP& dsp) {
	return dsp.ccr;
}

//NOT CHECKED
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
		return 0;
	}
	return 1;
};

//NOT CHECKED
float getRegisterValue(const std::string& key)
{
	for (const auto& element : registers)
	{
		if (element.registerName == key) {
			return element.registerValue;
		}
	}
	return 1; // Or any other default value you want to return if the element is not found
}

//Create a circular buffer for each delay line. You can use a std::vector to represent the buffer.

std::vector<float> smallDelayBuffer;
std::vector<float> largeDelayBuffer;

//Initialize the buffer sizes based on the desired delay lengths. Since you mentioned a small 
//delay of ~100ms and a large delay of ~10s for a 48kHz samplerate, you can calculate the buffer sizes as follows:

int smallDelaySize = static_cast<int>(0.1f * SAMPLERATE); // 100ms delay at 48kHz
int largeDelaySize = static_cast<int>(1.0f * SAMPLERATE); // 1s delay at 48kHz

//Keep track of the read and write positions for each delay line. You can use integer variables for this purpose.

int smallDelayReadPos = 0;
int smallDelayWritePos = 0;
int largeDelayReadPos = 0;
int largeDelayWritePos = 0;

//NOT CHECKED
//Implement a method to write a sample into each delay line. When writing a sample, 
//you need to update the write position and wrap it around if it exceeds the buffer size.
void writeSmallDelay(float sample) {
	smallDelayBuffer[smallDelayWritePos] = sample;
	smallDelayWritePos = (smallDelayWritePos + 1) % smallDelaySize;
}

void writeLargeDelay(float sample) {
	largeDelayBuffer[largeDelayWritePos] = sample;
	largeDelayWritePos = (largeDelayWritePos + 1) % largeDelaySize;
}

//NOT CHECKED
//Implement a method to read a sample from each delay line (with linear interpolation). 
//To do linear interpolation, you need to find the fractional part of the read position
//and interpolate between the two adjacent samples.

//linear interpolation: lerp(sample1, sample2, 0.5)

float readSmallDelay(int position) {
	int readPos = position % smallDelaySize;
	return smallDelayBuffer[readPos];
}

float readLargeDelay(int position) {
	int readPos = position % largeDelaySize;
	return largeDelayBuffer[readPos];
}

//Now, you have two delay lines with read and write methods (that provide linear interpolation) 
//between samples. You can integrate these methods into your FX8010 class and use them as needed. 
//Don't forget to update the read positions accordingly when reading from the delay lines.

//NOT CHECKED
// Um zu prüfen, ob ein Register mit registerName="a" bereits im Vector registers vorhanden ist
bool isRegisterNameInVector(const std::vector<Register>& registers, const std::string& name) {
	for (const auto& reg : registers) {
		if (reg.registerName == name) {
			return true; // Das Register mit dem angegebenen Namen wurde gefunden
		}
	}
	return false; // Das Register mit dem angegebenen Namen wurde nicht gefunden
}

//NOT CHECKED
// Um zu prüfen, ob ein Register mit registerName="a" bereits im Vector registers vorhanden ist
// Wenn ja gib -1 zurück, wenn nein gib den Index zurück.
int findRegisterIndexByName(const std::vector<Register>& registers, const std::string& name) {
	for (int i = 0; i < registers.size(); ++i) {
		if (registers[i].registerName == name) {
			return i; // Index des gefundenen Registers zurückgeben
		}
	}
	return -1; // -1 zurückgeben, wenn das Register nicht gefunden wurde
}

//CHECKED
// Funktion zum Schreiben von CSV-Daten in eine Datei
void writeCSVToFile(const std::string& filename, const std::vector<std::vector<string>>& data) {
	std::ofstream file(filename);
	if (!file) {
		std::cerr << "Fehler beim Öffnen der Datei: " << filename << std::endl;
		return;
	}

	for (const auto& row : data) {
		for (size_t i = 0; i < row.size(); ++i) {
			file << row[i];
			if (i < row.size() - 1) {
				file << ","; // Trennzeichen für CSV
			}
		}
		file << std::endl;
	}

	file.close();
}

//NOT CHECKED
// Funktion zum Umwandeln einer hexadezimalen Integer-Zahl in einen float
float hexToFloat(const std::string& hexString) {
	union {
		float floatValue;
		unsigned int intValue;
	} converter;

	std::stringstream ss;
	ss << std::hex << hexString;
	ss >> converter.intValue;

	return converter.floatValue;
}

//NOT CHECKED
// Funktion zum Umwandeln eines float-Werts in eine hexadezimale Integer-Zahl
std::string floatToHex(float floatValue) {
	union {
		float floatValue;
		unsigned int intValue;
	} converter;

	converter.floatValue = floatValue;
	std::stringstream ss;
	ss << std::hex << std::setfill('0') << std::setw(8) << converter.intValue;

	return ss.str();
}

//NOT CHECKED
//Funktion zum Spiegeln des Vectors
std::vector<double> mirrorVector(const std::vector<double>& inputVector) {
	std::vector<double> mirroredVector;
	for (int i = inputVector.size() - 1; i >= 0; --i) {
		mirroredVector.push_back(1.0f - inputVector[i]);
	}
	return mirroredVector;
}

//NOT CHECKED
//Funktion zum Verknüpfen der Vectoren
std::vector<double> concatenateVectors(const std::vector<double>& vector1, const std::vector<double>& vector2) {
	std::vector<double> concatenatedVector;
	concatenatedVector.reserve(vector1.size() + vector2.size());
	concatenatedVector.insert(concatenatedVector.end(), vector2.begin(), vector2.end());
	concatenatedVector.insert(concatenatedVector.end(), vector1.begin(), vector1.end());
	return concatenatedVector;
}

//NOT CHECKED
//Hier ist eine Funktion zum Negieren eines Vektors
std::vector<double> negateVector(const std::vector<double>& inputVector) {
	std::vector<double> negatedVector;
	negatedVector.reserve(inputVector.size());
	for (float value : inputVector) {
		negatedVector.push_back(-value);
	}
	return negatedVector;
}

//NOT CHECKED
//Um den Inhalt einer Textdatei in einen String zu laden
std::string loadFileToString(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << filePath << std::endl;
		return "";
	}

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	return content;
}

int main()
{
	std::string filePath = "example.txt";
	std::string fileContent = loadFileToString(filePath);

	if (!fileContent.empty()) {
		std::cout << "File content:" << std::endl;
		std::cout << fileContent << std::endl;
	}

	// Beispiel-Daten für CSV
	std::vector<std::vector<string>> data;
	data.push_back({ "X", "Y" });

	// Dateiname für die CSV-Datei
	std::string filename = "c:\\VST_SDK\\vst3sdk\\build3\\Project1\\temp\\output.csv";

	// Generate test sample 0...1.0
	vector<float>testSample;
	int numSamples = 32;
	for (int i = 0; i < numSamples; i++) {
		float value = static_cast<float>(i) / (numSamples - 1); // Wertebereich von 0 bis 1.0		
		testSample.push_back(value);
	}

	// Mehrere Lookup Tables in einem Vector
	vector<vector<double>> lookupTablesLog;
	vector<vector<double>> lookupTablesExp;
	int numTables = 32;
	int numEntries = 32;
	int x_min = 0; // < 0 gives nan! 
	int x_max = 1.0;
	// Populate lookupTablesLog with lookup tables for different i values
	// Tabellen mit 4 Vorzeichen und 32 Exponenten je 32 Werte = 4096 Float Werte (16384 Bytes) (2x für LOG und EXP)
	for (int i = 0; i < numTables; i++) {
		std::vector<double> lookupTableLog = createLogLookupTable(0, 1.0, numEntries, i);
		// an Y-Achse spiegeln, dann Negieren
		std::vector<double> mirroredVector = negateVector(mirrorVector(lookupTableLog));
		// Verknüpfen Sie die beiden Vektoren
		std::vector<double> combinedVector = concatenateVectors(mirroredVector, lookupTableLog);
		lookupTablesLog.push_back(combinedVector);
		std::vector<double> lookupTableExp = createExpLookupTable(0, 1.0, numEntries, i);
		lookupTablesExp.push_back(lookupTableExp);
	}

	// Initialize the lookup table
	initializeTanhLookup();

	//https://github.com/kxproject/kX-Audio-driver-Documentation/blob/master/A%20Beginner's%20Guide%20to%20DSP%20Programming.pdf

	// TODO: lese symbol aus quellcodezeile, wenn symbol in registers existiert, gib getindex() zurück, map auf instruction
	// wenn nicht existiert, pushe gpr in registers

	registers.push_back({ STATIC, "9", 5 });//5
	registers.push_back({ INPUT, "in", 0 });//6
	registers.push_back({ STATIC, "1000", 1000 });//7
	registers.push_back({ CONST, "0", 0 });//8 alle CONST sind vorgeladen/muss nicht zwingend implementiert werden
	registers.push_back({ CONST, "write", WRITE });//9
	registers.push_back({ CONST, "read", READ });//10
	registers.push_back({ CONST, "at", AT });//11

	// Vector, der die Instruktionen enthC$lt
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
		{LOG, 0, 2, 1, 4},
		{EXP, 0, 2, 1, 4},
		{MACINTW, 0,0,0,0},
		{MACMV, 0,1,4,3},
		{ANDXOR, 0, 0, 0, 0},
		{IDELAY, 9, 0, 11, 0},
		{IDELAY, 10, 0, 11, 7},
	};

	//instructions.push_back({ INTERP, 1, 0, 6, 4 });
	//instructions.push_back({ END, 0, 0, 0, 0 });

	setRegisterValue("b", 10.0); // Test the functions
	//std::cout << "b: " << getRegisterValue("b") << endl;

	// Fill buffer with 0 is important to avoid clicks at the bedinning
	smallDelayBuffer.resize(smallDelaySize, 0.0f);
	largeDelayBuffer.resize(largeDelaySize, 0.0f);

	// Startzeitpunkt speichern
	auto startTime = std::chrono::high_resolution_clock::now();

	bool doNotLoop = false;
	int instructionCounter = 0;
	// Durchlaufen der Instruktionen und AusfC<hren des Emulators
	//std::cout << "x,y" << std::endl; // CVS Achsenbezeichungen
	float R = 0, A = 0, X = 0, Y = 0;// TODO: nach DSP 
	for (int i = 0; i < 32; i++) {
		for (const auto& instruction : instructions)
		{
			// Zugriff auf die Operanden und Registerinformationen
			int opcode = instruction.opcode; // read only
			int operand1Index = instruction.operand1;
			int operand2Index = instruction.operand2;
			int operand3Index = instruction.operand3;
			int operand4Index = instruction.operand4;

			// Zugriff auf die Register und deren Daten
			Register& operand1Register = registers[operand1Index]; // read/write
			Register& operand2Register = registers[operand2Index]; // read/write
			Register& operand3Register = registers[operand3Index]; // read/write
			Register& operand4Register = registers[operand4Index]; // read/write

			// Hier koennen Sie die Instruktionen ausfuehren, basierend auf den Registern und Opcodes
			// ...
			// Lade Registerwerte 
			// TODO: Stereo Inputs?
			R = operand1Register.registerValue;
			if (operand2Register.registerType == INPUT) A = testSample[i]; // Sample Input
			else A = operand2Register.registerValue;
			if (operand3Register.registerType == INPUT) X = testSample[i]; // Sample Input
			else X = operand3Register.registerValue;
			if (operand4Register.registerType == INPUT) Y = testSample[i]; // Sample Input
			else Y = operand4Register.registerValue; // 10???
			//cout << R << "|" << A << "|" << X << "|" << Y << endl; // Registers before processing
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
				// Set ccr register based on R
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
				//R = (log(A)/X)+1; // Math.h Implementierung, 4 Mikrosekunden
				R = linearInterpolate(A, lookupTablesLog[X], 0, 1.0); // unter 1 Mikrosekunden
				dsp.accumulator = R;
				operand1Register.registerValue = R;
				break;
			case EXP:
				//R = exp(A*X-X); // Math.h Implementierung, 4 Mikrosekunden
				//R = pow(E,A*X-X); // 4 x schneller als exp(x)
				R = linearInterpolate(A, lookupTablesExp[X], 0, 1);
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
				// Aus R = A & X ^ Y synthetisierte bitwise logische Operationen (Dank ChatGPT)
				// 
				// AND	
				//		A = 0x00000000   
				//		X = 0x10101010  
				//		Y = 0x10000000
				// OR   
				//		A = 0x00000000
				//		X = 0x10101010
				//		Y = 0x01010101
				// XOR
				//		A = 0x10101010
				//		X = 0x01010101
				//		Y = 0x00000000
				// NOT
				//		A = 0x10101010
				//		X = 0x00000000
				//		Y = 0x11111111
				// 
				// Schritt 1: A & X
				// A& X ergibt eine bitweise Und - Verknüpfung zwischen A und X.Hierbei werden jeweils die 
				// entsprechenden Bits von A und X verglichen, und das Ergebnis hat an jeder Position 
				// eine 1, wenn beide Bits 1 sind, und eine 0, wenn eines der Bits oder beide Bits 0 sind.

				// A & X = 0x00000000 & 0x10101010 = 0x00000000

				// Schritt 2: A & X ^ Y
				// Das Ergebnis der vorherigen Operation(A & X) wird mit Y bitweise exklusiv - verodert(XOR).
				// Dies bedeutet, dass an jeder Position das Ergebnis eine 1 ist, wenn die Bits in A & X 
				// und Y unterschiedlich sind, und eine 0, wenn die Bits gleich sind.

				// (A & X) ^ Y = 0x00000000 ^ 0x01010101 = 0x01010101
				// 
				// Typcast zu float->int wird wahrscheinlich schlecht funktionieren als Input.
				// FX8010 kennt auch int als Zahlenbasis. Man müsste wohl R, A, X, Y vorher auf int-Typen umleiten/checken.
				R = (int)A & (int)X ^ (int)Y;
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
			case IDELAY:
				if (R == READ) {
					operand2Register.registerValue = readSmallDelay(Y); // iDelayRead(Y); // Y = Adresse
				}
				else if (R == WRITE) {
					writeSmallDelay(A); // iDelayWrite(A); // A = value
				}
				break;
			case XDELAY:
				if (R == READ) {
					operand2Register.registerValue = readLargeDelay(Y); // iDelayRead(Y); // Y = Adresse
				}
				else if (R == WRITE) {
					writeLargeDelay(A); // iDelayWrite(A); // A = value
				}
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

			instructionCounter++;
			//std::cout << testSample[i] << " , " << R << std::endl; // CVS Daten in Console (als tabelle für https://www.desmos.com/)
			//std::cout << "(" << testSample[i] << " , " << R << ")" << std::endl; // CVS Daten in Console (als punktfolge für https://www.desmos.com/)
			//data.push_back({ std::to_string(testSample[i]) , std::to_string(R) }); // CVS Daten in Vector zum speichern
			//cout << R << "|" << A << "|" << X << "|" << Y << endl; // Registers after processing
		}

		/*if (AUDIOBLOCKSIZE <= 1) {
			showRegisters(registers, instructions, instructionCounter);
		}
		else if (!doNotLoop) {
			std::cout << "Blocksize > 1: Wirklich alle Schleifendurchlaeufe anzeigen?" << std::endl;
			doNotLoop = true;
		}*/
		//std::cout << "Testsample: " << testSample[i] << std::endl;
	}
	// Endzeitpunkt speichern
	auto endTime = std::chrono::high_resolution_clock::now();

	// Berechnen der Differenz zwischen Start- und Endzeitpunkt
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

	// Ausgabe der gemessenen Zeit in Mikrosekunden
	std::cout << "Ausfuehrungszeit: " << duration << " Mikrosekunden" << " fuer " << instructions.size() * AUDIOBLOCKSIZE << " Instructions pro Audioblock." << std::endl;

	// CSV-Daten in Datei schreiben
	writeCSVToFile(filename, data);

	return 0;
}
