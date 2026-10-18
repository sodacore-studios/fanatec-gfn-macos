// SPDX-License-Identifier: GPL-2.0-only
// Experimental, process-local CSL DD -> G29 input bridge. OUTPUT CAPTURE ONLY.
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "g29_descriptor.h"
#include "report_map.h"
#include "ffb_wire.h"
#ifndef BRIDGE_LOG
#include "runtime_paths.h"
static char runtime_log[PATH_MAX];
#define BRIDGE_LOG runtime_log
#define DEFAULT_RUNTIME_LOG 1
#endif
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once=PTHREAD_ONCE_INIT;
static CFNumberRef vid,pid,version,in_size,out_size,zero,usage_page,usage;
static CFDataRef descriptor;
static CFArrayRef usages;
static _Atomic unsigned inputs,outputs,ignored;
static int logfile=-1;
static int force_socket=-1;
static struct sockaddr_un force_address;
static _Atomic unsigned force_forwarded;
static void log_line(const char *fmt,...) {
    if(logfile<0) return;
    char body[768],line[900];
    va_list ap;va_start(ap,fmt);vsnprintf(body,sizeof(body),fmt,ap);va_end(ap);
    int n=snprintf(line,sizeof(line),"%lld pid=%d %s\n",(long long)time(NULL),getpid(),body);
    if(n>0) write(logfile,line,(size_t)n<sizeof(line)?(size_t)n:sizeof(line)-1);
}
static CFNumberRef num(int n){return CFNumberCreate(NULL,kCFNumberIntType,&n);}
static void init_props(void) {
    vid=num(0x46d);pid=num(0xc24f);version=num(0x8900);
    in_size=num(12);out_size=num(16);zero=num(0);usage_page=num(1);usage=num(4);
    descriptor=CFDataCreate(NULL,g29_descriptor,sizeof(g29_descriptor));
    const void *keys[]={CFSTR("DeviceUsagePage"),CFSTR("DeviceUsage")};
    const void *values[]={usage_page,usage};
    CFDictionaryRef pair=CFDictionaryCreate(NULL,keys,values,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    const void *items[]={pair};usages=CFArrayCreate(NULL,items,1,&kCFTypeArrayCallBacks);CFRelease(pair);
}
static int number(CFTypeRef v) {
    int n=-1;
    if(v && CFGetTypeID(v)==CFNumberGetTypeID()) CFNumberGetValue(v,kCFNumberIntType,&n);
    return n;
}
// Calls originating inside this interposer resolve to the original IOKit APIs.
static int is_wheel(IOHIDDeviceRef d) {
    return d && number(IOHIDDeviceGetProperty(d,CFSTR("VendorID")))==0xeb7 &&
        number(IOHIDDeviceGetProperty(d,CFSTR("ProductID")))==0x20;
}
static CFTypeRef property(IOHIDDeviceRef d,CFStringRef key) {
    if(!is_wheel(d)) return IOHIDDeviceGetProperty(d,key);
    pthread_once(&once,init_props);
    if(CFEqual(key,CFSTR("VendorID"))) return vid;
    if(CFEqual(key,CFSTR("ProductID"))) return pid;
    if(CFEqual(key,CFSTR("VersionNumber"))) return version;
    if(CFEqual(key,CFSTR("Manufacturer"))) return CFSTR("Logitech");
    if(CFEqual(key,CFSTR("Product"))) return CFSTR("G29 Driving Force Racing Wheel");
    if(CFEqual(key,CFSTR("ReportDescriptor"))) {log_line("descriptor G29 bytes=%zu",sizeof(g29_descriptor));return descriptor;}
    if(CFEqual(key,CFSTR("MaxInputReportSize"))) return in_size;
    if(CFEqual(key,CFSTR("MaxOutputReportSize"))) return out_size;
    if(CFEqual(key,CFSTR("MaxFeatureReportSize"))) return zero;
    if(CFEqual(key,CFSTR("PrimaryUsagePage"))) return usage_page;
    if(CFEqual(key,CFSTR("PrimaryUsage"))) return usage;
    if(CFEqual(key,CFSTR("DeviceUsagePairs"))) return usages;
    return IOHIDDeviceGetProperty(d,key);
}
static CFDictionaryRef rewrite_match(CFDictionaryRef dict) {
    if(!dict) return NULL;
    int v=number(CFDictionaryGetValue(dict,CFSTR("VendorID")));
    int p=number(CFDictionaryGetValue(dict,CFSTR("ProductID")));
    if(v!=0x46d || (uint16_t)p!=0xc24f) return CFRetain(dict);
    CFMutableDictionaryRef copy=CFDictionaryCreateMutableCopy(NULL,0,dict);
    CFNumberRef a=num(0xeb7),b=num(0x20);
    CFDictionarySetValue(copy,CFSTR("VendorID"),a);CFDictionarySetValue(copy,CFSTR("ProductID"),b);
    CFRelease(a);CFRelease(b);log_line("match G29 -> CSL DD");return copy;
}
static void matching(IOHIDManagerRef m,CFDictionaryRef d) {
    CFDictionaryRef r=rewrite_match(d);IOHIDManagerSetDeviceMatching(m,r);if(r) CFRelease(r);
}
static void matching_multiple(IOHIDManagerRef m,CFArrayRef a) {
    if(!a){IOHIDManagerSetDeviceMatchingMultiple(m,a);return;}
    CFMutableArrayRef r=CFArrayCreateMutable(NULL,0,&kCFTypeArrayCallBacks);
    for(CFIndex i=0;i<CFArrayGetCount(a);i++) {
        CFDictionaryRef d=rewrite_match(CFArrayGetValueAtIndex(a,i));CFArrayAppendValue(r,d);CFRelease(d);
    }
    IOHIDManagerSetDeviceMatchingMultiple(m,r);CFRelease(r);
}
typedef struct Slot {
    IOHIDDeviceRef device;
    IOHIDReportCallback callback;
    void *context;
    uint8_t *caller_buffer;
    CFIndex caller_capacity;
    uint8_t raw[512];
    uint8_t latest[12];
    int have_latest;
    struct Slot *next;
} Slot;
// Slots survive process lifetime: avoids a late asynchronous callback using freed state.
static Slot *slots;
static Slot *slot_for(IOHIDDeviceRef d) {
    for(Slot *s=slots;s;s=s->next) if(s->device==d) return s;
    Slot *s=calloc(1,sizeof(*s));if(!s) return NULL;
    s->device=(IOHIDDeviceRef)CFRetain(d);s->next=slots;slots=s;return s;
}
static void report_callback(void *ctx,IOReturn result,void *sender,IOHIDReportType type,
                            uint32_t report_id,uint8_t *data,CFIndex length) {
    (void)sender;(void)report_id;
    Slot *s=ctx;uint8_t mapped[12];
    if(result!=kIOReturnSuccess || type!=kIOHIDReportTypeInput || length<0 ||
       !map_report(data,(size_t)length,mapped,0)){atomic_fetch_add(&ignored,1);return;}
    pthread_mutex_lock(&lock);
    memcpy(s->latest,mapped,12);s->have_latest=1;
    IOHIDReportCallback cb=s->callback;void *c=s->context;
    uint8_t *target=s->caller_buffer;CFIndex capacity=s->caller_capacity;
    if(target && capacity>=12) memcpy(target,mapped,12);
    pthread_mutex_unlock(&lock);
    unsigned count=atomic_fetch_add(&inputs,1)+1;
    if(count==1 || count%500==0) log_line("input count=%u steering=%u accelerator=%u brake=%u",count,
        (unsigned)mapped[4]+((unsigned)mapped[5]<<8),mapped[6],mapped[7]);
    if(cb && target && capacity>=12) cb(c,result,s->device,kIOHIDReportTypeInput,0,target,12);
}
static void register_input(IOHIDDeviceRef d,uint8_t *buf,CFIndex cap,IOHIDReportCallback cb,void *ctx) {
    if(!is_wheel(d)){IOHIDDeviceRegisterInputReportCallback(d,buf,cap,cb,ctx);return;}
    pthread_mutex_lock(&lock);Slot *s=slot_for(d);
    if(s){s->callback=cb;s->context=ctx;s->caller_buffer=buf;s->caller_capacity=cap;}
    pthread_mutex_unlock(&lock);
    if(!s) return;
    log_line("input callback registered capacity=%ld active=%d",cap,cb!=NULL);
    IOHIDDeviceRegisterInputReportCallback(d,s->raw,sizeof(s->raw),cb?report_callback:NULL,cb?s:NULL);
}
static IOReturn get_report(IOHIDDeviceRef d,IOHIDReportType t,CFIndex id,uint8_t *buf,CFIndex *len) {
    if(!is_wheel(d)) return IOHIDDeviceGetReport(d,t,id,buf,len);
    if(t==kIOHIDReportTypeInput && id==0 && buf && len && *len>=12) {
        pthread_mutex_lock(&lock);Slot *s=slot_for(d);int available=s&&s->have_latest;
        if(available){memcpy(buf,s->latest,12);*len=12;}pthread_mutex_unlock(&lock);
        return available?kIOReturnSuccess:kIOReturnNotReady;
    }
    log_line("GET_REPORT unsupported type=%d id=%ld requested=%ld",t,id,len?*len:-1);
    return kIOReturnUnsupported;
}
static IOReturn capture_output(IOHIDDeviceRef d,IOHIDReportType t,CFIndex id,const uint8_t *b,CFIndex n) {
    if(!is_wheel(d)) return IOHIDDeviceSetReport(d,t,id,b,n);
    unsigned count=atomic_fetch_add(&outputs,1)+1;
    // Local optional receiver defaults to dry-run. This process never writes the motor.
    if(force_socket>=0 && t==kIOHIDReportTypeOutput && id==0 && n==16 && b) {
        ForcePacket packet={.magic=FFB_MAGIC,.pid=(uint32_t)getpid(),.time_ns=monotonic_ns(),
            .type=(uint32_t)t,.report_id=(uint32_t)id,.length=16};
        memcpy(packet.bytes,b,16);
        if(sendto(force_socket,&packet,sizeof(packet),0,(struct sockaddr *)&force_address,sizeof(force_address))==sizeof(packet)) {
            if(atomic_fetch_add(&force_forwarded,1)==0)log_line("local FFB receiver connected; app remains capture-only");
        }
    }
    // Preserve shutdown evidence even after periodic force logging is sampled.
    int stop_packet=b && n==16 && (b[0]==0x13 || b[0]==0xf3 || b[0]==0xf5);
    if(count<=256 || count%100==0 || stop_packet) {
        char hex[129]={0};CFIndex end=n>64?64:n;
        for(CFIndex i=0;i<end;i++) snprintf(hex+i*2,3,"%02x",b[i]);
        log_line("OUTPUT_CAPTURE_ONLY count=%u type=%d id=%ld length=%ld bytes=%s",count,t,id,n,hex);
    }
    // Acknowledge receipt for protocol observation; NEVER forward to the physical wheel.
    return kIOReturnSuccess;
}
static IOReturn capture_async(IOHIDDeviceRef d,IOHIDReportType t,CFIndex id,const uint8_t *b,CFIndex n,
        CFTimeInterval timeout,IOHIDReportCallback cb,void *ctx) {
    if(!is_wheel(d)) return IOHIDDeviceSetReportWithCallback(d,t,id,b,n,timeout,cb,ctx);
    IOReturn result=capture_output(d,t,id,b,n);
    if(cb) cb(ctx,result,d,t,(uint32_t)id,(uint8_t *)b,n);
    return result;
}
#define INTERPOSE(replacement,original) \
__attribute__((used,section("__DATA,__interpose"))) static const struct { \
 const void *replacement,*original; } pair_##original = { \
 (const void *)&replacement,(const void *)&original };
INTERPOSE(property,IOHIDDeviceGetProperty)
INTERPOSE(matching,IOHIDManagerSetDeviceMatching)
INTERPOSE(matching_multiple,IOHIDManagerSetDeviceMatchingMultiple)
INTERPOSE(register_input,IOHIDDeviceRegisterInputReportCallback)
INTERPOSE(get_report,IOHIDDeviceGetReport)
INTERPOSE(capture_output,IOHIDDeviceSetReport)
INTERPOSE(capture_async,IOHIDDeviceSetReportWithCallback)
__attribute__((constructor)) static void setup(void) {
#ifdef DEFAULT_RUNTIME_LOG
    if(!runtime_path(runtime_log,sizeof(runtime_log),"bridge.log"))return;
#endif
    logfile=open(BRIDGE_LOG,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);
    force_address.sun_family=AF_UNIX;
    const char *slash=strrchr(BRIDGE_LOG,'/');
    size_t prefix=slash?(size_t)(slash-BRIDGE_LOG+1):0;
    if(prefix+sizeof("ffb.sock")<=sizeof(force_address.sun_path)) {
        memcpy(force_address.sun_path,BRIDGE_LOG,prefix);strcpy(force_address.sun_path+prefix,"ffb.sock");
        force_socket=socket(AF_UNIX,SOCK_DGRAM,0);
        if(force_socket>=0){fcntl(force_socket,F_SETFL,O_NONBLOCK);fcntl(force_socket,F_SETFD,FD_CLOEXEC);}
    }
    log_line("bridge loaded OUTPUT_CAPTURE_ONLY version=0.2 optional local force receiver");
}
void probe_print_hooks(void) {
    printf("bridge input=%u output_captured=%u ignored=%u; physical outputs disabled\n",
        atomic_load(&inputs),atomic_load(&outputs),atomic_load(&ignored));
}
