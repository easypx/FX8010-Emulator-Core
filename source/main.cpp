// Copyright 2023 Klangraum

#include "../include/FX8010.h"
#include "../include/helpers.h"

using namespace Klangraum;

#define RAMP_TEST 0
#define DC_TEST 0
#define DIRAC_TEST 1
#define SLIDER_TEST 0

int main()
{
    // Anzahl der Channels fuer Stereo
    int numChannels = 1;

    // Create an instance of FX8010 class
    Klangraum::FX8010 *fx8010 = new Klangraum::FX8010(numChannels);

    // Sourcecode laden und parsen
    if (fx8010->loadFile("testcode.da"))
    {
        // Diese Liste mit Controlregistern wird VST als Label-Identifizierer fuer Slider, Knobs, usw. nutzen koennen.
        vector<string> controlRegistersLabel = fx8010->getControlRegisters();
        cout << "Controlregisters: " << endl;
        for (const auto &element : controlRegistersLabel) // const auto &element bedeutet, dass keine Änderungen an element vorgenommen werden koennen
        {
            cout << element << endl;
        }

        // Vector mit Ramp erzeugen
        std::vector<float> ramp;

        // linearen Sampledaten 0 bis 1.0 bzw. -1.0 bis 1.0
        for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        {
            float value = static_cast<float>(i) / (AUDIOBLOCKSIZE - 1); // Wertebereich von 0 bis 1.0
            ramp.push_back(value);
        }

        // Ausgabe des Testsamples
        // for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        // {
        //     cout << ramp[i] << ", ";
        // }
        // cout << endl;

        // Dirac Impuls zum Testen der Delayline
        // std::vector<float> diracImpulse;
        // diracImpulse.resize(numSamples, 0);
        // float diracImpulse[numSamples];
        float diracImpulse[AUDIOBLOCKSIZE];
        diracImpulse[0] = 1.0f; // 1. Element ist Dirac-Impuls

        for (int i = 1; i < AUDIOBLOCKSIZE; i++)
        {
            diracImpulse[i] = 0.0f;
        }

        // Dirac Impuls ausgeben
        // for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        // {
        //     cout << diracImpulse[i] << ", ";
        // }
        // cout << endl;

        // Lege I/O Buffer an
        std::vector<float> inputBuffer;
        inputBuffer.resize(numChannels, 0.0);
        std::vector<float> outputBuffer;
        outputBuffer.resize(numChannels, 0.0);

        // 4 virtuelle Sliderwerte
        std::vector<float> sliderValues = {0.25, 0.5, 0.25, 0.1};

        // Vergleich mit Null mit einer kleinen Toleranz
        double tolerance = 1e-6;

        // Startzeitpunkt speichern
        auto startTime = std::chrono::high_resolution_clock::now();

        // Call the process() method to execute the instructions
        // Here we do Stereo processing.
        for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        {
            // Simuliere Sliderinput alle 8 Samples
            //----------------------------------------------------------------
            if (SLIDER_TEST)
            {

                if (i % 8 == 0)
                {
                    fx8010->setRegisterValue("volume", sliderValues[i / 8]);
                }
            }

            // Nutze dieselben Sampledaten als Stereoinput
            // AC-Testsample
            //----------------------------------------------------------------
            if (RAMP_TEST)
            {

                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = ramp[i]; // AC-Testsample
                }
            }

            // DC-Testvalue
            //----------------------------------------------------------------
            if (DC_TEST)
            {

                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = 1.0; // DC-Testvalue
                }
            }

            // Dirac-Impuls
            //----------------------------------------------------------------
            if (DIRAC_TEST)
            {

                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = diracImpulse[i]; // Dirac Impuls
                }
            }

            // Hier erfolgt die Berechung
            outputBuffer = fx8010->process(inputBuffer);

            // DSP Output anzeigen
            // NOTE: Anzeige beeinflusst Zeitmessung stark!
            if (DEBUG)
            {
                cout << "Sample: " << std::setw(3) << i << " | "
                     << "Input (0): " << std::setw(12) << ((std::abs(inputBuffer[0]) < tolerance) ? 0.0 : inputBuffer[0]) << " | ";
                cout << "Output (0): " << std::setw(12) << ((std::abs(outputBuffer[0]) < tolerance) ? 0.0 : outputBuffer[0]) << endl;
                // cout << "Input (1): " << ((std::abs(inputBuffer[1]) < tolerance) ? 0.0 : inputBuffer[1]) << " | ";
                // cout << "Output (1): " << ((std::abs(outputBuffer[1]) < tolerance) ? 0.0 : outputBuffer[1]) << endl;
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
cleanUp:
    delete fx8010;
    return 0;
}
