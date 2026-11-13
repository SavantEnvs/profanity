// Fuzzes the XMPP stanza parsers that handle server-controlled XML (src/xmpp/stanza.c):
// presence, entity capabilities (XEP-0115/0030), delay, error, MUC status and chat-state elements.
#include <stdint.h>
#include <string.h>

#include <glib.h>
#include <strophe.h>

#include "harness_xmpp.h"
#include "xmpp/stanza.h"

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    xmpp_stanza_t* stanza = harness_parse(data, size);
    if (!stanza) {
        return -1;
    }

    int err = 0;
    XMPPPresence* presence = stanza_parse_presence(stanza, &err);
    if (presence) {
        Resource* resource = stanza_resource_from_presence(presence);
        if (resource) {
            resource_destroy(resource);
        }
        stanza_free_presence(presence);
    }

    XMPPCaps* caps = stanza_parse_caps(stanza);
    stanza_free_caps(caps);

    EntityCapabilities* entity = stanza_create_caps_from_query_element(stanza);
    caps_destroy(entity);

    gchar* sha1 = stanza_create_caps_sha1_from_query(stanza);
    g_free(sha1);

    xmpp_stanza_t* child = xmpp_stanza_get_children(stanza);
    if (child) {
        entity = stanza_create_caps_from_query_element(child);
        caps_destroy(entity);
        sha1 = stanza_create_caps_sha1_from_query(child);
        g_free(sha1);
    }

    GDateTime* delay = stanza_get_delay(stanza);
    if (delay) {
        g_date_time_unref(delay);
    }
    delay = stanza_get_oldest_delay(stanza);
    if (delay) {
        g_date_time_unref(delay);
    }

    gchar* msg = stanza_get_error_message(stanza);
    g_free(msg);
    msg = stanza_get_status(stanza, "none");
    g_free(msg);
    msg = stanza_get_show(stanza, "online");
    g_free(msg);
    msg = stanza_get_reason(stanza);
    g_free(msg);
    msg = stanza_text_strdup(stanza);
    g_free(msg);

    GSList* codes = stanza_get_status_codes_by_ns(stanza, STANZA_NS_MUC_USER);
    g_slist_free_full(codes, g_free);

    GHashTable* contacts = stanza_get_service_contact_addresses(harness_ctx(), stanza);
    g_hash_table_destroy(contacts);

    stanza_get_idle_time(stanza);
    stanza_get_new_nick(stanza);
    stanza_get_actor(stanza);
    stanza_contains_chat_state(stanza);
    stanza_is_muc_presence(stanza);
    stanza_is_room_nick_change(stanza);
    stanza_room_destroyed(stanza);
    stanza_muc_requires_config(stanza);
    stanza_is_muc_self_presence(stanza, "me@example.com/res");

    xmpp_stanza_release(stanza);
    return 0;
}
