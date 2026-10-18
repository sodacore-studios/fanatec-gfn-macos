// SPDX-License-Identifier: GPL-2.0-only
#ifndef FANATEC_DEVICE_H
#define FANATEC_DEVICE_H
#include <IOKit/hid/IOHIDManager.h>
#include <string.h>
#include "device_profiles.h"
static int ftec_number(CFTypeRef value){
    int n=-1;
    if(value && CFGetTypeID(value)==CFNumberGetTypeID())CFNumberGetValue(value,kCFNumberIntType,&n);
    return n;
}
static const FtecProfile *ftec_device_profile(IOHIDDeviceRef d){
    if(!d)return NULL;
    int page=ftec_number(IOHIDDeviceGetProperty(d,CFSTR("PrimaryUsagePage")));
    int usage=ftec_number(IOHIDDeviceGetProperty(d,CFSTR("PrimaryUsage")));
    if(page!=1 || (usage!=4 && usage!=5 && usage!=8))return NULL;
    return ftec_profile(ftec_number(IOHIDDeviceGetProperty(d,CFSTR("VendorID"))),
                        ftec_number(IOHIDDeviceGetProperty(d,CFSTR("ProductID"))));
}
typedef struct {
    CFArrayRef elements;
    IOHIDElementRef steering,gas,brake,clutch,hat,buttons[25];
    int valid,output_id;
} FtecElements;
static int ftec_input_element(IOHIDElementRef e){
    IOHIDElementType t=IOHIDElementGetType(e);
    return t==kIOHIDElementTypeInput_Misc || t==kIOHIDElementTypeInput_Button || t==kIOHIDElementTypeInput_Axis;
}
static FtecElements ftec_elements(IOHIDDeviceRef d){
    FtecElements r={.output_id=-1};
    r.elements=IOHIDDeviceCopyMatchingElements(d,NULL,0);
    if(!r.elements)return r;
    int duplicate=0;
    for(CFIndex i=0;i<CFArrayGetCount(r.elements);i++){
        IOHIDElementRef e=(IOHIDElementRef)CFArrayGetValueAtIndex(r.elements,i);
        uint32_t page=IOHIDElementGetUsagePage(e),usage=IOHIDElementGetUsage(e),id=IOHIDElementGetReportID(e);
        // The driver writes seven payload bytes to the primary output report.
        if(IOHIDElementGetType(e)==kIOHIDElementTypeOutput && page>=0xff00 &&
           ftec_output_format(IOHIDElementGetReportSize(e),IOHIDElementGetReportCount(e),id)){
            if(r.output_id<0 || id==1)r.output_id=(int)id;
        }
        // Report 1 is the primary wheel report. ID 0 covers unnumbered legacy HID.
        if(!ftec_input_element(e) || id>1 || IOHIDElementIsRelative(e))continue;
        IOHIDElementRef *field=NULL;
        if(page==1){
            switch(usage){
                case 0x30:field=&r.steering;break;
                case 0x32:field=&r.gas;break;
                case 0x35:field=&r.brake;break;
                case 0x31:field=&r.clutch;break;
                case 0x39:field=&r.hat;break;
            }
        }else if(page==9 && usage>=1 && usage<=25)field=&r.buttons[usage-1];
        if(field){if(*field)duplicate=1;else *field=e;}
    }
    r.valid=!duplicate && r.steering && r.gas && r.brake;
    IOHIDElementRef axes[]={r.steering,r.gas,r.brake,r.clutch};
    for(unsigned i=0;i<4;i++)if(axes[i] &&
       (IOHIDElementGetLogicalMax(axes[i])<=IOHIDElementGetLogicalMin(axes[i]) ||
        IOHIDElementGetReportSize(axes[i])>32))r.valid=0;
    return r;
}
static int ftec_value(IOHIDDeviceRef d,IOHIDElementRef e,int64_t *out){
    IOHIDValueRef v=NULL;
    if(!e || IOHIDDeviceGetValue(d,e,&v)!=kIOReturnSuccess || !v)return 0;
    *out=IOHIDValueGetIntegerValue(v);return 1;
}
static int ftec_axis(IOHIDDeviceRef d,IOHIDElementRef e,uint32_t scale,uint32_t *out){
    int64_t value;if(!ftec_value(d,e,&value))return 0;
    *out=ftec_scale(value,IOHIDElementGetLogicalMin(e),IOHIDElementGetLogicalMax(e),scale);return 1;
}
static int ftec_map_elements(IOHIDDeviceRef d,const FtecElements *e,uint8_t out[12]){
    if(!e->valid)return 0;
    uint32_t steering,gas,brake,clutch=255;
    if(!ftec_axis(d,e->steering,65535,&steering) || !ftec_axis(d,e->gas,255,&gas) ||
       !ftec_axis(d,e->brake,255,&brake))return 0;
    if(e->clutch && !ftec_axis(d,e->clutch,255,&clutch))return 0;
    memset(out,0,12);out[0]=8;
    int64_t value;
    if(ftec_value(d,e->hat,&value)){
        value-=IOHIDElementGetLogicalMin(e->hat);
        if(value>=0 && value<8)out[0]=(uint8_t)value;
    }
    for(unsigned i=0;i<25;i++)if(ftec_value(d,e->buttons[i],&value) && value){
        unsigned bit=4+i;out[bit/8]|=(uint8_t)(1u<<(bit%8));
    }
    out[4]=steering&255;out[5]=steering>>8;out[6]=gas;out[7]=brake;out[8]=clutch;
    out[9]=129;out[10]=128;out[11]=156;return 1;
}
#endif
