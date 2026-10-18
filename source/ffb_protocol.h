// SPDX-License-Identifier: GPL-2.0-only
// CSL DD protocol reference: gotzl/hid-fanatecff, ftecff_update_slot.
// GFN G29 constant force: 11 08 level8 80 00 00 00 (observed in ACC).
// Only constant force is translated. Configuration/LED/condition packets are NOT forwarded.
#ifndef FFB_PROTOCOL_H
#define FFB_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
static inline void ftec_stop(uint8_t out[8]) {
    memset(out,0,8);out[0]=1;out[1]=0xf3;
}
static inline int ftec_constant(double normalized,double limit,uint8_t out[8]) {
    if(!out||!isfinite(normalized)||!isfinite(limit)||limit<=0||limit>1.0) return 0;
    if(normalized>limit) normalized=limit;
    if(normalized< -limit) normalized= -limit;
    if(fabs(normalized)<0.0001){ftec_stop(out);return 1;}
    int signed_force=(int)lrint(normalized*32767.0);
    uint16_t level=(uint16_t)(signed_force+32768);
    memset(out,0,8);
    out[0]=1;out[1]=0x01;out[2]=0x08;
    out[3]=level&255;out[4]=level>>8;out[7]=1;
    return 1;
}
// Return 1: recognized force/stop. Return 0: unsupported; never send these bytes.
static inline int translate_force(const uint8_t *packet,size_t n,double gain,int invert,uint8_t out[8]) {
    if(!packet || !out || n!=16 || !isfinite(gain)||gain<=0||gain>1.0) return 0;
    for(size_t i=7;i<16;i++) if(packet[i]) return 0;
    if(packet[0]==0xf3 || packet[0]==0x13 || packet[0]==0xf5) {ftec_stop(out);return 1;}
    if(packet[0]!=0x11||packet[1]!=0x08||packet[3]!=0x80||packet[4]||packet[5]||packet[6]) return 0;
    double force=((int)packet[2]-128)/128.0*gain;
    return ftec_constant(invert?-force:force,gain,out);
}
// Explicit 30-second test: 5s at 10%, 10s ramp, then 15s at 50%.
static inline double ffb_ramp_gain(uint64_t elapsed_ms,double target) {
    double initial=fmin(target,0.10);
    if(elapsed_ms<=5000) return initial;
    if(elapsed_ms>=15000) return target;
    return initial+(target-initial)*(double)(elapsed_ms-5000)/10000.0;
}
static inline double ffb_ramp50_gain(uint64_t elapsed_ms) {
    return ffb_ramp_gain(elapsed_ms,0.50);
}
#endif
