// Known-answer probe for mayhem/test.sh. Runs fixed inputs through profanity's parsing code
// (built with the project's normal flags) and prints one `name=value` line per result; test.sh
// compares every line against the expected value, so the program has to compute the right answers.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <strophe.h>

#include "common.h"
#include "harness_xmpp.h"
#include "tools/autocomplete.h"
#include "tools/parser.h"
#include "xmpp/form.h"
#include "xmpp/jid.h"
#include "xmpp/stanza.h"

#define S(s) ((s) ? (s) : "(null)")

static void
print_args(const char* name, gchar** args, gboolean ok)
{
    if (!ok || !args) {
        printf("%s=fail\n", name);
        return;
    }
    gchar* joined = g_strjoinv("|", args);
    printf("%s=ok:%s\n", name, joined);
    g_free(joined);
    g_strfreev(args);
}

static xmpp_stanza_t*
parse(const char* xml)
{
    return xmpp_stanza_new_from_string(harness_ctx(), xml);
}

static void
kat_parser(void)
{
    gboolean ok = FALSE;
    print_args("parse_args.two", parse_args("/cmd arg1 arg2", 2, 2, &ok), ok);
    print_args("parse_args.too_few", parse_args("/cmd a", 2, 2, &ok), ok);
    print_args("parse_args.too_many", parse_args("/cmd a b c", 0, 2, &ok), ok);
    print_args("parse_args.quoted", parse_args("/cmd \"hello world\" x", 1, 2, &ok), ok);
    print_args("parse_args.escaped", parse_args("/cmd a\\ b c", 1, 2, &ok), ok);
    print_args("parse_args.empty_quotes", parse_args("/cmd \"\" x", 1, 2, &ok), ok);
    print_args("parse_args.none", parse_args("/cmd", 0, 2, &ok), ok);
    print_args("parse_args_with_freetext.msg", parse_args_with_freetext("/msg bob hi there friend", 2, 2, &ok), ok);
    print_args("parse_args_as_one.correct", parse_args_as_one("/correct fixed \"it\"", 0, 0, &ok), ok);
    print_args("parse_args_as_one.none", parse_args_as_one("/correct", 0, 0, &ok), ok);
    printf("count_tokens=%d\n", count_tokens("/cmd a \"b c\"  d"));
    gchar* start = get_start("/cmd a  b c", 3);
    printf("get_start=[%s]\n", start);
    g_free(start);

    gchar** args = parse_args("/cmd -a one -b two", 0, 4, &ok);
    gchar* keys[] = { "-a", "-b", NULL };
    gboolean res = FALSE;
    GHashTable* options = parse_options(args, keys, &res);
    printf("parse_options=%d:%s:%s\n", res, S((gchar*)g_hash_table_lookup(options, "-a")), S((gchar*)g_hash_table_lookup(options, "-b")));
    options_destroy(options);
    g_strfreev(args);
    args = parse_args("/cmd -c one", 0, 4, &ok);
    options = parse_options(args, keys, &res);
    printf("parse_options.bad_key=%d\n", res);
    options_destroy(options);
    g_strfreev(args);
}

static void
kat_jid(void)
{
    Jid* jid = jid_create("Me@Example.COM/Home Office");
    printf("jid.bare=%s\n", S(jid->barejid));
    printf("jid.local=%s\n", S(jid->localpart));
    printf("jid.domain=%s\n", S(jid->domainpart));
    printf("jid.resource=%s\n", S(jid->resourcepart));
    printf("jid.full=%s\n", S(jid_fulljid_or_barejid(jid)));
    jid_destroy(jid);

    jid = jid_create("example.com");
    printf("jid.domain_only=%s|%s|%s\n", S(jid->localpart), S(jid->domainpart), jid_fulljid_or_barejid(jid));
    jid_destroy(jid);

    printf("jid.valid.basic=%d\n", jid_is_valid("a@b.c/d"));
    printf("jid.valid.slash_in_resource=%d\n", jid_is_valid("a@b/c/d@e"));
    printf("jid.valid.two_at=%d\n", jid_is_valid("a@b@c"));
    printf("jid.valid.empty_local=%d\n", jid_is_valid("@example.com"));
    printf("jid.valid.empty=%d\n", jid_is_valid(""));
    printf("jid.valid.bad_local_char=%d\n", jid_is_valid("a b@example.com"));
    printf("jid.user.domain=%d\n", jid_is_valid_user_jid("example.com"));
    printf("jid.user.bare=%d\n", jid_is_valid_user_jid("u@example.com"));
    gchar* full = create_fulljid("Foo@Bar", "baz");
    printf("jid.create_fulljid=%s\n", full);
    g_free(full);
}

static void
kat_common(void)
{
    int value = -1;
    gchar* err = NULL;
    gboolean ok = strtoi_range("42", &value, 0, 100, &err);
    printf("strtoi.ok=%d:%d\n", ok, value);
    ok = strtoi_range("101", &value, 0, 100, &err);
    printf("strtoi.range=%d:%s\n", ok, S(err));
    g_free(err);
    err = NULL;
    ok = strtoi_range("abc", &value, 0, 100, &err);
    printf("strtoi.nan=%d:%s\n", ok, S(err));
    g_free(err);

    printf("release_is_new.patch=%d\n", release_is_new("0.19.0", "0.19.1"));
    printf("release_is_new.older=%d\n", release_is_new("1.0.0", "0.99.99"));
    printf("release_is_new.same=%d\n", release_is_new("1.2.3", "1.2.3"));
    printf("release_is_new.garbage=%d\n", release_is_new("x", "1.0.0"));

    gchar* s = str_xml_sanitize("a\x01" "b\tc");
    printf("xml_sanitize=%s\n", s);
    g_free(s);
    s = strip_arg_quotes("\"hello world\"");
    printf("strip_arg_quotes=%s\n", s);
    g_free(s);
    printf("utf8_display_len=%d\n", utf8_display_len("\xe6\x97\xa5\xe6\x9c\xac" "a"));

    gchar dir[] = "a\xe2\x80\xae" "b";
    str_clean_directional_chars(dir);
    printf("clean_directional=%s\n", dir);
    gchar term[] = "x\x1b[1my\x7f";
    str_clean_terminal_control_chars(term);
    printf("clean_terminal=%s\n", term);

    GSList* mentions = get_mentions(TRUE, FALSE, "Hi Bob, bob! bobby", "bob");
    GString* offsets = g_string_new("");
    for (GSList* m = mentions; m; m = g_slist_next(m)) {
        g_string_append_printf(offsets, "%d,", GPOINTER_TO_INT(m->data));
    }
    printf("mentions=%s\n", offsets->str);
    g_string_free(offsets, TRUE);
    g_slist_free(mentions);

    gchar** argv = format_call_external_argv("open %u -f %p", "http://x", "f.txt");
    gchar* joined = g_strjoinv("|", argv);
    printf("external_argv=%s\n", joined);
    g_free(joined);
    g_strfreev(argv);
}

static void
kat_autocomplete(void)
{
    Autocomplete ac = autocomplete_new();
    autocomplete_add(ac, "banana");
    autocomplete_add(ac, "apple");
    autocomplete_add(ac, "apricot");
    printf("ac.length=%d\n", autocomplete_length(ac));
    for (int i = 0; i < 3; i++) {
        gchar* found = autocomplete_complete(ac, "ap", FALSE, FALSE);
        printf("ac.complete.%d=%s\n", i, S(found));
        g_free(found);
    }
    autocomplete_reset(ac);
    gchar* found = autocomplete_param_with_ac("/cmd ban", "/cmd", ac, FALSE, FALSE);
    printf("ac.param=%s\n", S(found));
    g_free(found);
    autocomplete_free(ac);
}

static void
kat_xmpp(void)
{
    xmpp_stanza_t* st = parse("<presence from='Romeo@example.net/orchard'><show>dnd</show><status>Busy</status><priority>5</priority></presence>");
    int err = 0;
    XMPPPresence* presence = stanza_parse_presence(st, &err);
    printf("presence=%s|%s|%s|%d\n", S(presence->jid->barejid), S(presence->show), S(presence->status), presence->priority);
    stanza_free_presence(presence);
    xmpp_stanza_release(st);

    st = parse("<presence><show>away</show></presence>");
    presence = stanza_parse_presence(st, &err);
    printf("presence.no_from=%d:%d\n", presence != NULL, err == STANZA_PARSE_ERROR_NO_FROM);
    xmpp_stanza_release(st);

    // XEP-0115 section 5.2 "simple generation example": the verification string hashes to this value.
    st = parse("<query xmlns='http://jabber.org/protocol/disco#info' node='http://code.google.com/p/exodus#QgayPKawpkPSDYmwT/WM94uAlu0='>"
               "<identity category='client' name='Exodus 0.9.1' type='pc'/>"
               "<feature var='http://jabber.org/protocol/caps'/><feature var='http://jabber.org/protocol/disco#info'/>"
               "<feature var='http://jabber.org/protocol/disco#items'/><feature var='http://jabber.org/protocol/muc'/></query>");
    gchar* sha1 = stanza_create_caps_sha1_from_query(st);
    printf("caps.sha1.simple=%s\n", S(sha1));
    g_free(sha1);
    EntityCapabilities* caps = stanza_create_caps_from_query_element(st);
    printf("caps.identity=%s|%s|%s\n", S(caps->identity->category), S(caps->identity->type), S(caps->identity->name));
    printf("caps.features=%d\n", g_slist_length(caps->features));
    caps_destroy(caps);
    xmpp_stanza_release(st);

    // XEP-0115 section 5.3 complex example: software-info form fields (the identity-hash part is not asserted).
    st = parse("<query xmlns='http://jabber.org/protocol/disco#info' node='http://psi-im.org#q07IKJEyjvHSyhy//CH0CxmKi8w='>"
               "<identity xml:lang='en' category='client' name='Psi 0.11' type='pc'/>"
               "<identity xml:lang='el' category='client' name='\xce\xa8 0.11' type='pc'/>"
               "<feature var='http://jabber.org/protocol/caps'/><feature var='http://jabber.org/protocol/disco#info'/>"
               "<feature var='http://jabber.org/protocol/disco#items'/><feature var='http://jabber.org/protocol/muc'/>"
               "<x xmlns='jabber:x:data' type='result'>"
               "<field var='FORM_TYPE' type='hidden'><value>urn:xmpp:dataforms:softwareinfo</value></field>"
               "<field var='ip_version' type='text-multi'><value>ipv4</value><value>ipv6</value></field>"
               "<field var='os'><value>Mac</value></field><field var='os_version'><value>10.5.1</value></field>"
               "<field var='software'><value>Psi</value></field><field var='software_version'><value>0.11</value></field>"
               "</x></query>");
    caps = stanza_create_caps_from_query_element(st);
    printf("caps.software=%s|%s|%s|%s\n", S(caps->software_version->software), S(caps->software_version->software_version),
           S(caps->software_version->os), S(caps->software_version->os_version));
    caps_destroy(caps);
    xmpp_stanza_release(st);

    st = parse("<iq type='error'><error type='cancel'><not-allowed xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></iq>");
    gchar* msg = stanza_get_error_message(st);
    printf("error_message=%s\n", S(msg));
    g_free(msg);
    xmpp_stanza_release(st);

    st = parse("<message><delay xmlns='urn:xmpp:delay' stamp='2002-09-10T23:08:25Z'/></message>");
    GDateTime* delay = stanza_get_delay(st);
    gchar* iso = delay ? g_date_time_format_iso8601(delay) : NULL;
    printf("delay=%s\n", S(iso));
    g_free(iso);
    if (delay) {
        g_date_time_unref(delay);
    }
    xmpp_stanza_release(st);

    st = parse("<x xmlns='jabber:x:data' type='result'><field var='FORM_TYPE' type='hidden'><value>urn:test</value></field>"
               "<field var='os'><value>Mac</value></field><field var='tags' type='list-multi'><value>b</value><value>a</value></field></x>");
    DataForm* form = form_create(st);
    printf("form.type=%s\n", S(form_get_form_type_field(form)));
    printf("form.os=%s\n", S(form_get_field_value(form, "os")));
    GSList* sorted = form_get_non_form_type_fields_sorted(form);
    printf("form.fields=%d\n", g_slist_length(sorted));
    g_slist_free(sorted);
    form_destroy(form);
    xmpp_stanza_release(st);
}

int
main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    kat_parser();
    kat_jid();
    kat_common();
    kat_autocomplete();
    kat_xmpp();
    return 0;
}
