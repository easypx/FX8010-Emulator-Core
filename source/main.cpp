// TODO:
// getErrorList(), um Fehler im VST anzuzeigen
// Flags für spezielle originalgeteue/vernachlässigbare Funktionalität z.B. TEMP GPR löschen, setCCR(), als neue Deklaration/Metadaten?
// FLAG NO_TEMP_ERASE, NO_SETCCR
// Präprozessor-Anweisungen für Debug/Release (if(DEBUG_LEVEL) entfernen) im kritischen process()
// Soll Syntaxcheck/Loadfile ErrorList zurückgeben?
// const in allen Methodenargumenten verwenden, damit nicht zufällig Änderungen stattfinden
// Testsample -1.0 bis 1.0
// LOG, EXP Tables
// Soll CCR, (READ, WRITE, AT) normale GPR sein? Damit liesse sich der Wert mit printRow() anzeigen und die Syntaxcheck vereinfachen.

#include "../include/FX8010.h"
#include "../include/helpers.h"

//using namespace Klangraum;

int main()
{
    // Create an instance of FX8010 class
    Klangraum::FX8010 *fx8010 = new Klangraum::FX8010();

    // Vector mit linearen Sampledaten. Nur zum Testen!
    vector<vector<float>> testSample;

    // Vector mit Testsamples erzeugen
    int numSamples = AUDIOBLOCKSIZE;
    int numChannels = 1;

    // Initialisiere testSample für jeden Kanal
    testSample.resize(numChannels);

    // Channel 0: 0 bis 1.0
    // Channel 1: 0 bis 0.5
    for (int j = 0; j < numChannels; j++)
    {
        for (int i = 0; i < numSamples; i++)
        {
            float value = static_cast<float>(i) / (numSamples - 1); // Wertebereich von 0 bis 1.0
            testSample[j].push_back(value / ((float)j + 1));
        }
    }

    // Ausgabe des Testsamples
    /*for (int j = 0; j < numChannels; j++)
{
    for (int i = 0; i < numSamples; i++)
    {
        cout << testSample[j][i] << endl;
    }
}*/

    if (fx8010->loadFile("testcode.da"))
    {
        // Startzeitpunkt speichern
        auto startTime = std::chrono::high_resolution_clock::now();

        // Call the process() method to execute the instructions
        // Here we do Stereo processing.
        float output = 0;
        for (int j = 0; j < numChannels; j++)
        {
            for (int i = 0; i < AUDIOBLOCKSIZE; i++)
            {
                output = fx8010->process(testSample[j][i]);
                // cout << "Output: " << output << endl;
            }
        }

        // Endzeitpunkt speichern
        auto endTime = std::chrono::high_resolution_clock::now();

        // Berechnen der Differenz zwischen Start- und Endzeitpunkt
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

        // Ausgabe der gemessenen Zeit in Mikrosekunden
        std::cout << "Ausfuehrungszeit: " << duration << " Mikrosekunden"
                  << " fuer " << fx8010->getInstructionCounter() << " Instructions pro Audioblock (" << AUDIOBLOCKSIZE << " Samples)." << std::endl;
        cout << "Erlaubtes Zeitfenster ohne Dropouts: " << 1.0 / (float)SAMPLERATE * (float)AUDIOBLOCKSIZE * 1000000.0 << " Mikrosekunden" << endl;
    }
    else
    {
        cout << Klangraum::colorMap[Klangraum::COLOR_BLUE] << "Es ist ein Fehler aufgetreten. Abbruch." << Klangraum::colorMap[Klangraum::COLOR_NULL] << endl;
    }
    return 0;
}