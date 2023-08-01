// Copyright 2023 Klangraum

#include "../include/helpers.h"

namespace Klangraum
{

    // Funktion zum Entfernen führender und nachfolgender Leerzeichen
    std::string trim(const std::string &str)
    {
        size_t first = str.find_first_not_of(" ");
        if (first == std::string::npos)
        {
            return ""; // Kein nicht-leerzeichen Zeichen gefunden
        }

        size_t last = str.find_last_not_of(" ");
        return str.substr(first, last - first + 1);
    }

    bool isNumber(const std::string &input)
    {
        // Regulärer Ausdruck für eine Zahl mit optionaler Dezimalstelle
        std::regex numberRegex("^-?\\d+(\\.\\d+)?$");

        return std::regex_match(input, numberRegex);
    }

    // https://stackoverflow.com/questions/66206815/how-to-change-the-ouput-color-in-the-terminal-of-visual-code
    std::map<colorCodes, std::string> colorMap = {
        {COLOR_RED, "\033[31m"},
        {COLOR_GREEN, "\033[32m"},
        {COLOR_YELLOW, "\033[33m"},
        {COLOR_BLUE, "\033[34m"},
        {COLOR_DEFAULT, "\033[38m"},
        {COLOR_NULL, "\033[0m"}};

    void printLine(int count)
    {
        std::string line(count, '-');
        std::cout << line << std::endl;
    }

    // Funktion zum Umwandeln von 32-Bit-Integer in Float
    float intToFloat(int32_t intValue)
    {
        // 32-Bit-Integer in 32-Bit-Float umwandeln
        return static_cast<float>(intValue) / static_cast<float>(INT32_MAX);
    }

    // Funktion zum Umwandeln von Float in 32-Bit-Integer
    int32_t floatToInt(float floatValue)
    {
        // Float in 32-Bit-Integer umwandeln
        return static_cast<int32_t>(floatValue * static_cast<float>(INT32_MAX));
    }

} // namespace Klangraum
