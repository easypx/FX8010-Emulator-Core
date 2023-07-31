// Copyright 2023 Klangraum

// IDEA: Map mit Registern statt Vector, Zugriff ueber RegisterName
// IDEA: Sourcecode mit Stringstream direkt auswerten (ChatGPT's Idee, Source in /temp)

#include "../include/FX8010.h"
#include "../include/helpers.h"

// Namespace Klangraum
namespace Klangraum
{
	FX8010::FX8010(int channels) : numChannels(channels)
	{
		initialize();
	};

	FX8010::~FX8010(){};

	void FX8010::initialize()
	{
		if (DEBUG)
			printLine(80);

		if (DEBUG)
			cout << "Initialisiere den DSP..." << endl;

		// Erstelle Fehler-Map
		//--------------------------------------------------------------------------------
		if (DEBUG)
			cout << "Erstelle die Fehler-Map" << endl;
		errorMap[ERROR_NONE] = "Kein Fehler";
		errorMap[ERROR_INVALID_INPUT] = "Ungültige Eingabe";
		errorMap[ERROR_DIVISION_BY_ZERO] = "Division durch Null";
		errorMap[ERROR_MULTIPLE_VAR_DECLARE] = "Mehrfache Variablendeklaration";
		errorMap[ERROR_VAR_NOT_DECLARED] = "Variable nicht deklariert";
		errorMap[ERROR_INPUT_FOR_R_NOT_ALLOWED] = "Verwendung von Input fuer R ist nicht erlaubt";
		errorMap[ERROR_NO_END_FOUND] = "Kein 'END' gefunden";
		errorMap[ERROR_IO_INDEX_OUT_OF_RANGE] = "I/O Index ausserhalb des gueltigen Bereichs (max. " + std::to_string(numChannels) + ")";
		errorMap[ERROR_SYNTAX_NOT_VALID] = "Ungueltige Syntax";
		errorMap[ERROR_ITRAMSIZE_TO_LARGE] = "iTRAM Size ausserhalb des gueltigen Bereichs (max. " + std::to_string(MAX_IDELAY_SIZE) + ")";
		errorMap[ERROR_XTRAMSIZE_TO_LARGE] = "xRAM Size ausserhalb des gueltigen Bereichs (max. " + std::to_string(MAX_XDELAY_SIZE) + ")";

		// Initialisieren
		errorList.clear();

		// First error is no error
		error.errorDescription = errorMap[ERROR_NONE];
		errorList.push_back(error);

		// Spezialregister
		//--------------------------------------------------------------------------------
		// Lege CCR Register an. Wir nutzen es wie ein GPR.
		// NOTE: Map wäre in Zukunft besser, um GPR oder Instruktion über Stringlabel zu identifizieren.
		if (DEBUG)
			cout << "Lege Spezialregister an (CCR, READ, WRTIE, AT)" << endl;

		registers.push_back({CCR, "ccr", 0, 0}); // GPR Index 0

		// Lege weitere (Pseudo)-Register an. Sie dienen nur als Label und werden nicht als Speicher verwendet.
		registers.push_back({READ, "read", 0, 0});	 // GPR Index 1
		registers.push_back({WRITE, "write", 0, 0}); // GPR Index 2
		registers.push_back({AT, "at", 0, 0});		 // GPR Index 3

		// LOG, EXP Tables anlegen (Wertebereich: -1.0 bis 1.0)
		//--------------------------------------------------------------------------------
		// Schar von Lookup Tables in einem Vector
		// TODO(Klangraum): sign
		if (DEBUG)
			cout << "Erzeuge LOG, EXP Lookuptables" << endl;

		int numExponent = 32;
		int numEntries = 32;
		int x_min = 0; // < 0 gives nan!
		int x_max = 1.0;

		lookupTablesLog.reserve(numExponent - 1);
		lookupTablesExp.reserve(numExponent - 1);

		// Populate lookupTablesLog with lookup tables for different i values
		// Tabellen mit (4 Vorzeichen (sign)) und 31 Exponenten je 32 Werte (2x für LOG und EXP)
		for (int i = 0; i < numExponent; i++)
		{
			std::vector<double> lookupTableLog = createLogLookupTable(0, 1.0, numEntries, i);
			std::vector<double> temp;
			temp.resize(32); // Initialisierung mit 0
			// an Y-Achse spiegeln
			temp = mirrorYVector(lookupTableLog);
			// letztes Element, jetzt 0, löschen, um doppelte 0 zu vermeiden
			// QUESTION: Brauchen wir das?
			// temp.pop_back();
			// Negieren
			temp = negateVector(temp); // = MirrorX
			// Verknüpfen Sie die beiden Vektoren
			temp = concatenateVectors(temp, lookupTableLog);
			temp.shrink_to_fit(); // um sicherzugehen, dass die Vector Size korrekt ist.
			// Schiebe neuen Vector in die Schar von LOG Vektoren
			lookupTablesLog.push_back(temp);

			std::vector<double> lookupTableExp = createExpLookupTable(0, 1.0, numEntries, i);
			temp.resize(32, 0); // Initialisierung mit 0
			// an Y-Achse spiegeln
			temp = mirrorYVector(lookupTableExp);
			// letztes Element, jetzt 0, löschen, um doppelte 0 zu vermeiden
			// QUESTION: Brauchen wir das?
			// temp.pop_back();
			// Negieren
			temp = negateVector(temp); // = MirrorX
			// Verknüpfen Sie die beiden Vektoren
			temp = concatenateVectors(temp, lookupTableExp);
			// temp1.shrink_to_fit(); // um sicherzugehen, dass die Vector Size korrekt ist.
			//  Schiebe neuen Vector in die Schar von EXP Vektoren
			lookupTablesExp.push_back(temp);
		}

		// loadFile(path);

		// Delaylines
		//--------------------------------------------------------------------------------
		// Speicherallokation nicht ganz klar. Dauert resize() zu lange?
		// Hier ist reserve() notwendig, sonst kommt es zum Zugriff auf einen leeren Vektor.
		// resize() erfolgt zusätzlich in Syntaxcheck/Parser aufgrund der Sourcecode-Deklaration.
		if (DEBUG)
			cout << "Reserviere Speicherplatz fuer Delaylines" << endl;
		// smallDelayBuffer.reserve(MAX_IDELAY_SIZE); // 100ms max. Gesamtgroesse
		// largeDelayBuffer.reserve(MAX_XDELAY_SIZE); // 1s max. Gesamtgroesse

		// I/O Buffers initialisieren?

		if (DEBUG)
			printLine(80);
	}

	// CHECKED
	// Funktion zum Erstellen der Lookup-Tabelle für die n-te Wurzel
	std::vector<double> FX8010::createLogLookupTable(double x_min, double x_max, int numEntries, int exponent)
	{
		std::vector<double> lookupTable;
		double step = (x_max - x_min) / (numEntries - 1);
		// push 1. value=0 to avoid nan! only for log(x)! change i to 1!
		// lookupTable.push_back(0.0f);
		for (int i = 0; i < numEntries; ++i)
		{
			double x = x_min + (i * step);
			// natural log as seen in dane programming pdf, issues with nan!
			// double logValue = std::log(x) / exponent + 1;
			// another good approximation! maybe this is what is really done in the dsp!
			// they mention approximation of pow and sqrt.
			double logValue = pow(x, 1.0 / static_cast<float>(exponent));
			lookupTable.push_back(logValue);
		}
		return lookupTable;
	}

	// CHECKED
	// Funktion zum Erstellen der Lookup-Tabelle für n-te Potenz (EXP)
	std::vector<double> FX8010::createExpLookupTable(double x_min, double x_max, int numEntries, int exponent)
	{
		std::vector<double> lookupTable;
		double step = (x_max - x_min) / (numEntries - 1);
		// lookupTable.push_back(0.0f); // see log for explanation
		for (int i = 0; i < numEntries; ++i)
		{
			double x = x_min + i * step;
			// double expValue = exp(x * numEntries - numEntries);
			double expValue = pow(x, static_cast<float>(exponent)); // see log for explanation
			lookupTable.push_back(expValue);
		}
		return lookupTable;
	}

	// NOT CHECKED
	// Funktion zum Spiegeln des Vectors
	std::vector<double> FX8010::mirrorYVector(const std::vector<double> &inputVector)
	{
		std::vector<double> mirroredVector;
		for (int i = inputVector.size() - 1; i >= 0; --i)
		{
			mirroredVector.push_back(inputVector[i]);
		}
		return mirroredVector;
	}

	// NOT CHECKED
	// Funktion zum Verknüpfen der Vectoren
	std::vector<double> FX8010::concatenateVectors(const std::vector<double> &vector1, const std::vector<double> &vector2)
	{
		std::vector<double> concatenatedVector;
		concatenatedVector.reserve(vector1.size() + vector2.size());
		concatenatedVector.insert(concatenatedVector.end(), vector1.begin(), vector1.end());
		concatenatedVector.insert(concatenatedVector.end(), vector2.begin(), vector2.end());
		return concatenatedVector;
	}

	// NOT CHECKED
	// Hier ist eine Funktion zum Negieren eines Vektors
	std::vector<double> FX8010::negateVector(const std::vector<double> &inputVector)
	{
		std::vector<double> negatedVector;
		negatedVector.reserve(inputVector.size());
		for (float value : inputVector)
		{
			negatedVector.push_back(-value);
		}
		return negatedVector;
	}

	vector<string> FX8010::getControlRegisters()
	{
		return controlRegisters;
	}

	// NOT CHECKED
	// Slighty modified cases, which makes more sense. ChatGPT thinks the same way.
	inline void FX8010::setCCR(const float result)
	{
		if (result == 0)
			registers[0].registerValue = 0x8; // 0b1000 | 8
		else if (result < 0 && result > -1.0)
			registers[0].registerValue = 0x4; // 0b0100 | 4
		else if (result > 0 && result < 1.0)
			registers[0].registerValue = 0x180;	  // 0b000110000000 | 384
		else if (result == 1.0 && result == -1.0) // Saturation
			registers[0].registerValue = 0x10;	  // 0b00010000 | 16
		else									  // != 0
			registers[0].registerValue = 0x100;	  // 0b000100000000 | 256
	}

	// NOT CHECKED
	// Beschreibe ein Register von VST aus z.B. Sliderinput
	int FX8010::setRegisterValue(const std::string &key, float value)
	{
		bool found = false;
		for (auto &element : registers)
		{
			if (element.registerName == key)
			{
				element.registerValue = value;
				found = true;
				break;
			}
		}
		if (found)
		{
			return 0;
		}
		return 1;
	};

	// NOT CHECKED
	float FX8010::getRegisterValue(const std::string &key)
	{
		for (const auto &element : registers)
		{
			if (element.registerName == key)
			{
				return element.registerValue;
			}
		}
		return 1; // Or any other default value you want to return if the element is not found
	}

	// NOT CHECKED
	inline int FX8010::getCCR()
	{
		return registers[0].registerValue;
	}

	// CHECKED
	inline float FX8010::saturate(const float input, const float threshold)
	{
		// Ternäre Schreibweise
		return (input >= threshold) ? threshold : ((input <= -threshold) ? -threshold : input);
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
	inline float FX8010::wrapAround(const float a)
	{
		// Ternäre Schreibweise
		return (a >= 1.0f) ? (a - 2.0f) : ((a < -1.0f) ? (a + 2.0f) : a);
	}

	inline int FX8010::logicOps(const GPR &A_, const GPR &X_, const GPR &Y_)
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
		const int A = static_cast<int>(A_.registerValue);
		const int X = static_cast<int>(X_.registerValue);
		const int Y = static_cast<int>(Y_.registerValue);

		if (Y == 0)
			R = A & X; // Bitweise AND-Verknüpfung von A und X
		else if (X == 0xFFFFFF)
			R = A ^ Y; // Bitweise XOR-Verknüpfung von A und Y
		else if (X == 0xFFFFFFF && Y == 0xFFFFFF)
			R = ~A; // Bitweise NOT-Verknüpfung von A
		else if (Y == ~X)
			R = A | Y; // Bitweise OR-Verknüpfung von A und Y
		else if (Y == 0xFFFFFF)
			R = ~A & X; // Bitweise NAND-Verknüpfung von A und X
		else
			R = (A & X) ^ Y; // (A AND X) XOR Y
		return R;
	}

	// Setzen der Schreibposition in der Microcode Deklaration
	inline void FX8010::setSmallDelayWritePos(int smallDelayWritePos_)
	{
		smallDelayWritePos = smallDelayWritePos_;
	}

	inline void FX8010::setLargeDelayWritePos(int largeDelayWritePos_)
	{
		largeDelayWritePos = largeDelayWritePos_;
	}

	// vector<Klangraum::FX8010::MyError> FX8010::getErrorList()
	//{
	//	return errorList;
	// }

	// NOT CHECKED
	// Syntaxchecker/Parser/Mapper
	// NOTE: Implementation ist "Just Good Enough". Eine genauere Auswertung und mehr Fehlermeldungen sind wünschenswert.
	bool FX8010::syntaxCheck(const std::string &input)
	{
		// verschiedene Kombinationen im Deklarationsteil, auch mehrfache Vorkommen
		// std::regex pattern1(R"(^\s*(static|temp)\s+((?:\w+\s*(?:=\s*\d+(?:\.\d*)?)?\s*,?\s*)+)\s*$)");

		// Deklarationen: z.B. static a || static b = 1.0
		// std::regex pattern1(R"(^\s*(static|temp|control|input|output|const)\s+(\w+)(?:\s*=\s*(\d+(?:\.\d+)?))?\s*$)");
		std::regex pattern1(R"(^\s*(static|temp|control|input|output|const|noise)\s+(\w+)(?:[\s=,]*\s*(\d+(?:\.\d+)?))?\s*$)");

		// Leerzeile
		std::regex pattern2(R"(^\s*$)");

		// tramsize im Deklarationsteil
		std::regex pattern3(R"(^\s*(itramsize|xtramsize)\s+(\d+)$)");

		// check instructions TODO: all instructions
		std::regex pattern4(R"(^\s*(mac|macn|macint|macintw|acc3|macmv|macw|macwn|skip|andxor|tstneg|limit|limitn|log|exp|interp|idelay|xdelay)\s+([a-zA-Z0-9_.-]+|\d+\.\d+)\s*,\s*([a-zA-Z0-9_.-]+|\d+\.\d+)\s*,\s*([a-zA-Z0-9_.-]+|\d+\.\d+)\s*,\s*([a-zA-Z0-9_.-]+|\d+\.\d+)\s*$)");

		// check metadata TODO: ...
		std::regex pattern5(R"(^\s*(comment|name|guid)\s+([a-zA-Z0-9_]+)\s*,\s*([a-zA-Z0-9_.]+)\s*$)");

		// Check "END"
		std::regex pattern6(R"(^\s*(end)\s*$)");

		// Check Kommentar ";"
		std::regex pattern7(R"(^\s*;+\s*$)");

		std::smatch match;

		// Teste auf Deklarationen: static a | static b = 1.0 (vorerst keine Mehrfachdeklarationen!)
		//------------------------------------------------------------------------------------------
		if (std::regex_match(input, match, pattern1))
		{
			if (DEBUG)
				cout << "Deklaration gefunden" << endl;
			const std::string registerTyp = match[1];
			const std::string registerName = match[2];
			std::string registerValue = "";
			if (match[3].matched)
			{
				registerValue = match[3];
			}

			// Fuege zu Liste der Control Register zu
			if (registerTyp == "control")
			{
				controlRegisters.push_back(registerName);
			}

			// wenn Registername nicht existiert (-1)
			if (findRegisterIndexByName(registers, registerName) == -1)
			{
				// Lege neues Temp-GPR an
				GPR reg = {0, "", 0, 0}; // Initialisieren

				// Überprüfen, ob der String in der Map existiert
				// NOTE: Im Grunde ist das ueberfluessig, weil Regex auf Types prueft
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
					return false;
				}
				reg.registerName = registerName;
				if (!registerValue.empty())
				{
					// Wenn Input oder Output Variable, dann uebernehme Value als IOIndex
					// NOTE: Hier gibt es einen Unterschied zu KX-Driver! Dieser hat keine Definition fuer
					// IOIndex. Die In-und Outputs werden dort inkrementell erzeugt und können beliebig verknüpft werden.
					// In diesem Emulator wird nur Stereo Processing gemacht, d.h. wir müssen bei der Definition
					// IOindizes angeben. Wir könnten theoretisch auch Multichannelprocessing machen und unsere I/O
					// Indizes frei wählen.
					// TODO(Klangraum): Evtl. spaeter andere Syntax z.B input in, 0/1 oder einfach input in_l/in_r?
					if (registerTyp == "input" || registerTyp == "output")
					{
						// Wenn im Sourcecode mehr I/O deklariert werden als der Initialisierungswert.
						// TODO: I/O-Initialisierung im Sourcecode?
						if (stoi(registerValue) > numChannels - 1)
						{
							error.errorDescription = errorMap[ERROR_IO_INDEX_OUT_OF_RANGE];
							error.errorRow = errorCounter;
							errorList.push_back(error);
							if (DEBUG)
								cout << "I/O Index zu gross" << endl;
							return false;
						}
						else
						{
							reg.IOIndex = stoi(registerValue);
						}
					}
					else
					{
						reg.registerValue = stof(registerValue);
					}
				}
				else
				{
					reg.registerValue = 0;
				}
				// Schiebe befülltes GPR nach Registers
				registers.push_back(reg);
				if (DEBUG)
					cout << "GPR: " << reg.registerType << " | " << reg.registerName << " | " << reg.registerValue << " | " << reg.IOIndex << endl;
			}
			else
			{
				error.errorDescription = errorMap[ERROR_MULTIPLE_VAR_DECLARE];
				error.errorRow = errorCounter;
				errorList.push_back(error);
				if (DEBUG)
					cout << "Multiple Variabledenklaration" << endl;
				return false;
			}
			return true;
		}

		// Teste auf Leerzeile
		//------------------------------------------------------------------------------------------
		else if (std::regex_match(input, pattern2))
		{
			if (DEBUG)
				cout << "Leerzeile oder Auskommentierung gefunden" << endl;
			return true;
		}

		// Teste auf Deklarationen: TRAM
		//------------------------------------------------------------------------------------------
		else if (std::regex_match(input, match, pattern3))
		{
			if (DEBUG)
				cout << "Deklaration TRAMSize gefunden" << endl;
			const std::string keyword = match[1];
			const std::string tramSize = match[2];
			if (keyword == "itramsize")
			{
				iTRAMSize = stoi(tramSize);
				if (iTRAMSize > 4800)
				{
					if (DEBUG)
						cout << "iTRAMSize zu gross (max. 4800)" << endl;
					error.errorDescription = errorMap[ERROR_ITRAMSIZE_TO_LARGE];
					error.errorRow = errorCounter;
					errorList.push_back(error);
					return false;
				}
				else
				{
					// Größe des Delayline Vectors anpassen und initialisieren
					// smallDelayBuffer.resize(4800, 0.0);
					if (DEBUG)
						cout << "iTRAMSize: " << iTRAMSize << endl;
				}
			}
			else if (keyword == "xtramsize")
			{
				xTRAMSize = stoi(tramSize);
				if (xTRAMSize > 48000)
				{
					if (DEBUG)
						cout << "xTRAMSize zu gross (max. 48000)" << endl;
					error.errorDescription = errorMap[ERROR_XTRAMSIZE_TO_LARGE];
					error.errorRow = errorCounter;
					errorList.push_back(error);
					return false;
				}
				else
				{
					// Größe des Delayline Vectors anpassen
					// largeDelayBuffer.resize(xTRAMSize, 0.0);
					if (DEBUG)
						cout << "xTRAMSize: " << xTRAMSize << endl;
				}
			}
			return true;
		}

		// Teste auf Instruktionen
		//------------------------------------------------------------------------------------------
		// Wenn gültige Instruktion
		else if (std::regex_match(input, match, pattern4))
		{
			if (DEBUG)
				cout << "Instruktion gefunden" << endl;

			// Lege neue Instruktion an
			Instruction instruction = {0, 0, 0, 0, 0, 0, 0, 0}; // Initialisierung

			// Extrahiere aus Regex-Erfassungsgruppen
			const std::string keyword = match[1];
			const std::string R = match[2];
			const std::string A = match[3];
			const std::string X = match[4];
			const std::string Y = match[5];

			// TODO:
			// Wenn instruction.operand1.registerType = INPUT -> Fehler: R darf kein Input sein
			// Wenn A | X | Y ...registerType = INPUT -> instruction.hasInput = true

			// Instructionname
			//------------------------------------------------------------------------------------------
			int instructionName = opcodeMap[keyword];
			instruction.opcode = instructionName;

			// GPR R
			//------------------------------------------------------------------------------------------
			instruction.operand1 = mapRegisterToIndex(R);
			if (instruction.operand1 == -1)
			{
				error.errorDescription = errorMap[ERROR_VAR_NOT_DECLARED];
				error.errorRow = errorCounter;
				errorList.push_back(error);
				if (DEBUG)
					cout << "Variable nicht deklariert" << endl;
				return false;
			}
			else
			{
				// Wenn Registertyp von GPR R = INPUT
				if (registers[instruction.operand1].registerType == INPUT)
				{
					error.errorDescription = errorMap[ERROR_INPUT_FOR_R_NOT_ALLOWED];
					error.errorRow = errorCounter;
					errorList.push_back(error);
					if (DEBUG)
						cout << "Verwendung von Input fuer R ist nicht erlaubt" << endl;
					return false;
				}
				else if (registers[instruction.operand1].registerType == OUTPUT)
				{
					// Setze Flag instruction.hasOutput = true
					instruction.hasOutput = true;
				}

				if (DEBUG)
					cout << "R: " << instruction.operand1 << " | ";
			}

			// GPR A
			//------------------------------------------------------------------------------------------
			instruction.operand2 = mapRegisterToIndex(A);
			if (instruction.operand2 == -1)
			{
				error.errorDescription = errorMap[ERROR_VAR_NOT_DECLARED];
				error.errorRow = errorCounter;
				errorList.push_back(error);
				if (DEBUG)
					cout << "Variable nicht deklariert" << endl;
				return false;
			}
			else
			{
				// Wenn Registertyp von GPR A = INPUT
				if (registers[instruction.operand2].registerType == INPUT)
				{
					// Setze Flag instruction.hasInput
					instruction.hasInput = true;
				}
				else if (registers[instruction.operand2].registerType == NOISE)
				{
					instruction.hasNoise = true;
				}

				if (DEBUG)
					cout << "A: " << instruction.operand2 << " | ";
			}

			// GPR X
			//------------------------------------------------------------------------------------------
			instruction.operand3 = mapRegisterToIndex(X);
			if (instruction.operand3 == -1)
			{
				error.errorDescription = errorMap[ERROR_VAR_NOT_DECLARED];
				error.errorRow = errorCounter;
				errorList.push_back(error);
				if (DEBUG)
					cout << "Variable nicht deklariert" << endl;
				return false;
			}
			else
			{
				// Wenn Registertyp von GPR X = INPUT
				if (registers[instruction.operand3].registerType == INPUT)
				{
					// Setze Flag instruction.hasInput
					instruction.hasInput = true;
				}
				else if (registers[instruction.operand3].registerType == NOISE)
				{
					instruction.hasNoise = true;
				}
				if (DEBUG)
					cout << "X: " << instruction.operand3 << " | ";
			}

			// GPR Y
			//------------------------------------------------------------------------------------------

			instruction.operand4 = mapRegisterToIndex(Y);
			if (instruction.operand4 == -1)
			{
				error.errorDescription = errorMap[ERROR_VAR_NOT_DECLARED];
				error.errorRow = errorCounter;
				errorList.push_back(error);
				if (DEBUG)
					cout << "Variable nicht deklariert" << endl;
				return false;
			}
			else
			{
				// Wenn Registertyp von GPR Y = INPUT
				if (registers[instruction.operand4].registerType == INPUT)
				{
					// Setze Flag instruction.hasInput
					instruction.hasInput = true;
				}
				else if (registers[instruction.operand4].registerType == NOISE)
				{
					instruction.hasNoise = true;
				}
				if (DEBUG)
					cout << "Y: " << instruction.operand4 << endl;
			}

			// Erzeuge neue Instruction (pure Integer Repraesentation) in Instructions, z.B. {INSTR,R,A,X,Y} => {1,0,1,2,3}
			instructions.push_back(instruction);
			return true;
		}

		// Teste auf END
		//------------------------------------------------------------------------------------------
		else if (std::regex_match(input, match, pattern6))
		{
			if (DEBUG)
				cout << "END gefunden" << endl;
			instructions.push_back({END});
			return true;
		}

		// Teste auf Kommentar
		// NOTE: Kommentare werden vorher entfernt und mit Leerzeile ersetzt, um Zeilennummern
		// beizubehalten! (verbesserungswürdig)
		//------------------------------------------------------------------------------------------
		else if (std::regex_match(input, match, pattern7))
		{
			// Kommt erstmal nicht zum Einsatz. Code wird in loadFile() von Kommentaren bereinigt.
			if (DEBUG)
				cout << "Kommentar gefunden" << endl;
		}
		// Wenn nichts zutrifft
		else
		{
			if (DEBUG)
				std::cout << "Ungueltige Syntax" << std::endl;
			error.errorDescription = errorMap[ERROR_SYNTAX_NOT_VALID];
			error.errorRow = errorCounter;
			errorList.push_back(error);
			return false;
		}
		return true;
	}

	// CHECKED
	// Gibt gemappten Registerindex zurück
	int FX8010::mapRegisterToIndex(const string &registerName)
	{
		int index = findRegisterIndexByName(registers, registerName);
		// Wenn GPR nicht existiert wurde und ein Zahl ist
		// (Zahlen können auch ohne Deklaration in den Instructions verwendet werden)
		if (index == -1 && isNumber(registerName))
		{
			// Lege neues GPR an
			GPR reg;
			// Zahlen in Sourcecode Instructions sind immer STATIC
			reg.registerType = STATIC;
			reg.registerName = registerName;
			reg.registerValue = stof(registerName);
			// if (DEBUG) cout << reg.registerValue;
			registers.push_back(reg);
			// Ermittle Index des neu angelegten Registers
			return findRegisterIndexByName(registers, registerName);
		}
		// wenn GPR nicht existiert und keine Zahl (haette deklariert sein muessen)
		else if (index == -1 && !isNumber(registerName))
		{
			return -1;
		}
		// wenn GPR schon existiert
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
		if (DEBUG)
			cout << "Lade File: " << path << endl;
		if (file)
		{
			if (DEBUG)
				cout << "Parse Quellcode. NOTE: Kommentarzeilen werden durch Leerzeilen ersetzt." << endl;
			if (DEBUG)
				printLine(80);
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

				// In Kleinbuchstaben umwandeln
				for (char &c : line)
				{
					c = std::tolower(c);
				}

				// Zeile (String) in Vector schreiben
				lines.push_back(line);
			}

			// Syntaxcheck/Parser/Mapper
			//--------------------------------------------------------------------------------
			// TODO: Überprüfe jede mögliche Deklaration/Instruktion mit bestimmtem Regex-Muster.
			// zusätzlich Range-Check, ...

			for (const string &line : lines)
			{
				if (DEBUG)
					cout << "Zeile " << errorCounter << ": ";
				syntaxCheck(line);
				errorCounter++;
			}

			// Auf das letzte Vektor-Element zugreifen
			// TODO: Sinnvoll?
			string lastElement = lines.back();
			// Wenn kein END Keyword gefunden wird
			if (lastElement != "end")
			{
				error.errorDescription = errorMap[ERROR_NO_END_FOUND];
				error.errorRow = errorCounter;
				errorList.push_back(error);
				if (DEBUG)
					cout << "Kein 'END' gefunden" << endl;
			}

			if (DEBUG)
				printLine(80);

			// Wenn errorList > 1 (1. error ist ERROR_NONE)
			int numErrors = errorList.size();
			if (numErrors > 1)
			{
				if (DEBUG)
					cout << colorMap[COLOR_RED] << "Syntaxfehler gefunden" << colorMap[COLOR_NULL] << endl; // Ausgabe Syntaxfehler mit Zeilennummer, beginnend mit 0

				for (int i = 1; i < numErrors; i++)
				{
					if (DEBUG)
						cout << errorList[i].errorDescription << " (" << errorList[i].errorRow << ")" << endl;
				}
				if (DEBUG)
					printLine(80);
				if (DEBUG)
					cout << colorMap[COLOR_YELLOW] << "Bitte beachte: korrekte Schreibweise der Schluesselwoerter, keine mehrfachen \nDeklarationen von Variablen, keine Sonderzeichen, Dezimalzahlen mit '.', \nR darf kein Input sein, Buffern der Inputs wird empfohlen" << colorMap[COLOR_NULL] << endl;
				return false;
			}
			else
			{
				if (DEBUG)
					cout << colorMap[COLOR_GREEN] << "Keine Syntaxfehler gefunden." << colorMap[COLOR_NULL] << endl;
				if (DEBUG)
					printLine(80);
			}
		}
		else
		{
			if (DEBUG)
				cout << colorMap[COLOR_RED] << "Fehler beim Oeffnen der Datei." << colorMap[COLOR_NULL] << endl;
			return false;
		}
		return true;
	}

	// CHECKED
	// Um zu prüfen, ob ein GPR mit registerName="a" bereits im Vector registers vorhanden ist
	// Wenn nicht gefunden gib -1 zurück, wenn ja gib den Index zurück.
	int FX8010::findRegisterIndexByName(const std::vector<GPR> &registers, const std::string &name)
	{
		for (int i = 0; i < registers.size(); ++i)
		{
			if (registers[i].registerName == name)
			{
				return i; // Index des gefundenen Registers zurückgeben
			}
		}
		return -1; // -1 zurückgeben, wenn das GPR nicht gefunden wurde
	}

	// NOT CHECKED
	// Gibt Referenz auf ErrorList zurück
	std::vector<FX8010::MyError> FX8010::getErrorList()
	{
		return errorList;
	}

	// Gib minVal oder maxVal zurück, wenn Grenzen überschritten werden, ansonsten value
	// wie saturate()
	int clamp(int value, int minVal, int maxVal)
	{
		return std::max(minVal, std::min(value, maxVal));
	}

	// NOT CHECKED
	// Implement a method to write a sample into each delay line. When writing a sample,
	// you need to update the write position and wrap it around if it exceeds the buffer size.
	inline void FX8010::writeSmallDelay(float sample, int position_)
	{
		// Range-Check
		position_ = std::max(0, std::min(position_, iTRAMSize - 1));
		// Schreibe Sample in Delayline
		smallDelayBuffer[smallDelayWritePos + position_] = sample;
		// Inkrementiere Schreibpointer (Ringbuffer)
		smallDelayWritePos = (smallDelayWritePos + 1) % iTRAMSize;
	}

	inline void FX8010::writeLargeDelay(float sample, int position_)
	{
		// Range-Check
		position_ = std::max(0, std::min(position_, xTRAMSize - 1));
		// Schreibe Sample in Delayline
		largeDelayBuffer[largeDelayWritePos + position_] = sample;
		// Inkrementiere Schreibpointer (Ringbuffer)
		largeDelayWritePos = (largeDelayWritePos + 1) % xTRAMSize;
	}

	// NOT CHECKED
	// Implement a method to read a sample from each delay line.
	// To do linear interpolation, you need to find the fractional part of the read position
	// and interpolate between the two adjacent samples.

	inline float FX8010::readSmallDelay(int position_)
	{
		// Erläuterung zum Ringbuffer:
		// Lesepointer läuft Schreibpointer hinterher!
		// Grundzustand: Lesepointer zeigt auf letztes Element in der Delayline (Index: iTRAMSize - 1),
		// Schreibpointer zeigt auf 1. Element. (Index: 0)
		// Wenn Lesepointer (position_ > 0) müssen wir diesen zurückstellen im Gegensatz zu Schreibpointer,
		// der vorgestellt werden muss, wenn wir spaeter Schreiben wollen! Normalerweise bleibt der dieser
		// zu Beginn auf Index: 0.
		// Beide Pointer werden beim Aufruf der Methoden inkrementiert mit Wraparound.

		// Range-Check
		// smallDelayReadPos wird mit (iTRAMSize-1) initialisiert, welches das letzte Element ist!
		position_ = std::max(0, std::min(position_, iTRAMSize - 1));

		// Lese Sample aus Delayline
		// Mit (smallDelayReadPos - position_) stellen wir den Lesepointer zurück.
		// NOTE: Modulo Operator sorgt für einen Wraparound.
		float out = smallDelayBuffer[(smallDelayReadPos - position_) % iTRAMSize];
		// Inkrementiere Lesepointer (Ringbuffer mit Wraparound)
		smallDelayReadPos = (smallDelayReadPos + 1) % iTRAMSize;
		return out;
	}

	inline float FX8010::readLargeDelay(int position_)
	{
		// Range-Check
		position_ = std::max(0, std::min(position_, xTRAMSize - 1));
		// Lese Sample aus Delayline
		float out = largeDelayBuffer[(largeDelayReadPos - position_) % xTRAMSize];
		// Inkrementiere Lesepointer (Ringbuffer)
		largeDelayReadPos = (largeDelayReadPos + 1) % xTRAMSize;
		return out;
	}

	// Funktion, die vier float-Argumente erwartet und eine zeilenweise Ausgabe in der gewünschten Form erstellt
	void FX8010::printRow(const int instruction, const float value1, const float value2, const float value3, const float value4, const double accumulator)
	{
		int columnWidth = 12; // Spaltenbreite

		// Vergleich mit Null mit einer kleinen Toleranz
		double tolerance = 1e-6;

		// Zeile ausgeben
		std::cout << "INSTR" << std::setw(columnWidth / 2) << instruction << " | ";
		std::cout << "R" << std::setw(columnWidth) << ((std::abs(value1) < tolerance) ? 0.0 : value1) << " | ";
		std::cout << "A" << std::setw(columnWidth) << ((std::abs(value2) < tolerance) ? 0.0 : value2) << " | ";
		std::cout << "X" << std::setw(columnWidth) << ((std::abs(value3) < tolerance) ? 0.0 : value3) << " | ";
		std::cout << "Y" << std::setw(columnWidth) << ((std::abs(value4) < tolerance) ? 0.0 : value4) << " | ";
		std::cout << "ACCU" << std::setw(columnWidth) << ((std::abs(accumulator) < tolerance) ? 0.0 : accumulator) << std::endl;
	}

	int FX8010::getInstructionCounter()
	{
		return instructionCounter;
	}

	// Fast White Noise
	// Linear Feedback Shift Register (LFSR) als Pseudo-Zufallszahlengenerator
	float FX8010::whitenoise()
	{
		float noise = 0.0f;
		g_x1 ^= g_x2;
		noise = g_x2 * g_fScale;
		g_x2 += g_x1;
		return noise;
	}

	// Hier werden Instruktionen ausgefuehrt, basierend auf den Registern und Opcodes
	std::vector<float> FX8010::process(const std::vector<float> &inputSamples)

	{
		// Endflag fuer einen Samplezyklus. Wird mit END im Sourcecode gesetzt.
		bool isEND = false;
		// Initialisiere Outputbuffer
		std::vector<float> outputSamples;
		outputSamples.resize(numChannels);
		// Skip n Instructions
		int numSkip = 0;

		// Durchlaufen der Instruktionen und Ausfuehren des Emulators
		do
		{
			for (auto &instruction : instructions)
			{
				if (numSkip == 0)
				{
					// Zugriff auf die Operanden und Registerinformationen
					const int opcode = instruction.opcode; // read only
					int operand1Index = instruction.operand1;
					int operand2Index = instruction.operand2;
					int operand3Index = instruction.operand3;
					int operand4Index = instruction.operand4;

					// Zugriff auf die GPR und deren Daten
					GPR &R = registers[operand1Index]; // read/write
					GPR &A = registers[operand2Index]; // read/write
					GPR &X = registers[operand3Index]; // read/write
					GPR &Y = registers[operand4Index]; // read/write
					
					// Hier werden nur Instruktionen mit entsprechendem Flag getestet.
					if (instruction.hasInput)
					{
						if (A.registerType == INPUT)
							A.registerValue = inputSamples[A.IOIndex];
						if (X.registerType == INPUT)
							X.registerValue = inputSamples[A.IOIndex];
						if (Y.registerType == INPUT)
							Y.registerValue = inputSamples[A.IOIndex];
					}
					// Es genügt wenn ein Register NOISE sein kann. (deswegen else if)
					if (instruction.hasNoise)
					{
						if (A.registerType == NOISE)
							A.registerValue = whitenoise();
						else if (X.registerType == NOISE)
							X.registerValue = whitenoise();
						else if (Y.registerType == NOISE)
							Y.registerValue = whitenoise();
					}

					// Befehlsdecoder
					//--------------------------------------------------------------------------------
					switch (opcode)
					{
					case MAC:
						// R = A + X * Y
						R.registerValue = A.registerValue + X.registerValue * Y.registerValue;
						accumulator = R.registerValue; // Copy unsaturated value into accumulator
						// Saturation
						R.registerValue = saturate(R.registerValue, 1.0f);
						// Set CCR register based on R
						setCCR(R.registerValue);
						break;
					case MACN:
						// R = A - X * Y
						R.registerValue = A.registerValue - X.registerValue * Y.registerValue;
						accumulator = R.registerValue; // Copy unsaturated value into accumulator
						// Saturation
						R.registerValue = saturate(R.registerValue, 1.0f);
						// Set CCR register based on R
						setCCR(R.registerValue);
						break;
					case MACINT:
						// R = A + X * Y
						R.registerValue = A.registerValue + X.registerValue * Y.registerValue;
						accumulator = R.registerValue; // Copy unsaturated value into accumulator
						// Saturation
						R.registerValue = saturate(R.registerValue, 1.0);
						// Set CCR register based on R
						setCCR(R.registerValue);
						break;
					case ACC3:
						// R = A + X + Y
						R.registerValue = A.registerValue + X.registerValue + Y.registerValue;
						accumulator = R.registerValue; // Copy unsaturated value into accumulator
						// Saturation
						R.registerValue = saturate(R.registerValue, 1.0);
						// Set CCR register based on R
						setCCR(R.registerValue);
						break;
					case LOG:
						// TODO: Y = sign
						R.registerValue = linearInterpolate(A.registerValue, lookupTablesLog[static_cast<int>(X.registerValue)], -1.0, 1.0);
						accumulator = R.registerValue;
						break;
					case EXP:
						R.registerValue = linearInterpolate(A.registerValue, lookupTablesExp[static_cast<int>(X.registerValue)], -1.0, 1.0);
						accumulator = R.registerValue;
						break;
					case MACW:
						R.registerValue = wrapAround(A.registerValue + X.registerValue * Y.registerValue); // TODO: Check
						accumulator = R.registerValue;
						break;
					case MACINTW:
						R.registerValue = wrapAround(A.registerValue + X.registerValue * Y.registerValue); // TODO: Check
						accumulator = R.registerValue;
						break;
					case MACMV:
						accumulator = accumulator + X.registerValue * Y.registerValue;
						R.registerValue = A.registerValue;
						break;
					case ANDXOR:
						R.registerValue = logicOps(A, X, Y);
						break;
					case TSTNEG:
						R.registerValue = A.registerValue >= Y.registerValue ? X.registerValue : -X.registerValue; // TODO: Check
						accumulator = R.registerValue;
						break;
					case LIMIT:
						R.registerValue = A.registerValue >= Y.registerValue ? X.registerValue : Y.registerValue; // TODO: Check
						accumulator = R.registerValue;
						break;
					case LIMITN:
						R.registerValue = A.registerValue < Y.registerValue ? X.registerValue : Y.registerValue; // TODO: Check
						accumulator = R.registerValue;
						break;
					case SKIP:
						// Wenn X = CCR, dann überspringe Y Instructions.
						if (static_cast<int>(X.registerValue) == registers[0].registerValue)
							numSkip = static_cast<int>(Y.registerValue);
						break;
					case INTERP:
						R.registerValue = (1.0 - X.registerValue) * A.registerValue + (X.registerValue * Y.registerValue);
						accumulator = R.registerValue;
						break;
					case IDELAY:
						// READ, A, AT, Y
						if (R.registerType == READ)
						{
							A.registerValue = readSmallDelay(static_cast<int>(Y.registerValue)); // Y = Adresse, (Y-2048) mit 11 Bit Shift
						}
						// WRITE, A, AT, Y
						else if (R.registerType == WRITE)
						{
							writeSmallDelay(A.registerValue, static_cast<int>(Y.registerValue)); // A = value
						}
						break;
					case XDELAY:
						// READ, A, AT, Y
						if (R.registerType == READ)
						{
							A.registerValue = readLargeDelay(static_cast<int>(Y.registerValue));
						}
						// WRITE, A, AT, Y
						else if (R.registerType == WRITE)
						{
							writeLargeDelay(A.registerValue, static_cast<int>(Y.registerValue));
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

					// Zähle Instruktionen
					instructionCounter++;

					// Anzeige der Registerwerte
					if (PRINT_REGISTERS && !isEND)
						printRow(opcode, R.registerValue, A.registerValue, X.registerValue, Y.registerValue, accumulator);

					// wenn R = OUTPUT Typ
					if (R.registerType == OUTPUT)
					{
						// Schreibe R in den Outputbuffer mit Index (für Mehrkanal)
						outputSamples[R.IOIndex] = R.registerValue;
					}

					// Debug outputs
					// std::cout << inputSamples[A.IOIndex] << " , " << R.registerValue << std::endl; // CVS Daten in Console (als tabelle für https://www.desmos.com/)
					// std::cout << "(" << testSample[i] << " , " << perand1Register.registerValue << ")" << std::endl; // CVS Daten in Console (als punktfolge für https://www.desmos.com/)
					// data.push_back({ std::to_string(testSample[i]) , std::to_string(R) }); // CVS Daten in Vector zum speichern
				}
				else
				{
					// Dekrementiere Skip-Counter
					numSkip = (numSkip > 0) ? --numSkip : 0;
					if (PRINT_REGISTERS)
						cout << "Skip instruction!" << endl;
				}
			}
		} while (!isEND);
		// Reset aller TEMP GPR nach einem Samplezyklus
		// NOTE: not really needed, performance issue
		// TODO: Extra Temp-Vector machen für schnelles Löschen?
		/*for (auto &reg : registers)
		{
			if (reg.registerType == TEMP)
			{
				reg.registerValue = 0;
			}
		}*/

		// Gib Vektor mit (Mehrkanal-)Sample(s) an VST zurück
		return outputSamples;
	}

} // namespace Klangraum