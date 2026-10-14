/* SPDX-License-Identifier: GPL-3.0-only */
/* Real LCD/canvas code against a DMA that remains busy until explicitly
 * waited on. It checks source bytes at completion, catching early reuse. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __attribute__(x)
static const void *dma_source;
static uint32_t dma_bytes, dma_waits;
static uint8_t dma_copy[240u*124u*2u];
static void fm1_lcd_wait(void)
{
    if(dma_source){assert(!memcmp(dma_source,dma_copy,dma_bytes));dma_source=0;dma_waits++;}
}
static void fm1_lcd_deselect(void) {}
static void fm1_lcd_send_cmd(uint8_t c){(void)c;assert(!dma_source);}
static void fm1_lcd_send_data(const void *p,uint32_t n)
{
    assert(!dma_source && n<=sizeof dma_copy);dma_source=p;dma_bytes=n;memcpy(dma_copy,p,n);
}
static void fm1_lcd_hw_init(void) {}
static void fm1_lcd_baud(uint32_t b){(void)b;}
#include "../firmware/src/lcd.c"
#include "../firmware/src/gfx.c"
int main(void)
{
    cv_begin(240,24,RGB(20,30,40));cv_rect(3,3,10,10,RGB(200,50,0));cv_blit(0,0);
    const uint16_t *first=cv_px;uint32_t before=dma_waits;
    cv_begin(55,60,RGB(0,70,0));cv_rect(0,0,20,20,RGB(90,0,90));
    assert(cv_px!=first && dma_waits==before && dma_source==first);
    assert(!memcmp(dma_source,dma_copy,dma_bytes));
    cv_blit(0,24);assert(dma_waits>before);
    before=dma_waits;
    cv_begin(240,124,RGB(10,0,0));assert(cv_px==first && dma_waits==before);
    cv_rect(0,0,240,124,RGB(100,100,100));cv_blit_from(0,24,2);
    assert(dma_source==cv_px+2u*240u);
    before=dma_waits;
    cv_begin(240,124,RGB(0,0,80));assert(dma_waits==before+1u && !dma_source);
    cv_blit(0,24);before=dma_waits;
    cv_begin(240,16,RGB(0,80,0));assert(cv_px!=first && dma_waits==before);
    cv_blit(0,220);lcd_fill(0,0,240,1,RGB(70,0,0));lcd_sync();
    assert(!dma_source);
    cv_begin(240,124,RGB(40,50,60));cv_blit_from(0,24,3);
    before=dma_waits;
    uint32_t *work=cv_work();
    assert((void *)work==(void *)cv_primary && !dma_source && !cv_flight && dma_waits==before+1u);
    memset(work,0xA5,16400u);                 /* decoded project overwrites only after DMA completed */
    cv_begin(240,124,RGB(60,50,40));cv_blit(0,24);
    cv_begin(240,16,RGB(9,81,0));cv_blit(0,220);
    before=dma_waits;work=cv_work();          /* small canvas still on DMA: borrow the other buffer */
    assert(dma_source==cv_alt && dma_waits==before);
    memset(work,0x5A,16400u);lcd_sync();assert(!dma_source);
    puts("LCD overlap: drawing overlaps DMA, partial/large/fill transfers preserve their source bytes");
    return 0;
}
