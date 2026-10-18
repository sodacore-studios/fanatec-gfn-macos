#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "device_profiles.h"
#include "ffb_protocol.h"
int main(void){
    assert(!ftec_profile(0x46d,0x20));assert(!ftec_profile(0xeb7,0xffff));
    for(size_t i=0;i<FTEC_PROFILE_COUNT;i++){
        const FtecProfile *p=ftec_profile(0xeb7,ftec_profiles[i].pid);assert(p==&ftec_profiles[i]);
        assert(p->highres==(p->pid!=1 && p->pid!=4));
        for(int input=0;input<256;input++){
            uint8_t command[16]={0x11,8,(uint8_t)input,128},out[8];
            assert(translate_force(command,16,1,0,out));
            uint8_t original[8];memcpy(original,out,8);ftec_encode_resolution(out,p->highres);
            if(p->highres || input==128)assert(!memcmp(out,original,8));
            else {assert(out[3]==original[4] && out[4]==0 && out[7]==0);}
        }
        uint8_t stops[3][8];ftec_stop_sequence(stops);
        for(unsigned k=0;k<3;k++)ftec_encode_resolution(stops[k],p->highres);
        assert(stops[0][p->highres?4:3]==128 && stops[1][1]==3 && stops[2][1]==243);
    }
    assert(ftec_output_format(8,7,0));assert(ftec_output_format(56,7,1));
    assert(!ftec_output_format(16,7,1));assert(!ftec_output_format(56,6,1));assert(!ftec_output_format(56,7,2));
    assert(ftec_scale(0,-32768,32767,65535)==32768);
    assert(ftec_scale(-999,0,65535,255)==0);assert(ftec_scale(999999,0,65535,255)==255);
    assert(ftec_scale(3,3,3,255)==0);assert(ftec_scale(0,4,3,255)==0);
    assert(ftec_scale(32768,0,65535,255)==127);
    puts("PASS: all seven profiles, every force level, legacy neutral/stop and signed HID scaling");
}
