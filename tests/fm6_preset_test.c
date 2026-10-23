/* SPDX-License-Identifier: GPL-3.0-only */
/* Independent native preset pools and failure recovery on simulated NOR. */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"

static void reboot_presets(void)
{
    up_cache_reset();native_cache_reset();up_boot();
}
static int bank_import(void)
{
    int bad=0;static uint8_t bank[4096],old[4096];uint32_t saved;
    reset();fm6_init();up_boot();
    for(uint32_t k=0;k<32;k++)fm6_factory(k%FM6_NFAC,bank+k*128u);
    uint32_t writes=erases;
    bad+=check("32-voice import commits two storage objects, not 32 saves",native_fm_import32(32,bank,&saved)==0 && saved==32 && erases==writes+2);
    reboot_presets();int ok=1;
    for(uint32_t k=0;k<32;k++)ok &= native_used(ENGI_FM6,32+k) && !memcmp(native_raw(ENGI_FM6,32+k),bank+k*128u,128);
    bad+=check("all imported VMEM patches survive reboot in F033-F064",ok && !native_count(ENGI_CZ) && !native_used(ENGI_FM6,0));
    memcpy(old,bank,sizeof bank);bank[4095]=0x80;writes=erases;
    bad+=check("bank validation finishes before any flash write",native_fm_import32(32,bank,&saved)==1 && saved==0 && erases==writes);bank[4095]=old[4095];
    song.playing=1;bad+=check("bank import refuses transport without changing flash",native_fm_import32(32,bank,&saved)==2 && !saved && erases==writes);song.playing=0;
    for(uint32_t k=0;k<32;k++)bank[k*128u+118]='X';
    fail_after=2;bad+=check("failure in first bank object leaves all old patches in RAM",native_fm_import32(32,bank,&saved)==2 && !saved && !memcmp(native_raw(ENGI_FM6,32),old,128));fail_after=-1;reboot_presets();
    bad+=check("interrupted first object retains prior bank after reboot",!memcmp(native_raw(ENGI_FM6,32),old,128) && !memcmp(native_raw(ENGI_FM6,63),old+31*128u,128));
    fail_after=12;bad+=check("failure in second object reports exactly 16 committed patches",native_fm_import32(32,bank,&saved)==2 && saved==16 && !memcmp(native_raw(ENGI_FM6,32),bank,128) && !memcmp(native_raw(ENGI_FM6,48),old+16*128u,128));fail_after=-1;reboot_presets();
    bad+=check("partial import RAM matches the durable objects after reboot",!memcmp(native_raw(ENGI_FM6,47),bank+15*128u,128) && !memcmp(native_raw(ENGI_FM6,48),old+16*128u,128));
    bad+=check("resending completes the interrupted bank",!native_fm_import32(32,bank,&saved) && saved==32);reboot_presets();
    bad+=check("resend commits the final voice",!memcmp(native_raw(ENGI_FM6,63),bank+31*128u,128));
    return bad;
}
int main(void)
{
    int bad=0;uint8_t fm[FM6_PACKED],cz[144u],got[FM6_PACKED];
    reset();fm6_init();up_boot();
    fm6_factory(3,fm);memcpy(fm+118,"MY VOICE  ",10);memset(cz,0,sizeof cz);memcpy(cz+128,"MY CZ TONE      ",16);cz[20]=0x37;
    bad+=check("64 FM6 slots, independent from 64 general slots; no CZ-1 or Prophet pool",native_limit(ENGI_FM6)==64 && !native_limit(ENGI_CZ) && !native_limit(ENGI_PROPHET) && UP_SLOTS==64 && !native_count(ENGI_FM6));
    bad+=check("F064 stores original 128-byte DX7 VMEM",native_put(ENGI_FM6,63,fm)==0 && !memcmp(native_raw(ENGI_FM6,63),fm,128));
    bad+=check("a CZ-1 tone is refused (its store retired with the engine)",native_put(ENGI_CZ,127,cz)==1 && !native_used(ENGI_CZ,127));
    bad+=check("a native slot cannot occupy a general slot",!up_used(63) && native_count(ENGI_FM6)==1 && !native_count(ENGI_CZ));
    TSEL->p[P_RESERVED33]=47;TSEL->p[P_LD_PIT]=12;TSEL->step[0]=(step_t){.time=ST_NOTE,.n=1,.note={60}};
    native_load(ENGI_FM6,63,0);fm6_pack(fm6_patch[0],got);
    bad+=check("native FM6 load keeps effects, modulation and pattern",!memcmp(got,fm,128) && TSEL->p[P_RESERVED33]==47 && TSEL->p[P_LD_PIT]==12 && TSEL->step[0].note[0]==60);
    bad+=check("a native CZ load is refused (the engine and its store retired)",native_load(ENGI_CZ,127,0)!=0 && TSEL->eng_req==ENGI_FM6);
    uint32_t total,cur=eng_list_pos(&total),fac=total-1u;
    bad+=check("F064 occupies the next ordinary preset position after FM6 factory voices",cur==fac && user_of(TSEL)==63);
    eng_list_step(1);bad+=check("ordinary engine preset scrolling wraps from F064 to the first voice",TSEL->eng_req==ENGI_FM6 && TSEL->preset==0 && !TSEL->user);
    eng_list_step(-1);fm6_pack(fm6_patch[0],got);bad+=check("ordinary engine preset scrolling returns to F064",TSEL->user==64 && !memcmp(got,fm,128));
    cur=preset_all_pos(&total);uint32_t k,e=preset_all_at(cur,&k);
    bad+=check("global preset browser identifies native slots as user sounds",e==USER_NATIVE_FM && k==63);
    favorite_set(USER_NATIVE_FM,63,1);favorite_set(USER_NATIVE_CZ,127,1);favorites.filter=1;
    bad+=check("native presets participate in the regular favorites filter (a CZ favorite no longer lists)",preset_pos(&total)<1 && total==1);
    preset_go(0);bad+=check("filtered global scrolling loads F064 directly",TSEL->eng_req==ENGI_FM6 && TSEL->user==64);
    settings_save();memset(&favorites,0,sizeof favorites);persist_t persisted;int n=st_load(OBJ_SETTINGS,&persisted,sizeof persisted);settings_import(&persisted,n);
    bad+=check("native favorites persist without changing the settings layout",favorite_has(USER_NATIVE_FM,63) && !favorite_has(USER_NATIVE_CZ,127));favorites.filter=0;
    {char zn[11];__typeof__(favorites) keep=favorites;memset(&favorites,0,sizeof favorites);
     favorite_set(ENGI_FM6,25,1);favorite_set(USER_NATIVE_FM,1,1);favorite_set(USER_NATIVE_FM,1,0);
     bad+=check("ZEN-1's FM6 presets (after INIT VOICE) keep their favorites apart from the native slots",favorite_has(ENGI_FM6,25) && !favorite_has(ENGI_FM6,26) && !favorite_has(USER_NATIVE_FM,1));
     favorites=keep;engine_load_of(TSEL,ENGI_FM6,25);fm6_name(zn,fm6_patch[0]);
     bad+=check("FM6 preset 25 loads ZEN-1's SOFT PAD, INIT VOICE stays at 24",!strcmp(zn,"SOFT PAD") && preset_init(ENGI_FM6)==24u);}
    reboot_presets();
    bad+=check("the native FM6 pool survives reboot byte for byte",native_used(ENGI_FM6,63) && !memcmp(native_raw(ENGI_FM6,63),fm,128));
    uint8_t newer[128];memcpy(newer,fm,128);newer[118]='N';fail_after=2;
    bad+=check("torn native save reports failure and rolls RAM back",native_put(ENGI_FM6,63,newer)==2 && !memcmp(native_raw(ENGI_FM6,63),fm,128));fail_after=-1;reboot_presets();
    bad+=check("torn native save keeps the last flash commit",!memcmp(native_raw(ENGI_FM6,63),fm,128));
    song.playing=1;uint32_t writes=erases;bad+=check("native writes refuse playback without flash erase",native_put(ENGI_FM6,0,fm)==2 && erases==writes);song.playing=0;
    native_put(ENGI_FM6,63,0);reboot_presets();
    bad+=check("erased native presets stay erased across boot",!native_used(ENGI_FM6,63));

    /* An FM6 general preset keeps its voice in the native slot of the same number. */
    reset();fm6_init();set_engine_of(TSEL,ENGI_FM6);
    uint8_t voice[FP_SIZE+1];fm6_unpack(fm,voice);fm6_set_patch(0,voice);
    up_store(47,"MIDDLE");reboot_presets();
    bad+=check("an FM6 user preset's voice survives reboot in its native slot",up_used(47) && native_used(ENGI_FM6,47) && !memcmp(native_raw(ENGI_FM6,47),fm,128));
    int layout=1;
    /* (the legacy projects are retired: the Prophet banks took their place when the app area grew) */
    #define RETIRED(o) st_retired(o)
    static int16_t owner[0x100000/ST_SECTOR];   /* (each sector of each live copy: one owner; a project copy has ten) */
    for(uint32_t i=0;i<sizeof owner/sizeof owner[0];i++)owner[i]=-1;
    for(uint32_t a=0;a<OBJ_COUNT;a++)for(uint32_t ac=0;ac<2;ac++){
        if(RETIRED(a))continue;
        for(uint32_t i=0;i<st_sectors(a);i++){
            uint32_t lo=st_proj(a)?st_proj_sector(a,ac,i):st_sector(a,ac);
            layout &= !(lo<0xE5000u && lo+ST_SECTOR>0xE0000u) && lo>=0x93000u;   /* (above the app area) */
            layout &= owner[lo/ST_SECTOR]<0; owner[lo/ST_SECTOR]=(int16_t)a;
        }
    }
    bad+=check("native storage stays separate from projects, general presets and OTA",layout);
    bad+=bank_import();
    printf("Native preset test %s\n",bad?"FAILED":"passed");return !!bad;
}
