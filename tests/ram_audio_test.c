/* SPDX-License-Identifier: GPL-3.0-only */
/* Four largest engine states must fit the resident audio arena. */
#define MELODEE_RAM_AUDIO 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
int main(void){
 host_tracks_init();memset(resource,0,sizeof resource);
 uint32_t largest=0,engine=0;
 for(uint32_t k=0;k<NENG_SHOWN;k++){uint32_t e=eng_vis(k),bytes=eng_state_size(e);if(bytes>largest){largest=bytes;engine=e;}}
 for(uint32_t k=0;k<NPART;k++){trk[k].engine=(uint8_t)engine;assert(eng_state_prepare(&trk[k]));}
 assert(resource_used()==largest*NPART);assert(largest*NPART<=64u*1024u);
 printf("Resident dry audio: four largest engines use %u B, fit 64 KiB: OK\n",largest*NPART);
 return 0;
}
