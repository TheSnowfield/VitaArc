#include <stdio.h>
#include <stdlib.h>
#include <logcat/text.h>
#include <logcat/logo.h>

int main(void)
{
    log_text_t text = {0};
    log_text_image(&text, log_logo_open());
    char line[1024];
    while(fgets(line, sizeof(line), stdin)) {
        log_text_write(&text, INFORMATION, line, strlen(line));
    }

    uint32_t* pixels = calloc(960 * 544, sizeof(*pixels));
    if(!pixels) {
        return 1;
    }

    log_text_draw(&text, pixels, 960, 544, 960);
    fwrite(pixels, sizeof(*pixels), 960 * 544, stdout);
    free(pixels);
    log_logo_close();
    return 0;
}
