// Exercise the real receiver output path with a replacement USB transport.
// No HID device is opened; every motor write terminates in this test process.
#define IOHIDDeviceSetReport capture_test_report
#define main receiver_program_main
#include "../source/ffb_receiver.c"
#undef main
#include <assert.h>
static uint8_t writes[16][8];
static unsigned write_count;
IOReturn capture_test_report(IOHIDDeviceRef device,IOHIDReportType type,CFIndex id,const uint8_t *data,CFIndex length){
    assert(device==wheel && type==kIOHIDReportTypeOutput && id==1 && length==8);
    assert(write_count<16);memcpy(writes[write_count++],data,8);
    return kIOReturnSuccess;
}
int main(void){
    // A sentinel is safe because the only API that receives it is the stub above.
    wheel=(IOHIDDeviceRef)(uintptr_t)1;
    const uint8_t expected[3][8]={
        {1,1,8,0,128,0,0,1}, {1,3,8,0,0,0,0,0}, {1,243,0,0,0,0,0,0}
    };
    for(int direction=-1;direction<=1;direction+=2){
        uint8_t force[8];assert(ftec_constant(direction*.5,1,force));
        write_count=0;write_motor(force);stop_motor();
        assert(write_count==4);
        assert(!memcmp(writes+1,expected,sizeof(expected)));
        assert(!failed && !quit_requested);
    }
    const uint8_t game_stops[4][16]={
        {0x11,8,128,128}, {0x13}, {0xf3}, {0xf5}
    };
    for(unsigned i=0;i<4;i++){
        uint8_t translated[8];assert(translate_force(game_stops[i],16,1,0,translated));
        write_count=0;output_force(translated);
        assert(write_count==3 && !memcmp(writes,expected,sizeof(expected)));
    }
    uint8_t active_force[8];assert(ftec_constant(.5,1,active_force));
    write_count=0;output_force(active_force);
    assert(write_count==1 && !memcmp(writes[0],active_force,8));
    wheel=NULL;
    puts("PASS: both force directions are neutralized, slot 0 disabled, then globally stopped");
}
