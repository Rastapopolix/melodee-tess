/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#undef memcpy
#undef memset
#undef memcmp
#define memcpy word_memcpy
#define memset word_memset
#define memcmp word_memcmp
#include "../firmware/src/libc.c"
#undef memcpy
#undef memset
#undef memcmp
_Static_assert(MELODEE_LIBC_WORDS,"exercise aligned word implementation");
int main(void)
{
    _Alignas(8) uint8_t src[280],actual[280],expected[280];
    for(unsigned i=0;i<280;i++)src[i]=(uint8_t)(i*37u);
    for(unsigned a=0;a<8;a++)for(unsigned b=0;b<8;b++)for(unsigned n=0;n<=256;n++){
        memset(actual,0xcc,sizeof actual);memset(expected,0xcc,sizeof expected);
        memcpy(expected+a,src+b,n);assert(word_memcpy(actual+a,src+b,n)==actual+a);
        assert(!memcmp(actual,expected,sizeof actual));
        for(unsigned c=0;c<256;c+=17){
            memset(actual,0xcc,sizeof actual);memset(expected,0xcc,sizeof expected);
            memset(expected+a,c,n);assert(word_memset(actual+a,c,n)==actual+a);
            assert(!memcmp(actual,expected,sizeof actual));
        }
        int x=memcmp(src+a,src+b,n),y=word_memcmp(src+a,src+b,n);assert((x>0)-(x<0)==(y>0)-(y<0));
    }
    for(unsigned i=0;i<256;i++){
        memcpy(actual,src,256);actual[i]^=255;
        int x=memcmp(actual,src,256),y=word_memcmp(actual,src,256);assert((x>0)-(x<0)==(y>0)-(y<0));
    }
    float f[4]={-1.0f,0.0f,0.125f,123.0f},copy[4];word_memcpy(copy,f,sizeof f);assert(!memcmp(copy,f,sizeof f));
    word_memset(copy,0,sizeof copy);for(unsigned i=0;i<4;i++)assert(copy[i]==0.0f);
    puts("Aligned libc: every length 0..256, all alignment pairs, canaries, compare ordering and float aliasing: OK");
}
