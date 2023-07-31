// Copyright 2023 Klangraum

#include "../include/FX8010.h"
#include "../include/helpers.h"

using namespace Klangraum;

#define SINOID_TEST 0
#define BIPOLAR_RAMP_TEST 1
#define UNIPOLAR_RAMP_TEST 0
#define DC_TEST 0
#define DIRAC_TEST 0
#define SLIDER_TEST 0

int main()
{
    // Anzahl der Channels fuer Stereo
    int numChannels = 1;

    // Erzeugung eines std::unique_ptr für die Instanz
    std::unique_ptr<Klangraum::FX8010> fx8010 = std::make_unique<Klangraum::FX8010>(numChannels);

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

        // Erzeugung des Vektors mit 32 float-Werten
        std::vector<float> sinoid;

        // Füllen des Vektors mit einer Sinuswelle von -1.0 bis 1.0 Amplitude
        for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        {
            float phase = static_cast<float>(i) / (AUDIOBLOCKSIZE - 1) * 2 * PI; // Phasenwert von 0 bis 2*pi
            float value = std::sin(phase);                                       // Wertebereich von -1.0 bis 1.0
            sinoid.push_back(value);
        }

        // Ausgabe des Vektors
        // for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        // {
        //     std::cout << sinoid[i] << std::endl;
        // }

        // Vector mit Bipolar-Ramp erzeugen
        std::vector<float> bipolarRamp;

        // linearen Sampledaten -1.0 bis 1.0
        for (int i = -(AUDIOBLOCKSIZE) / 2.0; i < (AUDIOBLOCKSIZE / 2.0); i++)
        {
            float value = static_cast<float>(i) / (AUDIOBLOCKSIZE / 2.0); // Wertebereich von 0 bis 1.0
            bipolarRamp.push_back(value);
        }

        // Ausgabe des Testsamples
        // for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        // {
        //     cout << bipolarRamp[i] << ", ";
        // }
        // cout << endl;

        // Vector mit Unipolar-Ramp erzeugen
        std::vector<float> unipolarRamp;

        // linearen Sampledaten 0 bis 1.0
        for (int i = 0; i < AUDIOBLOCKSIZE; i++)
        {
            float value = static_cast<float>(i) / AUDIOBLOCKSIZE; // Wertebereich von 0 bis 1.0
            unipolarRamp.push_back(value);
        }

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
        std::vector<float> sliderValues = {0.1, 0.25, 0.5, 1.0};

        // Vergleich mit Null mit einer kleinen Toleranz
        double tolerance = 1e-4;

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
                    // Hier wird das Label zum DSP Control-Typ bzw. Slider genutzt, um Register im DSP zu aendern
                    fx8010->setRegisterValue("filter_cutoff", sliderValues[i / 8]);
                }
            }

            // AC-Testsample SINOID
            //----------------------------------------------------------------
            if (SINOID_TEST)
            {
                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = sinoid[i]; // AC-Testsample
                }
            }

            // AC-Testsample BIPOLAR_RAMP zum Testen von LOG, EXP
            //----------------------------------------------------------------
            else if (BIPOLAR_RAMP_TEST)
            {

                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = bipolarRamp[i]/2.0; // AC-Testsample
                }
            }

            // AC-Testsample UNIPOLAR_RAMP zum Testen von INTERP
            //----------------------------------------------------------------
            else if (UNIPOLAR_RAMP_TEST)
            {

                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = unipolarRamp[i]; // AC-Testsample
                }
            }

            // DC-Testvalue zum Testen des virtuellen Sliderinputs
            //----------------------------------------------------------------
            else if (DC_TEST)
            {

                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = 1.0; // DC-Testvalue
                }
            }

            // Dirac-Impuls zum Testen der Delayline
            //----------------------------------------------------------------
            else if (DIRAC_TEST)
            {

                for (int j = 0; j < numChannels; j++)
                {
                    inputBuffer[j] = diracImpulse[i]; // Dirac Impuls
                }
            }

            // Hier erfolgt die Berechung
            outputBuffer = fx8010->process(inputBuffer);

            // DSP Output anzeigen
            // NOTE: Anzeige beeinflusst Zeitmessung stark! Fuer aussagekraeftige Zeitmessung DEBUG und PRINT_REGISTERS auf 0 setzen!
            if (DEBUG)
            {
                // CSV Output, kann direkt in https://www.desmos.com/ genutzt werden
                // NOTE: Desmos zeigt sehr kleine Werte falsch an!
                // Deshalb runden wir auf 0, wenn tolerance = 1e-4 unterschritten wird!
                cout << ((std::abs(bipolarRamp[i]) < tolerance) ? 0.0 : bipolarRamp[i]) << "," << ((std::abs(outputBuffer[0]) < tolerance) ? 0.0 : outputBuffer[0]) << endl;
                //cout << ((std::abs(inputBuffer[0]) < tolerance) ? 0.0 : inputBuffer[0]) << "," << ((std::abs(outputBuffer[0]) < tolerance) ? 0.0 : outputBuffer[0]) << endl;
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
        // NOTE: Hier wird (noch) Kleinschreibung verlangt, da Parser Sourcecode in Kleinbuchstaben umwandelt. (verbesserungswürdig)
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
