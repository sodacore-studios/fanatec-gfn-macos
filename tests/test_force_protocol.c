#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "ffb_protocol.h"
static int force(const uint8_t b[8]){return ((int)b[3]+((int)b[4]<<8))-32768;}
int main(void){
    uint8_t stops[3][8];ftec_stop_sequence(stops);
    const uint8_t expected[3][8]={
        {1,1,8,0,128,0,0,1}, {1,3,8,0,0,0,0,0}, {1,243,0,0,0,0,0,0}
    };
    assert(!memcmp(stops,expected,sizeof(expected)));
    uint8_t p[16]={0x11,0x08,0x80,0x80},out[8];
    assert(translate_force(p,16,0.03,0,out)&&out[1]==0x03);
    p[2]=255;assert(translate_force(p,16,0.03,0,out));
    assert(out[0]==1 && out[1]==1 && out[2]==8 && out[7]==1);
    assert(force(out)>0 && force(out)<=984);
    p[2]=0;assert(translate_force(p,16,0.03,0,out)&&force(out)>=-984 && force(out)<0);
    assert(translate_force(p,16,0.03,1,out)&&force(out)>0);
    assert(!translate_force(p,15,0.03,0,out));
    p[15]=1;assert(!translate_force(p,16,0.03,0,out));p[15]=0;
    p[1]=0x0c;assert(!translate_force(p,16,0.03,0,out));
    p[0]=0xf8;p[1]=0x12;assert(!translate_force(p,16,0.03,0,out));
    p[0]=0x13;assert(translate_force(p,16,0.03,0,out)&&out[1]==0x03);
    assert(!translate_force(p,16,1.01,0,out));
    assert(!ftec_constant(NAN,0.03,out));assert(!ftec_constant(1,NAN,out));
    assert(ftec_constant(1,0.03,out)&&force(out)<=984);
    assert(ffb_ramp50_gain(0)==0.10 && ffb_ramp50_gain(5000)==0.10);
    assert(fabs(ffb_ramp50_gain(10000)-0.30)<1e-9);
    assert(ffb_ramp50_gain(15000)==0.50 && ffb_ramp50_gain(UINT64_MAX)==0.50);
    for(unsigned t=0;t<=30000;t+=100){
        double g=ffb_ramp50_gain(t);assert(g>=0.10 && g<=0.50);
        for(unsigned v=0;v<256;v++){
            memset(p,0,16);p[0]=0x11;p[1]=0x08;p[2]=(uint8_t)v;p[3]=0x80;
            assert(translate_force(p,16,g,0,out));
            if(out[1]!=0x03)assert(abs(force(out))<=16384);
        }
    }
    for(unsigned t=0;t<=30000;t+=100){
        double g=ffb_ramp_gain(t,1.0);assert(g>=0.10 && g<=1.0);
        for(unsigned v=0;v<256;v++){
            memset(p,0,16);p[0]=0x11;p[1]=0x08;p[2]=(uint8_t)v;p[3]=0x80;
            assert(translate_force(p,16,g,0,out));
            if(out[1]!=0x03)assert(abs(force(out))<=32767);
        }
    }
    assert(ffb_ramp_gain(15000,1.0)==1.0 && ffb_ramp_gain(UINT64_MAX,1.0)==1.0);
    assert(ffb_ramp_gain(0,0.03)==0.03 && ffb_ramp_gain(15000,0.03)==0.03);
    assert(!ftec_constant(1,0,out));
    assert(!ftec_constant(1,1.01,out));
    assert(!ftec_constant(1,1,NULL));
    assert(ftec_constant(-1,0.1,out) && force(out)<0);
    memset(p,0,16);p[0]=0x11;p[1]=8;p[3]=128;
    assert(!translate_force(NULL,16,1,0,out));
    assert(!translate_force(p,16,1,0,NULL));
    assert(!translate_force(p,16,NAN,0,out));
    assert(!translate_force(p,16,0,0,out));
    for(int i=3;i<=6;i++) {
        uint8_t original=p[i];p[i]=1;
        assert(!translate_force(p,16,1,0,out));p[i]=original;
    }
    for(int i=7;i<16;i++) {p[i]=1;assert(!translate_force(p,16,1,0,out));p[i]=0;}
    p[0]=0xf3;assert(translate_force(p,16,1,0,out) && out[1]==0x03);
    p[0]=0xf5;assert(translate_force(p,16,1,0,out) && out[1]==0x03);
    puts("PASS: stop, signs, malformed data, 50%/100% limits and every input level across ramps");
}
