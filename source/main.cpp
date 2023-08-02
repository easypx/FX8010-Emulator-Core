// Copyright 2023 Klangraum

#include "../include/FX8010.h"
#include "../include/helpers.h"

using namespace Klangraum;

#define SLIDER_TEST 1

int main()
{
    // Anzahl der Channels fuer Stereo
    int numChannels = 1;

    // Pointer auf neue Instanz von FX8010
    Klangraum::FX8010 *fx8010 = new Klangraum::FX8010(numChannels);

    // AC-Testsample SINOID
    //----------------------------------------------------------------

    // Erzeugung des Vektors
    std::vector<float> sinoid;

    // Füllen des Vektors mit einer Sinuswelle von -1.0 bis 1.0 Amplitude
    for (int i = 0; i < AUDIOBLOCKSIZE; i++)
    {
        float phase = static_cast<float>(i) / (AUDIOBLOCKSIZE - 1) * 2 * PI; // Phasenwert von 0 bis 2*pi
        float value = std::sin(phase);                                       // Wertebereich von -1.0 bis 1.0
        sinoid.push_back(value);
    }

    // AC-Testsample BIPOLAR_RAMP zum Testen von LOG, EXP
    //----------------------------------------------------------------

    // Vector mit Bipolar-Ramp erzeugen
    std::vector<float> bipolarRamp;

    // linearen Sampledaten -1.0 bis 1.0
    for (int i = -(AUDIOBLOCKSIZE) / 2.0; i < (AUDIOBLOCKSIZE / 2.0); i++)
    {
        float value = static_cast<float>(i) / (AUDIOBLOCKSIZE / 2.0); // Wertebereich von 0 bis 1.0
        bipolarRamp.push_back(value);
    }

    // AC-Testsample UNIPOLAR_RAMP zum Testen von INTERP
    //----------------------------------------------------------------

    // Vector mit Unipolar-Ramp erzeugen
    std::vector<float> unipolarRamp;

    // linearen Sampledaten 0 bis 1.0
    for (int i = 0; i < AUDIOBLOCKSIZE; i++)
    {
        float value = static_cast<float>(i) / AUDIOBLOCKSIZE; // Wertebereich von 0 bis 1.0
        unipolarRamp.push_back(value);
    }

    // Dirac-Impuls zum Testen der Delayline
    //----------------------------------------------------------------

    // Dirac Impuls zum Testen der Delayline
    float diracImpulse[AUDIOBLOCKSIZE];
    diracImpulse[0] = 1.0f; // 1. Element ist Dirac-Impuls

    for (int i = 1; i < AUDIOBLOCKSIZE; i++)
    {
        diracImpulse[i] = 0.0f;
    }

    // Lege I/O Buffer an
    // Diese enthalten einzelne Samples fuer mehrere Kanäle! process() wird 1x je Sample aufgerufen.
    std::vector<float> inputBuffer;
    inputBuffer.resize(numChannels, 0.0);
    std::vector<float> outputBuffer;
    outputBuffer.resize(numChannels, 0.0);

    // 4 virtuelle Sliderwerte
    std::vector<float> sliderValues = {0.1, 0.25, 0.5, 1.0};

    // Vergleich mit Null mit einer kleinen Toleranz
    double tolerance = 1e-4;
    // DC-Testvalue zum Testen des virtuellen Sliderinputs
    //----------------------------------------------------------------
    // Simply put a static value into inputBuffer[j] = 1.0

    // Sourcecode Laden und Parsen
    if (fx8010->loadFile("testcode.da"))
    {
        // Vorbereitung der CSV Ausgabe
        if (DEBUG)
        {
            cout << "CSV Output kann direkt in https://www.desmos.com/ genutzt werden:" << endl;
            cout << endl;
        }

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
                    fx8010->setRegisterValue("volume", sliderValues[i / 8]);
                }
            }

            for (int j = 0; j < numChannels; j++)
            {
                inputBuffer[j] = bipolarRamp[i];
            }

            // Hier erfolgt die Berechnung
            outputBuffer = fx8010->process(inputBuffer);

            // DSP Output anzeigen
            // NOTE: Anzeige beeinflusst Zeitmessung stark! Fuer aussagekraeftige Zeitmessung DEBUG und PRINT_REGISTERS auf 0 setzen!
            if (DEBUG)
            {
                // CSV Output, kann direkt in https://www.desmos.com/ genutzt werden
                // NOTE: Desmos zeigt sehr kleine Werte falsch an!
                // Deshalb runden wir auf 0, wenn tolerance = 1e-4 unterschritten wird!

                cout << ((std::abs(bipolarRamp[i]) < tolerance) ? 0.0 : bipolarRamp[i]) << "," << ((std::abs(outputBuffer[0]) < tolerance) ? 0.0 : outputBuffer[0]) << endl;

                // cout << ((std::abs(inputBuffer[0]) < tolerance) ? 0.0 : inputBuffer[0]) << "," << ((std::abs(outputBuffer[0]) < tolerance) ? 0.0 : outputBuffer[0]) << endl;

                // std::cout << "(" << ((std::abs(bipolarRamp[i]) < tolerance) ? 0.0 : bipolarRamp[i]) << " , " << ((std::abs(outputBuffer[0]) < tolerance) ? 0.0 : outputBuffer[0]) << ")" << std::endl; // CVS Daten in Console (als punktfolge für https://www.desmos.com/)

                // data.push_back({ std::to_string(testSample[i]) , std::to_string(R) }); // CVS Daten in Vector zum speichern
            }
        }

        std::cout << endl;

        // if (!(DEBUG || PRINT_REGISTERS))
        //{
        // Endzeitpunkt speichern
        auto endTime = std::chrono::high_resolution_clock::now();

        // Berechnen der Differenz zwischen Start- und Endzeitpunkt
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

        // Ausgabe der gemessenen Zeit in Mikrosekunden
        std::cout << "Ausfuehrungszeit: " << duration << " Mikrosekunden"
                  << " fuer " << fx8010->getInstructionCounter() << " Instructions pro Audioblock (" << AUDIOBLOCKSIZE << " Samples)." << std::endl;
        cout << "Erlaubtes Zeitfenster ohne Dropouts: " << 1.0 / static_cast<float>(SAMPLERATE) * static_cast<float>(AUDIOBLOCKSIZE) * 1000000.0 << " Mikrosekunden" << endl;
        if (DEBUG || PRINT_REGISTERS)
            cout << "ACHTUNG: Diese Werte sind viel groesser als die tatsaechliche Ausfuehrungszeit, da DEBUG oder PRINT_REGISTERS 1 sind!" << endl;
        //}

        std::cout << endl;

        // Beliebigen Registerwert anzeigen
        // NOTE: Hier wird (noch) Kleinschreibung verlangt, da Parser Sourcecode in Kleinbuchstaben umwandelt. (verbesserungswürdig)
        string testRegister = "filter_cutoff";
        float value = fx8010->getRegisterValue(testRegister);
        cout << "Registerwert fuer '" << testRegister << "': " << value << endl;

        std::cout << endl;

        // Ausgabe Metadaten
        cout << "Metadaten: " << endl;
        for (const auto &pair : fx8010->getMetaData())
        {
            std::cout << pair.first << ": " << pair.second << std::endl;
        }
        cout << endl;
        // Diese Liste mit Controlregistern wird VST als Label-Identifizierer fuer Slider, Knobs, usw. nutzen koennen.
        cout << "Controlregisters: " << endl;
        for (const auto &element : fx8010->getControlRegisters()) // const auto &element bedeutet, dass keine Änderungen an element vorgenommen werden koennen
        {
            cout << element << endl;
        }
    }
    else
    {
        cout << Klangraum::colorMap[Klangraum::COLOR_BLUE] << "Es ist ein Fehler aufgetreten. Abbruch." << Klangraum::colorMap[Klangraum::COLOR_NULL] << endl;

        // Zeige Fehlerliste an
        cout << "Fehlerliste: " << endl;
        for (const auto &element : fx8010->getErrorList()) // const auto &element bedeutet, dass keine Änderungen an element vorgenommen werden koennen
        {
            cout << element.errorDescription << " (" << element.errorRow << ")" << endl;
        }
    }

cleanUp:
    delete fx8010;

    return 0;
}
