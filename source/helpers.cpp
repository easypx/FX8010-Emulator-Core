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

    // Wandle alle Elemente in Kleinbuchstaben um
    // NOTE: tolower() ist Lambda Funktion in transform()
    /*	std::transform(lines.begin(), lines.end(), lines.begin(), [](std::string str)
                       {
    for (char& c : str) {
        c = std::tolower(c);
    }
    return str; });

    float g_fScale = 2.0f / 0xffffffff;
int g_x1 = 0x67452301;
int g_x2 = 0xefcdab89;

void whitenoise(
  float* _fpDstBuffer, // Pointer to buffer
  unsigned int _uiBufferSize, // Size of buffer
  float _fLevel ) // Noiselevel (0.0 ... 1.0)
{
  _fLevel *= g_fScale;

  while( _uiBufferSize-- )
  {
    g_x1 ^= g_x2;
    *_fpDstBuffer++ = g_x2 * _fLevel;
    g_x2 += g_x1;
  }
}
*/

} // namespace Klangraum
