// Fuzzes XEP-0004 data-form parsing and re-serialisation (src/xmpp/form.c): the <x xmlns='jabber:x:data'/>
// forms servers send for MUC room configuration, ad-hoc commands and service discovery.
#include <stdint.h>
#include <string.h>

#include <glib.h>
#include <strophe.h>

#include "harness_xmpp.h"
#include "xmpp/form.h"
#include "xmpp/stanza.h"

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    xmpp_stanza_t* stanza = harness_parse(data, size);
    if (!stanza) {
        return -1;
    }

    DataForm* form = form_create(stanza);
    if (form) {
        (void)form_get_form_type_field(form);

        GSList* sorted = form_get_non_form_type_fields_sorted(form);
        for (GSList* curr = sorted; curr; curr = g_slist_next(curr)) {
            FormField* field = curr->data;
            GSList* values = form_get_field_values_sorted(field);
            g_slist_free(values);
            if (field->var) {
                form_get_field_value(form, field->var);
            }
        }
        g_slist_free(sorted);

        xmpp_stanza_t* submission = form_create_submission(form);
        if (submission) {
            xmpp_stanza_release(submission);
        }

        form_destroy(form);
    }

    xmpp_stanza_release(stanza);
    return 0;
}
