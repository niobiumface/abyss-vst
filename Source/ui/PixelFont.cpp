#include "PixelFont.h"

#include <cstring>

namespace abyss
{

namespace
{
void setRows (char rows[7][6], const char* a, const char* b, const char* c,
              const char* d, const char* e, const char* f, const char* g)
{
    const char* src[7] = { a, b, c, d, e, f, g };
    for (int y = 0; y < 7; ++y)
    {
        for (int x = 0; x < 5; ++x)
            rows[y][x] = src[y][x];
        rows[y][5] = 0;
    }
}
}

int pixelTextWidth (juce::StringRef text) noexcept
{
    const int n = text.length();
    return n <= 0 ? 0 : n * 6 - 1;
}

bool pixelGlyph (juce::juce_wchar ch, char rows[7][6]) noexcept
{
    const char blank[6] = "     ";
    auto clear = [&] { setRows (rows, blank, blank, blank, blank, blank, blank, blank); };

    switch (ch)
    {
        case ' ': clear(); return true;
        case 'A': setRows (rows, " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"); return true;
        case 'B': setRows (rows, "#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "); return true;
        case 'C': setRows (rows, " ####", "#    ", "#    ", "#    ", "#    ", "#    ", " ####"); return true;
        case 'D': setRows (rows, "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### "); return true;
        case 'E': setRows (rows, "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"); return true;
        case 'F': setRows (rows, "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "); return true;
        case 'G': setRows (rows, " ####", "#    ", "#    ", "#  ##", "#   #", "#   #", " ### "); return true;
        case 'H': setRows (rows, "#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"); return true;
        case 'I': setRows (rows, "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####"); return true;
        case 'K': setRows (rows, "#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"); return true;
        case 'L': setRows (rows, "#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"); return true;
        case 'M': setRows (rows, "#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #"); return true;
        case 'N': setRows (rows, "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"); return true;
        case 'O': setRows (rows, " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "); return true;
        case 'P': setRows (rows, "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "); return true;
        case 'R': setRows (rows, "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"); return true;
        case 'S': setRows (rows, " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "); return true;
        case 'T': setRows (rows, "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "); return true;
        case 'U': setRows (rows, "#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "); return true;
        case 'V': setRows (rows, "#   #", "#   #", "#   #", "#   #", " # # ", " # # ", "  #  "); return true;
        case 'W': setRows (rows, "#   #", "#   #", "#   #", "#   #", "# # #", "## ##", "#   #"); return true;
        case 'X': setRows (rows, "#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"); return true;
        case 'Y': setRows (rows, "#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "); return true;
        case '\'': setRows (rows, "  #  ", "  #  ", " #   ", "     ", "     ", "     ", "     "); return true;
        case '%': setRows (rows, "##  #", "## # ", "  #  ", "  #  ", " #   ", "# ## ", "#  ##"); return true;
        case '-': setRows (rows, "     ", "     ", "     ", "#####", "     ", "     ", "     "); return true;
        case '+': setRows (rows, "     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     "); return true;
        case '.': setRows (rows, "     ", "     ", "     ", "     ", "     ", " ##  ", " ##  "); return true;
        case ':': setRows (rows, "     ", " ##  ", " ##  ", "     ", " ##  ", " ##  ", "     "); return true;
        case '/': setRows (rows, "    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    "); return true;
        case '!': setRows (rows, "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "     ", "  #  "); return true;
        case '?': setRows (rows, " ### ", "#   #", "    #", "   # ", "  #  ", "     ", "  #  "); return true;
        case '0': setRows (rows, " ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "); return true;
        case '1': setRows (rows, "  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "); return true;
        case '2': setRows (rows, " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"); return true;
        case '3': setRows (rows, "#### ", "    #", "    #", " ### ", "    #", "    #", "#### "); return true;
        case '4': setRows (rows, "#   #", "#   #", "#   #", "#####", "    #", "    #", "    #"); return true;
        case '5': setRows (rows, "#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "); return true;
        case '6': setRows (rows, " ### ", "#    ", "#    ", "#### ", "#   #", "#   #", " ### "); return true;
        case '7': setRows (rows, "#####", "    #", "   # ", "  #  ", "  #  ", "  #  ", "  #  "); return true;
        case '8': setRows (rows, " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "); return true;
        case '9': setRows (rows, " ### ", "#   #", "#   #", " ####", "    #", "    #", " ### "); return true;
        case 'b': setRows (rows, "#    ", "#    ", "###  ", "#  # ", "#  # ", "###  ", "     "); return true;
        case 'y': setRows (rows, "     ", "     ", "#   #", " # # ", "  #  ", " #   ", "#    "); return true;
        case 'n': setRows (rows, "     ", "     ", "## # ", "#  # ", "#  # ", "#   #", "     "); return true;
        case 'f': setRows (rows, "  ## ", " #   ", "#### ", " #   ", " #   ", " #   ", "     "); return true;
        case 'p': setRows (rows, "     ", "     ", "###  ", "#  # ", "###  ", "#    ", "#    "); return true;
        case 'r': setRows (rows, "     ", "     ", "# ## ", "##   ", "#    ", "#    ", "     "); return true;
        case 'o': setRows (rows, "     ", "     ", " ### ", "#   #", "#   #", "#   #", " ### "); return true;
        default: clear(); return false;
    }
}

juce::Image renderPixelText (juce::StringRef text, juce::Colour colour)
{
    const int width = std::max (1, pixelTextWidth (text));
    juce::Image image (juce::Image::ARGB, width, 7, true);
    juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

    int pen = 0;
    for (int i = 0; i < text.length(); ++i)
    {
        char rows[7][6];
        pixelGlyph (text[i], rows);
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 5; ++x)
                if (rows[y][x] == '#')
                    data.setPixelColour (pen + x, y, colour);
        pen += 6;
    }
    return image;
}

} // namespace abyss
