#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <wctype.h>

#include "match.h"

enum {
    MATCH_NOESCAPE = 1,
    MATCH_PATHNAME = 2,
    MATCH_PERIOD = 4,
    MATCH_LEADING_DIR = 8,
    MATCH_CASEFOLD = 16,
};

static wint_t __character(const char** text)
{
    const unsigned char* bytes = (const void*)*text;
    unsigned first = *bytes++;
    ++*text;
    if(first < 0xc2 || first > 0xf4) {
        return first;
    }

    unsigned count = first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
    uint32_t value = first & (0x7f >> count);
    for(unsigned index = 0; index < count; ++index) {
        if((bytes[index] & 0xc0) != 0x80) {
            return first;
        }

        value = (value << 6) | (bytes[index] & 63);
    }

    if(value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        return first;
    }

    if((count == 2 && value < 0x800) || (count == 3 && value < 0x10000)) {
        return first;
    }

    *text += count;
    return value;
}

static wint_t __fold(wint_t value, int flags)
{
    return flags & MATCH_CASEFOLD ? towlower(value) : value;
}

static wint_t __literal(const char** pattern, int flags)
{
    if(**pattern == '\\' && !(flags & MATCH_NOESCAPE) && (*pattern)[1]) {
        ++*pattern;
    }

    return __fold(__character(pattern), flags);
}

static int __class(const char** pattern, wint_t value)
{
    const char* start = *pattern;
    if(start[0] != '[' || start[1] != ':') {
        return -1;
    }

    const char* end = strstr(start + 2, ":]");
    if(!end || end - start > 32) {
        return -1;
    }

    char name[32];
    size_t length = end - start - 2;
    memcpy(name, start + 2, length);
    name[length] = 0;
    wctype_t type = wctype(name);
    *pattern = end + 2;
    return type && iswctype(value, type);
}

static int __bracket(const char** pattern, wint_t value, int flags)
{
    const char* next = *pattern + 1;
    bool invert = *next == '!' || *next == '^';
    if(invert) {
        ++next;
    }

    bool first = true;
    bool matched = false;
    while(*next && (*next != ']' || first)) {
        first = false;
        int result = __class(&next, value);
        if(result >= 0) {
            matched |= result != 0;
            continue;
        }

        wint_t low = __literal(&next, flags);
        if(*next != '-' || !next[1] || next[1] == ']') {
            matched |= low == value;
            continue;
        }

        ++next;
        wint_t high = __literal(&next, flags);
        matched |= low <= value && value <= high;
    }

    if(*next != ']') {
        return -1;
    }

    *pattern = next + 1;
    return matched != invert;
}

static bool __unit(const char** pattern, const char* text, int flags)
{
    if(**pattern == '\\' && !(*pattern)[1] && !(flags & MATCH_NOESCAPE)) {
        return false;
    }

    if((flags & MATCH_PATHNAME) && *text == '/' && **pattern != '/') {
        return false;
    }

    wint_t value = __fold(__character(&text), flags);
    switch(**pattern) {
        case '?':
            ++*pattern;
            return true;

        case '[': {
            int result = __bracket(pattern, value, flags);
            if(result >= 0) {
                return result != 0;
            }

            break;
        }
    }

    return __literal(pattern, flags) == value;
}

int bridge_fnmatch(const char* pattern, const char* text, int flags)
{
    if(!pattern || !text || (flags & ~31)) {
        return 1;
    }

    const char* start = text;
    const char* star = NULL;
    const char* retry = NULL;
    for(;;) {
        bool leading = text == start || ((flags & MATCH_PATHNAME) && text[-1] == '/');
        if(*text == '.' && leading && (flags & MATCH_PERIOD)) {
            const char* literal = pattern;
            if(*literal == '\\' && !(flags & MATCH_NOESCAPE)) {
                ++literal;
            }

            if(*literal != '.') {
                return 1;
            }
        }

        if(*pattern == '*') {
            while(*pattern == '*') {
                ++pattern;
            }

            star = pattern;
            retry = text;
            continue;
        }

        if(!*pattern) {
            if(!*text || ((flags & MATCH_LEADING_DIR) && *text == '/')) {
                return 0;
            }
        } else if(*text && __unit(&pattern, text, flags)) {
            __character(&text);
            continue;
        }

        if(!star || !*retry || ((flags & MATCH_PATHNAME) && *retry == '/')) {
            return 1;
        }

        __character(&retry);
        text = retry;
        pattern = star;
    }
}
