/* SPDX-License-Identifier: GPL-3.0-only */
/* The Prophet's user bank on simulated NOR: identity, atomic saves, favorites. The engine was retired 2026-10-22;
 * its stored programs stay (prophet_user.c) until the own project format, so the playing and project checks went with it */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
int main(int argc,char **argv)
{
    int bad=0;reset();up_boot();p5_patch_t p,q;
    uint32_t default_erases=erases;int defaults=1;
    for(uint32_t slot=0;slot<128u;slot++)defaults &= !p5_user_used(slot);
    bad+=check("fresh Prophet user storage is empty without erasing flash",defaults&&erases==default_erases);
    bad+=check("Sequential's first factory program remains separate",!memcmp(p5_fac(0)->raw+P5_NAME,"It's a Prophet 5",15)&&p5_fac(0)->model==0x32);
    p5_bank_t empty;memset(&empty,0,sizeof empty);empty.magic=P5_BANK_MAGIC;
    bad+=check("an existing intentionally empty saved bank keeps its empty collection",!st_save(OBJ_P5BANK0,&empty,sizeof empty)&&(p5_user_reset(),!p5_user_used(0)&&!p5_user_used(25)&&!p5_user_used(26)));
    bad+=check("an explicitly saved copy remains in its user slot",!p5_user_put(0,p5_fac(0))&&(p5_user_reset(),p5_user_used(0)));
    reset();up_boot();
    p5_patch_init(&p);for(uint32_t i=88;i<133;i++)p.raw[i]=(uint8_t)(i*179u);
    int all=1;for(uint32_t slot=0;slot<128;slot++){p.raw[65]=(uint8_t)('A'+slot%26);all &= !native_put(ENGI_PROPHET,slot,(const uint8_t *)&p);}
    bad+=check("all 128 native slots fit without changing historical object IDs",all && OBJ_P5BANK0==OBJ_NATIVEFM0+2 && st_sector(OBJ_P5BANK0,0)==0x93000 && st_sector(OBJ_P5BANK0+4,1)==0x9C000 &&
                 OBJ_P5BANK0==25u);   /* persistent and backup object identifiers stay fixed */
    p5_user_reset();bad+=check("P128 survives reboot with all opaque bytes and wire metadata",!p5_user_get(127,&q)&&!memcmp(&p,&q,sizeof p));
    uint32_t before=erases;bad+=check("slot 129 is rejected before any erase",native_put(ENGI_PROPHET,128,(const uint8_t *)&p)==1&&erases==before);
    q=p;q.raw[17]=13;fail_after=2;bad+=check("interrupted native save reports failure and rolls RAM back",native_put(ENGI_PROPHET,127,(const uint8_t *)&q)==2&&!memcmp(native_raw(ENGI_PROPHET,127),&p,sizeof p));fail_after=-1;p5_user_reset();
    bad+=check("interrupted native save keeps the last flash commit after reboot",!p5_user_get(127,&q)&&!memcmp(&p,&q,sizeof p));
    song.playing=1;before=erases;bad+=check("native saves during playback cannot erase storage",native_put(ENGI_PROPHET,0,(const uint8_t *)&p)==2&&before==erases);song.playing=0;
    bad+=check("P128 and Prophet INIT favorites use separate persistent masks",favorite_set(USER_NATIVE_P5,127,1)&&favorite_set(ENGI_PROPHET,0,1));p5_user_reset();
    bad+=check("Prophet favorites survive cache reset without moving old identifiers",favorite_has(USER_NATIVE_P5,127)&&favorite_has(ENGI_PROPHET,0)&&USER_GENERAL==16&&USER_NATIVE_FM==17&&USER_NATIVE_CZ==18);
    favorite_set(0,11,1);favorite_set(ENGI_PROPHET,200,1);settings_save();
    persist_t prefs;int prefs_len=st_load(OBJ_SETTINGS,&prefs,sizeof prefs);memset(&favorites,0,sizeof favorites);
    bad+=check("all Prophet factory stars persist without overwriting retired ANALOG's stars",prefs_len==(int)sizeof prefs&&settings_import(&prefs,prefs_len)&&favorite_has(ENGI_PROPHET,0)&&favorite_has(ENGI_PROPHET,200)&&favorite_has(0,11)&&favorite_has(USER_NATIVE_P5,127));
    if(argc>1){FILE *f=fopen(argv[1],"rb");static uint8_t old[27200];int ok=f&&fread(old,1,sizeof old,f)==sizeof old;if(f)fclose(f);bad+=check("real pre-Prophet runtime imports with legacy engine identities intact",ok&&bank_valid(old,sizeof old)&&proj_scratch.t[0].engine==old[8+68+92]);}
    printf("Prophet user bank: %d failure(s)\n",bad);return !!bad;
}
