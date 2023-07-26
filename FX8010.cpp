// IDEA: Map mit Registern statt Vector, Zugriff ueber RegisterName
// IDEA: Sourcecode mit Stringstream direkt auswerten (ChatGPT Source in /temp)

#include "../include/FX8010.h"
#include "../include/helpers.h"

FX8010::FX8010()
{
	initialize();
};

FX8010::~FX8010(){};

void FX8010::initialize()
{
	// LOG, EXP Tables anlegen (Wertebereich: -1.0 bis 1.0)
}

// NOT CHECKED
inline void FX8010::setCCR(float result, bool isSaturated)
{
	if (result == 0)
		dsp.ccr = 0x8;
	else if (result != 0)
		dsp.ccr = 0x100;
	else if (result < 0)
		dsp.ccr = 0x4;
	else if (result <= 0)
		dsp.ccr = 0x1008;
	else if (result > 0)
		dsp.ccr = 0x180;
	else if (result >= 0)
		dsp.ccr = 0x80;
	else if (isSaturated)
		dsp.ccr = 0x10;
}

// NOT CHECKED
inline int FX8010::getCCR()
{
	return dsp.ccr;
}

// CHECKED
inline float FX8010::hardClip(float input, float threshold)
{
	if (input > threshold)
	{
		return threshold;
	}
	else if (input < -threshold)
	{
		return -threshold;
	}
	else
	{
		return input;
	}
}

// CHECKED
// Funktion zur linearen Interpolation mit der Lookup-Tabelle (LOG)
inline double FX8010::linearInterpolate(double x, const std::vector<double> &lookupTable, double x_min, double x_max)
{
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
inline float FX8010::wrapAround(float a)
{
	if (a >= 1.0f)
	{
		a -= 2.0f;
	}
	else if (a < -1.0f)
	{
		a += 2.0f;
	}
	return a;
}

inline int FX8010::logicOps(int A, int X, int Y)
{
	// siehe "Processor with Instruction Set for Audio Effects (US930158, 1997).pdf"
	// A	      X	         Y	        R
	// -------------------------------------------
	// A          X          Y    (A and X) xor Y
	// A          X          0       A and X
	// A      0xFFFFFF       Y       A xor Y
	// A      0xFFFFFFF  0xFFFFFF     not A
	// A          X         ~X       A or Y
	// A          X      0xFFFFFF    A nand X
	int R = 0;
	if (Y == 0)
		R = A & X; // Bitweise AND-Verknüpfung von A und X
	else if (X == 0xFFFFFF)
		R = A ^ Y; // Bitweise XOR-Verknüpfung von A und Y
	else if (X == 0xFFFFFFF && Y == 0xFFFFFF)
		R = ~A; // Bitweise NOT-Verknüpfung von A
	else if (Y == ~X)
		R = A | Y; // Bitweise OR-Verknüpfung von A und Y
	else if (Y == 0xFFFFFF)
		R = ~(A & X); // Bitweise NAND-Verknüpfung von A und X
	else
		R = (A & X) ^ Y; // (A AND X) XOR Y
	return R;
}

// Setzen der Schreibposition in der Microcode Deklaration
inline void FX8010::setSmallDelayWritePos(int smallDelayWritePos)
{
	smallDelayWritePos = smallDelayWritePos;
}

inline void FX8010::setLargeDelayWritePos(int largeDelayWritePos)
{
	largeDelayWritePos = largeDelayWritePos;
}

bool FX8010::syntaxCheck(const std::string &input)
{
	// verschiedene Kombinationen im Deklarationsteil, auch mehrfache Vorkommen
	// std::regex pattern1(R"(^\s*(static|temp)\s+((?:\w+\s*(?:=\s*\d+(?:\.\d*)?)?\s*,?\s*)+)\s*$)");

	// Deklarationen: static a | static b = 1.0
	std::regex pattern1(R"(^\s*(static|temp|input|output|control)\s+(\w+)(?:\s*=\s*(\d+(?:\.\d+)?))?\s*$)");

	// leere Zeile
	std::regex pattern2(R"(^\s*$)");

	// tramsize im Deklarationsteil
	std::regex pattern3(R"(^(itramsize|xtramsize)\s+(\d+)$)");

	// check instructions TODO: all instructions
	std::regex pattern4(R"(^\s*(mac|macint|acc3|tstneg|interp)\s+([a-zA-Z0-9_]+)\s*,\s*([a-zA-Z0-9_.-]+)\s*,\s*([a-zA-Z0-9_.-]+)\s*,\s*([a-zA-Z0-9_.-]+)\s*$)");

	// check metadata TODO: ...
	std::regex pattern5(R"(^\s*(comment|name|guid)\s+([a-zA-Z0-9_]+)\s*,\s*([a-zA-Z0-9_.]+)\s*$)");

	// Check "END"
	std::regex pattern6(R"(^\s*(end)\s*$)");

	// Check Kommentar ";"
	std::regex pattern7(R"(^\s*;+\s*$)");

	std::smatch match;

	// CHECKED
	// Teste auf Deklarationen: static a | static b = 1.0 (vorerst keine Mehrfachdeklarationen!)
	if (std::regex_match(input, match, pattern1))
	{
		if (DEBUG)
			cout << "Deklaration Variable gefunden" << endl;
		std::string registerTyp = match[1];
		std::string registerName = match[2];
		std::string registerValue = "";
		if (match[3].matched)
		{
			registerValue = match[3];
		}
		// wenn Registername nicht existiert (-1)
		if (findRegisterIndexByName(registers, registerName) == -1)
		{
			// Lege neues Temp-Register an
			Register reg;

			// Überprüfen, ob der String in der Map existiert
			if (typeMap.find(registerTyp) != typeMap.end())
			{
				reg.registerType = typeMap[registerTyp];
			}
			else
			{
				// Behandlung für den Fall, dass der Typ nicht gefunden wurde.
				// NOTE: Kann eigentlich nicht passieren, weil die Regex auf Types prueft!
				if (DEBUG)
					std::cout << "Ungueltiger Typ: " << registerTyp << std::endl;
			}
			reg.registerName = registerName;
			if (!registerValue.empty())
			{
				reg.registerValue = stof(registerValue);
			}
			else
			{
				reg.registerValue = 0;
			}
			// Schiebe befülltes Register nach Registers
			registers.push_back(reg);
			if (DEBUG)
				cout << reg.registerType << " | " << reg.registerName << " | " << reg.registerValue << " | " << reg.isSaturated << " | " << reg.IOIndex << endl;
		}
		else
		{
			if (DEBUG)
				cout << "Doppelte Variabledenklaration" << endl;
			return false;
		}

		return true;
	}
	// CHECKED
	// Teste auf Leerzeile
	else if (std::regex_match(input, pattern2))
	{
		if (DEBUG)
			cout << "Leerzeile gefunden" << endl;
		return true;
	}

	// CHECKED
	// Teste auf Deklarationen: TRAM
	else if (std::regex_match(input, match, pattern3))
	{
		if (DEBUG)
			cout << "Deklaration TRAMSize gefunden" << endl;
		std::string keyword = match[1];
		std::string tramSize = match[2];
		if (keyword == "itramsize")
		{
			iTRAMSize = stoi(tramSize);
			if (DEBUG)
				cout << "iTRAMSize: " << iTRAMSize << endl;
			// TODO: Delay Vector Resize?
		}
		else if (keyword == "xtramsize")
		{
			xTRAMSize = stoi(tramSize);
			if (DEBUG)
				cout << "xTRAMSize: " << xTRAMSize << endl;
		}
		return true;
	}

	// CHECKED
	// Teste auf Instruktionen
	else if (std::regex_match(input, match, pattern4))
	{
		if (DEBUG)
			cout << "Instruktion gefunden" << endl;
		std::string keyword = match[1];
		std::string R = match[2];
		std::string A = match[3];
		std::string X = match[4];
		std::string Y = match[5];

		// Instruction
		Instruction instruction;
		int instructionName = opcodeMap[keyword];
		instruction.opcode = instructionName;
		if (DEBUG)
			cout << "INSTR: " << instructionName << " | ";

		// Register R
		instruction.operand1 = mapRegisterToIndex(R);
		if (instruction.operand1 == -1)
		{
			if (DEBUG)
				cout << "Variable nicht deklariert" << endl;
			return false;
		}
		else
		{
			if (DEBUG)
				cout << "R: " << instruction.operand1 << " | ";
		}

		// Register A
		instruction.operand2 = mapRegisterToIndex(A);
		if (instruction.operand2 == -1)
		{
			if (DEBUG)
				cout << "Variable nicht deklariert" << endl;
			return false;
		}
		else
		{
			if (DEBUG)
				cout << "A: " << instruction.operand2 << " | ";
		}

		// Register X
		instruction.operand3 = mapRegisterToIndex(X);
		if (instruction.operand3 == -1)
		{
			if (DEBUG)
				cout << "Variable nicht deklariert" << endl;
			return false;
		}
		else
		{
			if (DEBUG)
				cout << "X: " << instruction.operand3 << " | ";
		}

		// Register Y
		instruction.operand4 = mapRegisterToIndex(Y);
		if (instruction.operand4 == -1)
		{
			if (DEBUG)
				cout << "Variable nicht deklariert" << endl;
			return false;
		}
		else
		{
			if (DEBUG)
				cout << "Y: " << instruction.operand4 << endl;
		}

		// Erzeuge neue Instruction (pure Integer Repraesentation) in Instructions, z.B. {INSTR,R,A,X,Y} = {1,0,1,2,3}
		instructions.push_back(instruction);
		return true;
	}
	else if (std::regex_match(input, match, pattern6))
	{
		if (DEBUG)
			cout << "END gefunden" << endl;
		instructions.push_back({END});
		return true;
	}
	else if (std::regex_match(input, match, pattern7))
	{
		// Kommt erstmal nicht zum Einsatz. Code wird in loadFile() von Kommentaren bereinigt.
		if (DEBUG)
			cout << "Kommentar gefunden" << endl;
		return true;
	}
	return false;
}

// Gibt gemappten Registerindex zurück
int FX8010::mapRegisterToIndex(const string &registerName)
{
	int index = findRegisterIndexByName(registers, registerName);
	// Wenn Register nicht existiert wurde und ein Zahl ist
	// (Zahlen können auch ohne Deklaration in den Instructions verwendet werden)
	if (index == -1 && isNumber(registerName))
	{
		// Lege neues Register an
		Register reg;
		// Zahlen in Sourcecode Instructions sind immer STATIC
		reg.registerType = STATIC;
		reg.registerName = registerName;
		reg.registerValue = stof(registerName);
		//if (DEBUG) cout << reg.registerValue;
		registers.push_back(reg);
		// Ermittle Index des neu angelegten Registers
		return index = findRegisterIndexByName(registers, registerName);
	}
	// wenn Register nicht existiert und keine Zahl (haette deklariert sein muessen)
	else if (index == -1 && !isNumber(registerName))
	{
		return -1;
	}
	// wenn Register schon existiert
	else if (index >= 0)
	{
		return index;
	}
	return -1;
}

// CHECKED
bool FX8010::loadFile(const string &path)
{
	ifstream file(path);  // Dateipfad zum Textfile (im Binary-Ordner)
	string line;		  // einzelne Zeile
	vector<string> lines; // mehrere Zeilen
	if (file)
	{
		if (DEBUG)
			cout << "Lade File: " << path << endl;
		int row = 1;
		while (getline(file, line)) // Zeile für Zeile einlesen
		{
			// Entferne den Kommentaranteil
			std::string cleanedCode;
			size_t commentPos = line.find(';');
			if (commentPos != std::string::npos)
			{
				line = line.substr(0, commentPos);
			}

			// Hänge die gereinigte Zeile an den bereinigten Code an
			cleanedCode += line;
			cleanedCode += '\n';

			// Zeile (String) in Vector schreiben
			lines.push_back(line);
		}

		// Wandle alle Elemente in Kleinbuchstaben um
		// NOTE: tolower() ist Lambda Funktion in transform()
		std::transform(lines.begin(), lines.end(), lines.begin(), [](std::string str)
					   {
        for (char& c : str) {
            c = std::tolower(c);
        }
        return str; });

		// Syntaxcheck/Parser/Mapper
		//--------------------------
		// Überprüfe zeilenweise auf bestimmtes Regex-Muster
		// TODO: Wenn (nicht)bekannt verzweige in entsprechende Auswertung des Schlüsselworts -> Art des Fehlers ausgeben
		//   1. Keyword erkennen
		//   2. Keyword entfernen und Rest erkennen (jedes mögliche Muster einzeln)
		//   3. genaue Fehlerbeschreibung erzeugen
		//   4. Fehlercode zurückgeben und über Map ausgeben

		for (const string &line : lines)
		{
			if (DEBUG)
				cout << "Zeile " << row << ": ";
			// cout << line << endl; // Zeilenweise ausgeben
			if (!syntaxCheck(line))
			{
				if (DEBUG)
					cout << colorMap[COLOR_RED] << "Syntaxfehler gefunden" << colorMap[COLOR_NULL] << endl; // Ausgabe Syntaxfehler mit Zeilennummer, beginnend mit 0
				if (DEBUG)
					cout << colorMap[COLOR_YELLOW] << "Bitte beachte: korrekte Schreibweise der Schluesselwoerter, keine mehrfachen \nDeklarationen von Variablen, keine Sonderzeichen, Dezimalzahlen mit '.'" << colorMap[COLOR_NULL] << endl;
				return false;
			}
			row++;
		}

		if (DEBUG)
			cout << colorMap[COLOR_GREEN] << "Keine Syntaxfehler gefunden." << colorMap[COLOR_NULL] << endl;
		return true;
	}
	else
	{
		if (DEBUG)
			cout << colorMap[COLOR_RED] << "Fehler beim Oeffnen der Datei." << colorMap[COLOR_NULL] << endl;
		return false;
	}
}

// CHECKED
// Um zu prüfen, ob ein Register mit registerName="a" bereits im Vector registers vorhanden ist
// Wenn nicht gefunden gib -1 zurück, wenn ja gib den Index zurück.
int FX8010::findRegisterIndexByName(const std::vector<Register> &registers, const std::string &name)
{
	for (int i = 0; i < registers.size(); ++i)
	{
		if (registers[i].registerName == name)
		{
			return i; // Index des gefundenen Registers zurückgeben
		}
	}
	return -1; // -1 zurückgeben, wenn das Register nicht gefunden wurde
}

// NOT CHECKED
// Implement a method to write a sample into each delay line. When writing a sample,
// you need to update the write position and wrap it around if it exceeds the buffer size.
inline void FX8010::writeSmallDelay(float sample)
{
	smallDelayBuffer[smallDelayWritePos] = sample;
	smallDelayWritePos = (smallDelayWritePos + 1) % smallDelaySize;
}

inline void FX8010::writeLargeDelay(float sample)
{
	largeDelayBuffer[largeDelayWritePos] = sample;
	largeDelayWritePos = (largeDelayWritePos + 1) % largeDelaySize;
}

// NOT CHECKED
// Implement a method to read a sample from each delay line (with linear interpolation).
// To do linear interpolation, you need to find the fractional part of the read position
// and interpolate between the two adjacent samples.

// linear interpolation: lerp(sample1, sample2, 0.5)

inline float FX8010::readSmallDelay(int position)
{
	int readPos = position % smallDelaySize;
	return smallDelayBuffer[readPos];
}

inline float FX8010::readLargeDelay(int position)
{
	int readPos = position % largeDelaySize;
	return largeDelayBuffer[readPos];
}

// Funktion, die vier float-Argumente erwartet und eine zeilenweise Ausgabe in der gewünschten Form erstellt
void FX8010::printRow(const float value1, const float value2, const float value3, const float value4, const double accumulator)
{
	int columnWidth = 16; // Spaltenbreite

	// Zeile ausgeben
	if (DEBUG)
		std::cout << "R" << std::setw(columnWidth) << value1 << " | ";
	if (DEBUG)
		std::cout << "A" << std::setw(columnWidth) << value2 << " | ";
	if (DEBUG)
		std::cout << "X" << std::setw(columnWidth) << value3 << " | ";
	if (DEBUG)
		std::cout << "Y" << std::setw(columnWidth) << value4 << " | ";
	if (DEBUG)
		std::cout << "ACCU" << std::setw(columnWidth) << accumulator << std::endl;
}

int FX8010::getInstructionCounter()
{
	return dsp.instructionCounter;
}

float FX8010::process(float inputBuffer)
{
	//float R = 0, A = 0, X = 0, Y = 0; // TODO: nach DSP
	bool isEND = false;
	float outputSample = 0;
	int numSkip = 0;
	// Durchlaufen der Instruktionen und Ausfuehren des Emulators
	do
	{
		if (numSkip == 0)
		{
			for (auto &instruction : instructions)
			{
				// Zugriff auf die Operanden und Registerinformationen
				const int opcode = instruction.opcode; // read only
				int operand1Index = instruction.operand1;
				int operand2Index = instruction.operand2;
				int operand3Index = instruction.operand3;
				int operand4Index = instruction.operand4;

				// Zugriff auf die Register und deren Daten
				Register &R = registers[operand1Index]; // read/write
				Register &A = registers[operand2Index]; // read/write
				Register &X = registers[operand3Index]; // read/write
				Register &Y = registers[operand4Index]; // read/write

				// Hier koennen Sie die Instruktionen ausfuehren, basierend auf den Registern und Opcodes
				// TODO: Stereo Inputs? , hasInput Check als Instruktionmember (beim Syntaxcheck setzen),
				// um diese if-Bedingungen zu reduzieren

				if (A.registerType == INPUT)
					A.registerValue = inputBuffer; // Sample Input, TODO: dsp.inputSample[A.IOIndex], Input Buffer as Vector with numChannels?
				if (X.registerType == INPUT)
					X.registerValue = inputBuffer;
				if (Y.registerType == INPUT)
					Y.registerValue = inputBuffer;

				// Registers before processing
				// printRow(R.registerValue, A.registerValue, X.registerValue, Y.registerValue, dsp.accumulator);

				switch (opcode)
				{
				case MAC:
					// R = A + X * Y
					R.registerValue = A.registerValue + X.registerValue * Y.registerValue;
					dsp.accumulator = R.registerValue; // Copy unsaturated value into accumulator
					// Saturation
					R.registerValue = hardClip(R.registerValue, 1.0);
					// Set saturation flag
					R.isSaturated = true;
					// Set CCR register based on R
					setCCR(R.registerValue, R.isSaturated);
					break;
				case MACINT:
					// R = A + X * Y
					R.registerValue = A.registerValue + X.registerValue * Y.registerValue;
					dsp.accumulator = R.registerValue; // Copy unsaturated value into accumulator
					// Saturation
					R.registerValue = hardClip(R.registerValue, 1.0);
					// Set saturation flag
					R.isSaturated = true;
					// Set CCR register based on R
					setCCR(R.registerValue, R.isSaturated);
					break;
				case ACC3:
					// R = A + X + Y;
					R.registerValue = A.registerValue + X.registerValue + Y.registerValue;
					dsp.accumulator = R.registerValue; // Copy unsaturated value into accumulator
					// Saturation
					R.registerValue = hardClip(R.registerValue, 1.0);
					// Set saturation flag
					R.isSaturated = true;
					// Set CCR register based on R
					setCCR(R.registerValue, R.isSaturated);
					break;
				/*case LOG:
					R = linearInterpolate(A, lookupTablesLog[X], 0, 1.0);
					dsp.accumulator = R;
					//operand1Register.registerValue = R;
					break;
				case EXP:
					R = linearInterpolate(A, lookupTablesExp[X], 0, 1.0);
					dsp.accumulator = R;
					//operand1Register.registerValue = R;
					break;
				case MACW:
					R = wrapAround(A + X * Y); // TODO: Check
					dsp.accumulator = R;
					//operand1Register.registerValue = R;
					break;
				case MACINTW:
					R = wrapAround(A + X * Y); // TODO: Check
					dsp.accumulator = R;
					//operand1Register.registerValue = R;
					break;
				case MACMV:
					dsp.accumulator = dsp.accumulator + X * Y;
					R = A;
					//operand1Register.registerValue = R;
					break;
				case ANDXOR:
					// TODO: Funktioniert Typkonversion?
					R = logicOps((int)A.registerValue, (int)X.registerValue, (int)Y.registerValue);
					break;
				case TSTNEG:
					R = A >= Y ? X : -X; // TODO: Check
					dsp.accumulator = R;
					//operand1Register.registerValue = R;
					break;
				case LIMIT:
					R = A >= Y ? X : Y; // TODO: Check
					dsp.accumulator = R;
					//operand1Register.registerValue = R;
					break;
				case LIMITN:
					R = A < Y ? X : Y; // TODO: Check
					dsp.accumulator = R;
					//operand1Register.registerValue = R;
					break;*/
				case SKIP:
					if ((int)X.registerValue == getCCR())
						numSkip = Y.registerValue; // Wenn X = CCR, dann überspringe Y Instructions.
					break;
				case INTERP:
					R.registerValue = (1.0 - X.registerValue) * A.registerValue + (X.registerValue * Y.registerValue);
					dsp.accumulator = R.registerValue;
					break;
				case IDELAY:
					if (R.registerType == READ)
					{
						A.registerValue = readSmallDelay(Y.registerValue /*-2048*/); // Y = Adresse, (Y-2048) mit 11 Bit Shift
					}
					else if (R.registerType == WRITE)
					{
						writeSmallDelay(A.registerValue); // A = value
					}
					break;
				case XDELAY:
					if (R.registerType == READ)
					{
						A.registerValue = readLargeDelay(Y.registerValue /*-2048*/);
					}
					else if (R.registerType == WRITE)
					{
						writeLargeDelay(A.registerValue);
					}
					break;
				case END:
					// End of sample cycle
					isEND = true;
					break;
				default:
					// std::cout << "Opcode: " << opcode << " ist nicht implementiert." << std::endl;
					break;
				}

				dsp.instructionCounter++;
				
				if (DEBUG && !isEND)
					printRow(R.registerValue, A.registerValue, X.registerValue, Y.registerValue, dsp.accumulator);

				// wenn R = OUTPUT Typ, an VST Output zurückgeben
				if (R.registerType == OUTPUT)
				{
					return R.registerValue;
				}

				// Reset aller TEMP Register
				if (isEND)
				{
					// Reset temp registers, TODO: Need this?
					if (R.registerType == TEMP)
						R.registerValue = 0;
					if (A.registerType == TEMP)
						A.registerValue = 0;
					if (X.registerType == TEMP)
						X.registerValue = 0;
					if (Y.registerType == TEMP)
						Y.registerValue = 0;
				}
				// Debug outputs
				// std::cout << testSample[i] << " , " << operand1Register.registerValue << std::endl; // CVS Daten in Console (als tabelle für https://www.desmos.com/)
				// std::cout << "(" << testSample[i] << " , " << perand1Register.registerValue << ")" << std::endl; // CVS Daten in Console (als punktfolge für https://www.desmos.com/)
				// data.push_back({ std::to_string(testSample[i]) , std::to_string(R) }); // CVS Daten in Vector zum speichern
				// Register nach process()
			}
		}
		else
		{
			numSkip = (numSkip > 0) ? numSkip-- : 0;
			if (DEBUG)
				cout << "Skip instruction!" << endl;
		}
	} while (!isEND);
}