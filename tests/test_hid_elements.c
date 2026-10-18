#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <assert.h>
#include <stdio.h>
typedef struct {IOHIDElementType type;unsigned page,usage,id,bits,count;CFIndex min,max,value;int relative,unavailable;} TestElement;
static CFArrayRef fixture;
static CFNumberRef vendor,product,usage_page,device_usage;
static CFTypeRef test_property(IOHIDDeviceRef d,CFStringRef key){
    (void)d;
    if(CFEqual(key,CFSTR("VendorID")))return vendor;
    if(CFEqual(key,CFSTR("ProductID")))return product;
    if(CFEqual(key,CFSTR("PrimaryUsagePage")))return usage_page;
    if(CFEqual(key,CFSTR("PrimaryUsage")))return device_usage;
    return NULL;
}
static CFArrayRef test_elements(IOHIDDeviceRef d,CFDictionaryRef m,IOOptionBits o){
    (void)d;(void)m;(void)o;return fixture?CFRetain(fixture):NULL;
}
static IOReturn test_value(IOHIDDeviceRef d,IOHIDElementRef e,IOHIDValueRef *out){
    (void)d;TestElement *item=(TestElement *)e;
    if(item->unavailable)return kIOReturnError;
    *out=(IOHIDValueRef)e;return kIOReturnSuccess;
}
#define IOHIDDeviceGetProperty test_property
#define IOHIDDeviceCopyMatchingElements test_elements
#define IOHIDDeviceGetValue test_value
#define IOHIDValueGetIntegerValue(v) (((TestElement *)(v))->value)
#define IOHIDElementGetType(e) (((TestElement *)(e))->type)
#define IOHIDElementGetUsagePage(e) (((TestElement *)(e))->page)
#define IOHIDElementGetUsage(e) (((TestElement *)(e))->usage)
#define IOHIDElementGetReportID(e) (((TestElement *)(e))->id)
#define IOHIDElementGetReportSize(e) (((TestElement *)(e))->bits)
#define IOHIDElementGetReportCount(e) (((TestElement *)(e))->count)
#define IOHIDElementGetLogicalMin(e) (((TestElement *)(e))->min)
#define IOHIDElementGetLogicalMax(e) (((TestElement *)(e))->max)
#define IOHIDElementIsRelative(e) (((TestElement *)(e))->relative)
#include "fanatec_device.h"
static CFNumberRef number(int n){return CFNumberCreate(NULL,kCFNumberIntType,&n);}
static void replace_number(CFNumberRef *target,int value){CFRelease(*target);*target=number(value);}
static FtecElements layout(TestElement *items,size_t n){
    const void *pointers[16];assert(n<=16);
    for(size_t i=0;i<n;i++)pointers[i]=&items[i];
    if(fixture)CFRelease(fixture);
    fixture=CFArrayCreate(NULL,pointers,n,NULL);
    return ftec_elements((IOHIDDeviceRef)1);
}
int main(void){
    IOHIDDeviceRef d=(IOHIDDeviceRef)1;
    vendor=number(0xeb7);product=number(0x6);usage_page=number(1);device_usage=number(4);
    assert(ftec_device_profile(d)->pid==6);assert(!ftec_device_profile(NULL));
    replace_number(&device_usage,5);assert(ftec_device_profile(d));
    replace_number(&device_usage,8);assert(ftec_device_profile(d));
    replace_number(&device_usage,1);assert(!ftec_device_profile(d));replace_number(&device_usage,4);
    replace_number(&usage_page,0xff00);assert(!ftec_device_profile(d));replace_number(&usage_page,1);
    assert(ftec_number(NULL)==-1);assert(ftec_number(CFSTR("invalid"))==-1);
    FtecElements empty=ftec_elements(d);assert(!empty.valid && empty.output_id==-1);
    TestElement items[]={
        {kIOHIDElementTypeInput_Axis,1,0x30,1,16,1,0,65535,32768,0,0},
        {kIOHIDElementTypeInput_Axis,1,0x32,1,16,1,0,65535,65535,0,0},
        {kIOHIDElementTypeInput_Axis,1,0x35,1,16,1,0,65535,0,0,0},
        {kIOHIDElementTypeInput_Axis,1,0x31,1,16,1,0,65535,65535,0,0},
        {kIOHIDElementTypeInput_Misc,1,0x39,1,4,1,0,7,8,0,0},
        {kIOHIDElementTypeInput_Button,9,1,1,1,1,0,1,1,0,0},
        {kIOHIDElementTypeOutput,0xff00,2,1,56,7,0,255,0,0,0}
    };
    FtecElements e=layout(items,7);uint8_t mapped[12];
    assert(e.valid && e.output_id==1 && ftec_map_elements(d,&e,mapped));
    assert(mapped[0]==24 && mapped[4]==0 && mapped[5]==128 && mapped[6]==255 && mapped[7]==0 && mapped[8]==255);
    items[4].value=3;items[5].value=0;assert(ftec_map_elements(d,&e,mapped)&&mapped[0]==3);
    items[0].min=-32768;items[0].max=32767;items[0].value=-32768;
    assert(ftec_map_elements(d,&e,mapped)&&mapped[4]==0&&mapped[5]==0);
    for(unsigned i=0;i<4;i++){items[i].unavailable=1;assert(!ftec_map_elements(d,&e,mapped));items[i].unavailable=0;}
    e.clutch=NULL;e.hat=NULL;e.buttons[0]=NULL;assert(ftec_map_elements(d,&e,mapped)&&mapped[8]==255&&mapped[0]==8);
    e.valid=0;assert(!ftec_map_elements(d,&e,mapped));CFRelease(e.elements);
    // Ambiguous/relative/missing axes and invalid ranges cannot become a profile.
    items[3].usage=0x30;e=layout(items,7);assert(!e.valid);CFRelease(e.elements);items[3].usage=0x31;
    items[0].relative=1;e=layout(items,7);assert(!e.valid);CFRelease(e.elements);items[0].relative=0;
    items[0].id=2;e=layout(items,7);assert(!e.valid);CFRelease(e.elements);items[0].id=1;
    items[0].max=items[0].min;e=layout(items,7);assert(!e.valid);CFRelease(e.elements);items[0].max=32767;
    items[0].bits=64;e=layout(items,7);assert(!e.valid);CFRelease(e.elements);items[0].bits=16;
    items[6].id=0;e=layout(items,7);assert(e.output_id==0);CFRelease(e.elements);
    items[6].id=255;e=layout(items,7);assert(e.output_id==-1);CFRelease(e.elements);
    CFRelease(fixture);CFRelease(vendor);CFRelease(product);CFRelease(usage_page);CFRelease(device_usage);
    puts("PASS: HID descriptor selection, axes/pedals/buttons, missing/ambiguous fields and read failures");
}
