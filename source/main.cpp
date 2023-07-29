// Copyright 2023 Klangraum

#include "../include/FX8010.h"
#include "../include/helpers.h"
#include <chrono>

// using namespace Klangraum;

int main()
{
    // Anzahl der Channels fuer Stereo
    int numChannels = 2;

    // Create an instance of FX8010 class
    Klangraum::FX8010 *fx8010 = new Klangraum::FX8010(numChannels);

    // Vector mit Testsamples erzeugen
    int numSamples = AUDIOBLOCKSIZE;

    // Vector mit linearen Sampledaten. Nur zum Testen!
    std::vector<float> testSample;

    // Channel 0: 0 bis 1.0
    for (int i = 0; i < numSamples; i++)
    {
        float value = static_cast<float>(i) / (numSamples - 1); // Wertebereich von 0 bis 1.0
        testSample.push_back(value);
    }

    // Ausgabe des Testsamples
    // for (int i = 0; i < numSamples; i++)
    //{
    //    cout << testSample[i] << endl;
    //}

    if (fx8010->loadFile("testcode.da"))
    {
        // Diese Liste mit Controltegistern wird VST als Label-Identifizierer fuer Slider, Knobs, usw. nutzen koennen.
        vector<string> controlRegisters = fx8010->getControlRegisters();
        cout << "Controlregisters: " << endl;
        for (const auto &element : controlRegisters) // const auto &element bedeutet, dass keine Änderungen an element vorgenommen werden koennen
        {
            cout << element << endl;
        }

        // Lege I/O Buffer an
        std::vector<float> inputBuffer;
        inputBuffer.resize(numChannels);
        std::vector<float> outputBuffer;
        outputBuffer.resize(numChannels);

        // 4 virtuelle Sliderwerte
        std::vector<float> sliderValues = {0.25, 0.5, 0.25, 0.1};

        // Startzeitpunkt speichern
        auto startTime = std::chrono::high_resolution_clock::now();

        // Call the process() method to execute the instructions
        // Here we do Stereo processing.
        for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        {
            // Simuliere Sliderinput alle 8 Samples
            if (i % 8 == 0)
            {
                fx8010->setRegisterValue("volume", sliderValues[i / 8]);
            }
            // Nutze dasselbe testSample als Stereoinput
            // inputBuffer[0] = testSample[i]; // AC-Testvalue
            // inputBuffer[1] = testSample[i]; // AC-Testvalue

            inputBuffer[0] = 1.0; // DC-Testvalue
            inputBuffer[1] = 1.0; // DC-Testvalue

            // Hier erfolgt die Berechung
            outputBuffer = fx8010->process(inputBuffer);

            // DSP Output anzeigen
            // NOTE: Anzeige beeinflusst Zeitmessung stark!
            if (DEBUG)
            {
                cout << "Output (0): " << outputBuffer[0] << endl;
                cout << "Output (1): " << outputBuffer[1] << endl;
            }
        }

        if (!(DEBUG || PRINT_REGISTERS))
        {
            // Endzeitpunkt speichern
            auto endTime = std::chrono::high_resolution_clock::now();

            // Berechnen der Differenz zwischen Start- und Endzeitpunkt
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            // Ausgabe der gemessenen Zeit in Mikrosekunden
            std::cout << "Ausfuehrungszeit: " << duration << " Mikrosekunden"
                      << " fuer " << fx8010->getInstructionCounter() << " Instructions pro Audioblock (" << AUDIOBLOCKSIZE << " Samples)." << std::endl;
            cout << "Erlaubtes Zeitfenster ohne Dropouts: " << 1.0 / static_cast<float>(SAMPLERATE) * static_cast<float>(AUDIOBLOCKSIZE) * 1000000.0 << " Mikrosekunden" << endl;
        }

        // Beliebigen Registerwert anzeigen
        // NOTE: Kleinschreibung verlangt, da Parser Sourcecode in Kleinbuchstaben umwandelt. (verbesserungswürdig)
        string testRegister = "c";
        float value = fx8010->getRegisterValue(testRegister);
        cout << "Registerwert fuer '" << testRegister << "': " << value << endl;
    }
    else
    {
        cout << Klangraum::colorMap[Klangraum::COLOR_BLUE] << "Es ist ein Fehler aufgetreten. Abbruch." << Klangraum::colorMap[Klangraum::COLOR_NULL] << endl;
      
        // Zeige Fehlerliste an
        vector<Klangraum::FX8010::MyError> errorList;
        errorList.clear();
        errorList = fx8010->getErrorList();

        cout << "Fehlerliste: " << endl;
        for (const auto &element : errorList) // const auto &element bedeutet, dass keine Änderungen an element vorgenommen werden koennen
        {
            cout << element.errorDescription << " (" << element.errorRow << ")" << endl;
        }
    }
    return 0;
}
