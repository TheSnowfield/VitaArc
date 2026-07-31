#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <opengl/shader_binary.h>

static void put(unsigned char* bytes, unsigned value)
{
    for(unsigned index = 0; index < 4; ++index) {
        bytes[index] = (unsigned char)(value >> (8 * index));
    }
}

static void check_envelope(void)
{
    unsigned char bytes[224] = {0};

    unsigned char* gxp = bytes + 8;
    put(bytes, 1);
    put(bytes + 4, 0);
    put(gxp, 0x00505847);
    put(gxp + 8, 216);
    put(gxp + 0x24, 1);
    put(gxp + 0x28, 156 - 0x28);
    put(gxp + 156, 16);
    gxp[160] = 1; /* Uniform parameter. */
    memcpy(gxp + 172, "CC_MVPMatrix", 13);
    assert(bridge_shader_binary_validate(bytes, sizeof(bytes), 0x8b31));
    assert(!bridge_shader_binary_validate(bytes, sizeof(bytes), 0x8b30));
    assert(!bridge_shader_binary_validate(bytes, sizeof(bytes), 0));
    assert(!bridge_shader_binary_validate(NULL, sizeof(bytes), 0x8b31));
    for(size_t size = 0; size < sizeof(bytes); ++size) {
        assert(!bridge_shader_binary_validate(bytes, size, 0x8b31));
    }

    struct fault {
        size_t offset;
        unsigned value;
    } faults[] = {{0, 0xffffffff},
        {4, 1},
        {8, 0},
        {8 + 8, 0xffffffff},
        {8 + 8, 155},
        {8 + 8, 212},
        {8 + 20, 1},
        {8 + 0x24, 0xffffffff},
        {8 + 0x28, 0xffffffff},
        {8 + 156, 0xffffffff},
        {8 + 156, 216 - 156},
        {8 + 160, 0}};

    for(size_t fault = 0; fault < sizeof(faults) / sizeof(faults[0]); ++fault) {
        unsigned char changed[sizeof(bytes)];
        memcpy(changed, bytes, sizeof(bytes));
        put(changed + faults[fault].offset, faults[fault].value);
        assert(!bridge_shader_binary_validate(changed, sizeof(changed), 0x8b31));
    }

    unsigned char no_name[sizeof(bytes)];
    memcpy(no_name, bytes, sizeof(bytes));
    memset(no_name + 8 + 172, 'x', sizeof(bytes) - 8 - 172);
    assert(!bridge_shader_binary_validate(no_name, sizeof(no_name), 0x8b31));
    unsigned char duplicate[sizeof(bytes) + 4];
    put(duplicate, 2);
    put(duplicate + 4, 0);
    put(duplicate + 8, 0);
    memcpy(duplicate + 12, gxp, sizeof(bytes) - 8);
    assert(!bridge_shader_binary_validate(duplicate, sizeof(duplicate), 0x8b31));
    put(gxp + 20, 1);
    assert(bridge_shader_binary_validate(bytes, sizeof(bytes), 0x8b30));
}

int main(int argc, char** argv)
{
    check_envelope();
    for(int index = 1; index < argc; ++index) {
        FILE* file = fopen(argv[index], "rb");
        assert(file);
        assert(fseek(file, 0, SEEK_END) == 0);
        long size = ftell(file);
        assert(size > 0 && size <= 1024 * 1024);
        rewind(file);
        unsigned char* bytes = malloc((size_t)size);
        assert(bytes && fread(bytes, 1, (size_t)size, file) == (size_t)size);
        fclose(file);
        assert(bridge_shader_binary_validate(bytes, (size_t)size, 0x8b31) ||
            bridge_shader_binary_validate(bytes, (size_t)size, 0x8b30));
        free(bytes);
    }

    puts("Shader binary bounds/stage/reflection/matrix rejection checks passed.");
    return 0;
}
