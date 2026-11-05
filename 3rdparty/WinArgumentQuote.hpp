#pragma once
#include <string>

// Quote one argument for CommandLineToArgvW / MSVC argument parsing.
inline std::wstring quoteWindowsArgument(const std::wstring &arg) {
    std::wstring out(1, L'"');
    size_t slashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++slashes;
        } else if (ch == L'"') {
            out.append(slashes * 2 + 1, L'\\');
            out.push_back(ch);
            slashes = 0;
        } else {
            out.append(slashes, L'\\');
            slashes = 0;
            out.push_back(ch);
        }
    }
    out.append(slashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}
