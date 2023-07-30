// Copyright 2023 Klangraum

#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <regex>
#include <map>
#include <iostream>

namespace Klangraum
{
    
    // TODO: Helper-Klasse, Kapselung verhindert Doppeldefinitionen

    // Funktion zum Entfernen führender und nachfolgender Leerzeichen
    std::string trim(const std::string &str);

    bool isNumber(const std::string &input);

    // Farben fuer Konsole
    // Benutzung: cout << colorMap[COLOR_RED] << "Text" << colorMap[COLOR_NULL];
    enum colorCodes
    {
        COLOR_RED,
        COLOR_GREEN,
        COLOR_YELLOW,
        COLOR_BLUE,
        COLOR_DEFAULT,
        COLOR_NULL // Restore?
    };

    // Externe Deklaration der globalen Variable
    extern std::map<enum colorCodes, std::string> colorMap;

    void printLine(int count);

} // namespace Klangraum

#endif // HELPERS_H