// Fuzzes the tab-completion engine (src/tools/autocomplete.c): item list maintenance, prefix search
// with escaped/quoted input, and the `/command <param>` completion wrappers.
//
// Input layout: byte 0 holds flags; the rest is UTF-8 text whose '\n'-separated lines are the items,
// the last line being the text being completed.
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "tools/autocomplete.h"

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return -1;
    }

    gboolean quote = (data[0] & 1) != 0;
    gboolean previous = (data[0] & 2) != 0;
    gboolean unsorted = (data[0] & 4) != 0;
    gboolean reversed = (data[0] & 8) != 0;

    gchar* text = g_strndup((const gchar*)data + 1, size - 1);
    if (!g_utf8_validate(text, -1, NULL)) {
        g_free(text);
        return -1;
    }

    gchar** lines = g_strsplit(text, "\n", 0);
    guint count = g_strv_length(lines);
    const gchar* search = count > 0 ? lines[count - 1] : "";

    Autocomplete ac = autocomplete_new();
    for (guint i = 0; i + 1 < count; i++) {
        if (unsorted) {
            autocomplete_add_unsorted(ac, lines[i], reversed);
        } else {
            autocomplete_add(ac, lines[i]);
        }
    }
    autocomplete_length(ac);
    autocomplete_contains(ac, search);

    for (int round = 0; round < 3; round++) {
        gchar* found = autocomplete_complete(ac, search, quote, previous);
        g_free(found);
    }
    autocomplete_reset(ac);

    gchar* line = g_strdup_printf("/cmd %s", search);
    gchar* found = autocomplete_param_with_ac(line, "/cmd", ac, quote, previous);
    g_free(found);
    g_free(line);

    GList* list = autocomplete_create_list(ac);
    g_list_free_full(list, g_free);

    if (count > 1) {
        autocomplete_remove(ac, lines[0]);
        autocomplete_remove_older_than_max_reverse(ac, (int)(size % 5));
    }

    autocomplete_clear(ac);
    autocomplete_free(ac);
    g_strfreev(lines);
    g_free(text);
    return 0;
}
