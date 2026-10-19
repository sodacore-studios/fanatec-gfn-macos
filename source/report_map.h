// SPDX-License-Identifier: GPL-2.0-only
#ifndef REPORT_MAP_H
#define REPORT_MAP_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// CSL DD 0eb7:0020: captured PC-mode descriptor, report 1.
// 4-bit hat, 124 button bits, four 16-bit axes, ancillary/vendor bytes.
// G29: 4-bit hat, 25 buttons, 3 padding bits, 16-bit steering,
// three 8-bit pedals, three vendor bytes. No report ID.
static inline int map_report(const uint8_t *src,size_t n,uint8_t dst[12],int swap_pedals) {
    if(!src || !dst || n!=34 || src[0]!=1) return 0;
    memset(dst,0,12);
    memcpy(dst,src+1,4);
    dst[3]&=0x1f; // exactly 25 buttons; never expose padding as buttons
    dst[4]=src[17];dst[5]=src[18];
    dst[6]=src[swap_pedals?22:20];
    dst[7]=src[swap_pedals?20:22];
    dst[8]=255; // standard two-pedal set: no clutch
    dst[9]=129;dst[10]=128;dst[11]=156;
    return 1;
}
#endif
