/* SPDX-License-Identifier: GPL-3.0-only */
/* Real UI and NOR paths: unsupported data stays intact until a confirmed replacement. */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"

static int same_object(uint32_t obj, const void *p, uint32_t len)
{
    uint8_t got[4096];
    return len <= sizeof got && st_load(obj, got, sizeof got) == (int)len && !memcmp(got, p, len);
}
static void user_page(void)
{
    for (uint32_t p = 0; p < NPAGES; p++) if (PAGES[p].graph == GR_USER) { ui.page = (uint8_t)p; break; }
    ui.home = ui.menu = ui.layer = 0; ui.uslot = 0;
}
int main(void)
{
    int bad = 0; char name[32];
    uint8_t old[3080]; memset(old, 0x5a, sizeof old);
    reset();
    st_save(OBJ_BANK0, old, sizeof old); proj_fetch(0);
    bad += check("unreadable project is occupied and named OLD FORMAT", !project_used(0) && project_old(0) && project_name(0, name) && !strcmp(name, "OLD FORMAT"));
    bad += check("new song avoids occupied old project slots", project_free_slot() == 1);
    project_load(0);
    bad += check("unreadable project load keeps its flash bytes", !strcmp(ui.msg, "OLD FORMAT") && same_object(OBJ_BANK0, old, sizeof old));

    reset(); st_save(OBJ_UPRESET0, old, sizeof old); up_boot();
    bad += check("boot preserves a wrong-shape preset bank", up_old(0) && up_old(15) && !up_used(0) && same_object(OBJ_UPRESET0, old, sizeof old));
    up_rec_t rec = {0}; rec.used = UP_USED; rec.ver = UP_VER; rec.engine = 9u; rec.np = P_COUNT; strcpy(rec.name, "NEW");
    for (uint32_t i = 0; i < P_COUNT; i++) rec.packed[i] = 64;
    uint32_t before = erases;
    bad += check("protocol cannot overwrite an old bank implicitly", up_put(0, &rec) == 2 && erases == before && same_object(OBJ_UPRESET0, old, sizeof old));
    set_engine(9u); user_page(); confirm_open(CF_OVR_USER, 0);
    bad += check("device save asks about the whole old bank", ui.confirm == CF_OVR_USER && user_old_bank(0));
    ui.confirm = 0;
    bad += check("cancel retains old bank and no write authority", !preset_old_write && same_object(OBJ_UPRESET0, old, sizeof old));
    confirm_open(CF_OVR_USER, 0); ui.confirm = 0; ui.name_active = 1;
    bad += check("accept opens naming without authorizing protocol writes", ui.name_active && !preset_old_write && up_put(0, &rec) == 2);
    ui.name_active = 0; up_ui_named(2, 0, "NEW");
    bad += check("confirmed device save replaces old bank once", up_used(0) && !up_old(0) && !up_old(15) && !preset_old_write);

    reset(); up_bank_t bank = {0}; bank.magic = UP_BANK_MAGIC; bank.rsize = sizeof(up_rec_t); bank.nslot = UP_PER_BANK;
    bank.r[0] = rec; bank.r[0].ver = 1;
    st_save(OBJ_UPRESET0, &bank, sizeof bank); up_boot();
    bad += check("unknown record inside current bank stays occupied", up_old(0) && !up_used(0) && !up_old(1));
    up_put(1, &rec); up_cache_reset();
    bad += check("saving another slot retains unknown record bytes", !memcmp(up_rec(0), &bank.r[0], sizeof(up_rec_t)) && up_old(0));

    reset(); st_save(OBJ_NATIVEFM0, old, sizeof old); st_save(OBJ_CZBANK0, old, sizeof old); up_boot();
    bad += check("boot never formats occupied old FM or CZ banks", native_old(ENGI_FM6, 0) && native_old(ENGI_CZ, 0) && same_object(OBJ_NATIVEFM0, old, sizeof old) && same_object(OBJ_CZBANK0, old, sizeof old));
    before = erases;
    bad += check("native direct saves cannot erase old banks", native_put(ENGI_FM6, 0, FM6_INIT) == 2 && native_put(ENGI_CZ, 0, cz_patch[0].raw) == 2 && erases == before);
    sysex_stage(ENGI_FM6, FM6_INIT, 1, FM6_PACKED); sysex_choose();
    bad += check("incoming patch waits for old-bank confirmation", ui.confirm == CF_IMPORT && sx_import.count == 1 && same_object(OBJ_NATIVEFM0, old, sizeof old));
    preset_old_allow(1); sysex_commit(); preset_old_allow(0);
    bad += check("confirmed import replaces old native bank", !sx_import.count && native_used(ENGI_FM6, 0) && !native_old(ENGI_FM6, 0) && !preset_old_write);

    reset(); st_save(OBJ_SETTINGS, old, sizeof old); persist_boot();
    bad += check("boot preserves unsupported settings record", persist_old && same_object(OBJ_SETTINGS, old, sizeof old));
    settings.palette = 2; settings_save();
    bad += check("first settings write requires replacement confirmation", ui.confirm == CF_OLD_SETTINGS && same_object(OBJ_SETTINGS, old, sizeof old));
    ui.confirm = 0; settings_poll();
    bad += check("cancel keeps unsupported settings protected", persist_old && same_object(OBJ_SETTINGS, old, sizeof old));
    settings_save(); ui.confirm = 0; settings_old_accept(); settings_save();
    bad += check("accepted settings replacement stores PER5", !persist_old && !tmpl_old && persist_saved.magic == PERSIST_MAGIC && !persist_pending);

    reset(); persist_t prefs = {0}; settings_export(&prefs);
    uint8_t legacy_template[sizeof(persist_t) + 20]; memcpy(legacy_template, &prefs, sizeof prefs); memset(legacy_template + sizeof prefs, 0x5a, 20);
    st_save(OBJ_SETTINGS, legacy_template, sizeof legacy_template); persist_boot();
    bad += check("current settings with old template stay protected", tmpl_old && !template_used() && persist_saved.magic == PERSIST_MAGIC);
    template_save();
    bad += check("template save asks before replacing old template", ui.confirm == CF_OLD_TEMPLATE && same_object(OBJ_SETTINGS, legacy_template, sizeof legacy_template));
    ui.confirm = 0; settings_old_accept(); template_save();
    bad += check("accepted template save writes current template", template_used() && !template_old() && !tmpl_dirty);
    printf("%s\n", bad ? "STORAGE SAFETY TEST FAILED" : "storage safety test passed");
    return bad != 0;
}
