// Copyright 2023 Klangraum

#include "../include/FX8010.h"
#include "../include/helpers.h"

// using namespace Klangraum;

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
            testSample[j].push_back(value / (static_cast<float>(j + 1)));
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
                  << " fuer " << fx8010->getInstructionCounter() << " Instructions pro Audioblock (" <<
                  AUDIOBLOCKSIZE << " Samples)." << std::endl;
        cout << "Erlaubtes Zeitfenster ohne Dropouts: " << 1.0 / static_cast<float>(SAMPLERATE) *
        static_cast<float>(AUDIOBLOCKSIZE) * 1000000.0 << " Mikrosekunden" << endl;
    }
    else
    {
        cout << Klangraum::colorMap[Klangraum::COLOR_BLUE] << "Es ist ein Fehler aufgetreten. Abbruch." <<
        Klangraum::colorMap[Klangraum::COLOR_NULL] << endl;
    }
    return 0;
}
