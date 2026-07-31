#include <string.h>

#include "descriptor.h"

static bool __parse_type(const char** cursor, char* type, bool allow_void)
{
    const char* text = *cursor;
    size_t dimensions = 0;
    while(*text == '[') {
        ++dimensions;
        ++text;
    }

    if(dimensions > 255 || !*text) {
        return false;
    }

    *type = *text++;
    switch(*type) {
        case 'L': {
            const char* name = text;
            while(*text && *text != ';') {
                if(strchr(".([)", *text)) {
                    return false;
                }

                ++text;
            }

            if(text == name || *text != ';') {
                return false;
            }

            ++text;
            break;
        }

        case 'V':
            if(!allow_void) {
                return false;
            }

            if(dimensions) {
                return false;
            }

            break;

        default:
            if(!strchr("ZBCSIJFD", *type)) {
                return false;
            }

            break;
    }

    if(dimensions) {
        *type = 'L';
    }

    *cursor = text;
    return true;
}

bool jni_sig_parse(const char* text, jni_sig_t* sig)
{
    if(!text) {
        return false;
    }

    if(!sig) {
        return false;
    }

    if(*text != '(') {
        return false;
    }

    memset(sig, 0, sizeof(*sig));
    ++text;
    while(*text != ')') {
        if(sig->argc == JNI_MAX_CALL_ARGS) {
            return false;
        }

        char* argument = &sig->args[sig->argc];
        if(!__parse_type(&text, argument, false)) {
            return false;
        }

        ++sig->argc;
        sig->slots += *argument == 'J' || *argument == 'D' ? 2 : 1;
        if(sig->slots > 255) {
            return false;
        }
    }

    ++text;
    if(!__parse_type(&text, &sig->ret, true)) {
        return false;
    }

    return *text == 0;
}

bool jni_type_parse(const char* text, char* type)
{
    if(!text) {
        return false;
    }

    if(!type) {
        return false;
    }

    if(!__parse_type(&text, type, false)) {
        return false;
    }

    return *text == 0;
}
