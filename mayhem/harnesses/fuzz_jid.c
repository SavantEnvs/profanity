// Fuzzes profanity's JID handling (src/xmpp/jid.c): every address received from the
// server or typed by the user goes through jid_create()/jid_is_valid().
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "xmpp/jid.h"

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    gchar* input = g_strndup((const gchar*)data, size);

    jid_is_valid(input);
    jid_is_valid_user_jid(input);

    Jid* jid = jid_create(input);
    if (jid) {
        jid_is_valid_room_form(jid);
        jid_fulljid_or_barejid(jid);
        jid_ref(jid);
        jid_destroy(jid);

        Jid* rebuilt = jid_create_from_bare_and_resource(jid->barejid, jid->resourcepart);
        if (rebuilt) {
            jid_fulljid_or_barejid(rebuilt);
            jid_destroy(rebuilt);
        }
        gchar* full = create_fulljid(jid->barejid, jid->resourcepart ? jid->resourcepart : "res");
        g_free(full);
        jid_destroy(jid);
    }

    // Second half of the input as a separate resource part.
    gchar* bare = g_strndup((const gchar*)data, size / 2);
    gchar* resource = g_strndup((const gchar*)data + size / 2, size - size / 2);
    Jid* combined = jid_create_from_bare_and_resource(bare, resource);
    if (combined) {
        jid_fulljid_or_barejid(combined);
        jid_destroy(combined);
    }
    g_free(bare);
    g_free(resource);

    g_free(input);
    return 0;
}
