// Fuzzes profanity's command-line tokenizer (src/tools/parser.c): the code that
// splits every line typed at the prompt into command arguments.
//
// Input layout: byte 0 selects min args (0-3), byte 1 max args (0-7), byte 2 the
// entry point; the rest is the input line.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "tools/parser.h"

static gchar* option_keys[] = { "-a", "-b", "name", "jid", "port", "server", NULL };

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 3) {
        return -1;
    }

    int min = data[0] % 4;
    int max = data[1] % 8;
    int mode = data[2] % 5;

    gchar* input = g_strndup((const gchar*)data + 3, size - 3);

    // parser.c walks the line with g_utf8_get_char()/g_utf8_offset_to_pointer(),
    // which glib documents as requiring valid UTF-8; the prompt delivers UTF-8.
    if (!g_utf8_validate(input, -1, NULL)) {
        g_free(input);
        return -1;
    }

    gboolean result = FALSE;
    switch (mode) {
    case 0: {
        gchar** args = parse_args(input, min, max, &result);
        g_strfreev(args);
        break;
    }
    case 1: {
        gchar** args = parse_args_with_freetext(input, min, max, &result);
        g_strfreev(args);
        break;
    }
    case 2: {
        gchar** args = parse_args_as_one(input, min, max, &result);
        g_strfreev(args);
        break;
    }
    case 3: {
        count_tokens(input);
        gchar* start = get_start(input, min);
        g_free(start);
        break;
    }
    default: {
        gchar** args = parse_args(input, 0, 16, &result);
        if (args) {
            gboolean ok = FALSE;
            GHashTable* options = parse_options(args, option_keys, &ok);
            if (options) {
                options_destroy(options);
            }
            g_strfreev(args);
        }
        break;
    }
    }

    g_free(input);
    return 0;
}
