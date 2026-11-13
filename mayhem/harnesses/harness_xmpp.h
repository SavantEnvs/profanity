// Shared setup for the XMPP harnesses. stanza.c and form.c read the libstrophe context through
// connection_get_ctx(), which returns a file-static in src/xmpp/connection.c. Including that file in
// the harness translation unit (and leaving its object out of the link) lets us set that static
// without a connection or any profanity runtime state.
#ifndef HARNESS_XMPP_H
#define HARNESS_XMPP_H

#include "xmpp/connection.c"

static void
harness_quiet_log(void* userdata, xmpp_log_level_t level, const char* area, const char* msg)
{
}

static void
harness_glib_quiet(const gchar* domain, GLogLevelFlags level, const gchar* message, gpointer data)
{
}

static xmpp_ctx_t*
harness_ctx(void)
{
    static xmpp_ctx_t* ctx = NULL;
    if (!ctx) {
        g_log_set_default_handler(harness_glib_quiet, NULL);
        xmpp_initialize();
        static xmpp_log_t quiet = { harness_quiet_log, NULL };
        ctx = xmpp_ctx_new(NULL, &quiet);
        conn.xmpp_ctx = ctx;
    }
    return ctx;
}

// libstrophe parses a NUL-terminated string; returns NULL if it is not a single well-formed element.
static xmpp_stanza_t*
harness_parse(const uint8_t* data, size_t size)
{
    char* text = g_strndup((const gchar*)data, size);
    xmpp_stanza_t* stanza = xmpp_stanza_new_from_string(harness_ctx(), text);
    g_free(text);
    return stanza;
}

#endif
