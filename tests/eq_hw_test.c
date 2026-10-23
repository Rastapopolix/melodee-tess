/* SPDX-License-Identifier: GPL-3.0-only */
/* Register-model tests establish ownership, row isolation and bounded failure;
 * actual accelerator arithmetic/latency still needs an FM-1 measurement. */
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint32_t reg[8],ram[32],writes,starts,hang;
static uint32_t eq_read(uint32_t a){return a>=0x1e800u?ram[(a-0x1e800u)/4u]:reg[(a-0x1e000u)/4u];}
static void eq_write(uint32_t a,uint32_t v)
{
    writes++;
    if(a>=0x1e800u){ram[(a-0x1e800u)/4u]=v;return;}
    reg[(a-0x1e000u)/4u]=v;
    if(a==0x1e000u && (v&3u)==3u){
        starts++;assert(v==0x400300a3u);assert(reg[3]==0x00040004u && reg[2]==0 && reg[7]==0);
        if(!hang){reg[0]|=0x80000000u;ram[5]=0x3f800000u;}
    }
}
#define MELODEE_HW_EQ 1
#define FM1_EQ_TEST 1
#define FE_READ(a) eq_read(a)
#define FE_WRITE(a,v) eq_write(a,v)
#define FE_SYNC() ((void)0)
#include "../firmware/hal/fm1_eq.h"
int main(void)
{
    float row[4][8]={{0}},in[32]={0},out[32]={0};row[0][0]=1;
    assert(fm1_eq_run(row,in,out,32));assert(row[0][5]==1 && starts==1);
    assert(reg[1]==0x00800080u && reg[6]==31u && !(reg[0]&0x8000u));
    float other[4][8]={{0}};other[0][0]=0.5f;
    assert(fm1_eq_run(other,in,out,1));assert(other[0][5]==1 && row[0][0]==1);
    uint32_t before=writes;reg[0]=0x01000000u;
    assert(!fm1_eq_run(row,in,out,32) && writes==before);reg[0]=0;
    assert(!fm1_eq_run(row,in,out,0) && !fm1_eq_run(row,in,out,33));
    hang=1;assert(!fm1_eq_run(row,in,out,32));assert(fm1_eq_fault && !reg[0]);
    before=writes;assert(!fm1_eq_run(row,in,out,32) && writes==before);
    puts("EQ hardware interface: float strides, isolated rows, no IRQ, busy guard, bounded timeout and permanent fallback: OK");
}
