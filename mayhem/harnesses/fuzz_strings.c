// Fuzzes the string helpers in src/common.c that process untrusted text: message mention matching,
// XML/terminal sanitisers, argument unquoting, number parsing and external-command templates.
//
// Input layout: byte 0 selects the function (low bits) and boolean flags (high bits); the rest is
// split on the first '\n' into two UTF-8 strings.
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "common.h"

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return -1;
    }

    unsigned selector = data[0] % 12;
    gboolean flag_a = (data[0] & 0x40) != 0;
    gboolean flag_b = (data[0] & 0x80) != 0;

    gchar* text = g_strndup((const gchar*)data + 1, size - 1);
    if (!g_utf8_validate(text, -1, NULL)) {
        g_free(text);
        return -1;
    }

    gchar* second = NULL;
    gchar* newline = strchr(text, '\n');
    if (newline) {
        *newline = '\0';
        second = newline + 1;
    }

    switch (selector) {
    case 0: {
        int value = 0;
        gchar* err = NULL;
        strtoi_range(text, &value, -1000, 100000, &err);
        g_free(err);
        break;
    }
    case 1: {
        int value = 0;
        gchar* err = NULL;
        string_to_verbosity(text, &value, &err);
        g_free(err);
        break;
    }
    case 2:
        utf8_display_len(text);
        break;
    case 3: {
        gchar* clean = str_xml_sanitize(text);
        g_free(clean);
        break;
    }
    case 4:
        str_clean_directional_chars(text);
        break;
    case 5:
        str_clean_terminal_control_chars(text);
        break;
    case 6:
        str_ascii_casecmp0(text, second);
        break;
    case 7:
        release_is_new(text, second);
        break;
    case 8: {
        gchar* stripped = strip_arg_quotes(text);
        g_free(stripped);
        break;
    }
    case 9: {
        GSList* mentions = get_mentions(flag_a, flag_b, text, second);
        g_slist_free(mentions);
        break;
    }
    case 10: {
        gchar** argv = format_call_external_argv(text, second, second);
        g_strfreev(argv);
        break;
    }
    default:
        // what == NULL: the matcher skips its cons_show() error output, which needs the UI profanity
        // always initialises first. This is valid_tls_policy_option() minus that console reporting.
        string_matches_one_of(NULL, text, TRUE, "force", "allow", "trust", "disable", "legacy", "direct", NULL);
        break;
    }

    g_free(text);
    return 0;
}
