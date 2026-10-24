#define _GNU_SOURCE
#include <fnmatch.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc/match.h>

int main(void)
{
    const char* tokens[] = {"a", "b", "/", ".", "*", "?", "[ab]", "[!a]", "[a-b]", "\\*", "\\?", "[[:digit:]]"};
    const char* letters = "ab/.*?1";
    unsigned errors = 0;
    setlocale(LC_CTYPE, "C.UTF-8");
    srand(123);
    for(unsigned i = 0; i < 100000; ++i) {
        char pattern[128] = {0}, text[16] = {0};
        unsigned count = rand() % 7;
        for(unsigned j = 0; j < count; ++j) strcat(pattern, tokens[rand() % 12]);
        count = rand() % 9;
        for(unsigned j = 0; j < count; ++j) text[j] = letters[rand() % 7];
        int flags = rand() % 32;
        int native = (flags & ~3) | ((flags & 1) << 1) | ((flags & 2) >> 1);
        int expected = fnmatch(pattern, text, native) == 0;
        int actual = bridge_fnmatch(pattern, text, flags) == 0;
        if(expected != actual) {
            if(errors < 10) printf("%s | %s | flags=%d expected=%d actual=%d\n", pattern, text, flags, expected, actual);
            ++errors;
        }
    }
    printf("fnmatch: %u mismatches in 100000 cases\n", errors);
    return errors != 0;
}
