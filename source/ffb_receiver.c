// SPDX-License-Identifier: GPL-2.0-only
// Default is DRY RUN. Physical output requires an explicit --live or --pulse-test.
// Only constant-force packets are supported; all other effects are ignored.
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "ffb_protocol.h"
#include "ffb_wire.h"
#include "fanatec_device.h"
#ifndef FFB_SOCKET
#include "runtime_paths.h"
static char runtime_socket[104];
#define FFB_SOCKET runtime_socket
#define DEFAULT_RUNTIME_SOCKET 1
#endif
static volatile sig_atomic_t quit_requested;
static IOHIDManagerRef manager;
static IOHIDDeviceRef wheel;
static int failed;
static int output_id=1,highres=1,allow_experimental;
static void stop_signal(int n){(void)n;quit_requested=1;}
static IOReturn write_motor(const uint8_t packet[8]){
    if(!wheel) return kIOReturnSuccess;
    uint8_t encoded[8];memcpy(encoded,packet,8);ftec_encode_resolution(encoded,highres);
    encoded[0]=(uint8_t)output_id;
    IOReturn r=IOHIDDeviceSetReport(wheel,kIOHIDReportTypeOutput,output_id,
                                  encoded+(output_id==0),output_id==0?7:8);
    if(r!=kIOReturnSuccess){fprintf(stderr,"motor write failed 0x%08x; stopping\n",r);failed=1;quit_requested=1;}
    return r;
}
static void stop_motor(void){
    uint8_t packets[3][8];ftec_stop_sequence(packets);
    for(int i=0;i<3;i++)write_motor(packets[i]);
}
static void output_force(const uint8_t packet[8]){
    if(packet[1]==0x03)stop_motor();else write_motor(packet);
}
static int open_wheel(int diagnose){
    int v=0xeb7;CFNumberRef vid=CFNumberCreate(NULL,kCFNumberIntType,&v);
    const void *key=CFSTR("VendorID"),*value=vid;
    CFDictionaryRef match=CFDictionaryCreate(NULL,&key,&value,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    manager=IOHIDManagerCreate(NULL,0);IOHIDManagerSetDeviceMatching(manager,match);
    CFRelease(match);CFRelease(vid);
    IOReturn r=IOHIDManagerOpen(manager,0);CFSetRef devices=IOHIDManagerCopyDevices(manager);
    unsigned count=0;IOHIDDeviceRef selected=NULL;
    if(!r && devices){
        CFIndex n=CFSetGetCount(devices);const void **items=calloc((size_t)n+1,sizeof(void *));
        if(!items){CFRelease(devices);return 0;}CFSetGetValues(devices,items);
        for(CFIndex i=0;i<n;i++){
            IOHIDDeviceRef d=(IOHIDDeviceRef)items[i];const FtecProfile *p=ftec_device_profile(d);
            if(p){count++;selected=d;}
            if(diagnose){
                FtecElements e=ftec_elements(d);
                printf("USB %04x:%04x profile=%s input_layout=%s output_report=%d force_bits=%d\n",
                    0xeb7,ftec_number(IOHIDDeviceGetProperty(d,CFSTR("ProductID"))),p?p->name:"unsupported interface",
                    e.valid?"X/Z/Rz available":"unverified or ambiguous",e.output_id,p?(p->highres?16:8):0);
                if(e.elements)for(CFIndex k=0;k<CFArrayGetCount(e.elements);k++){
                    IOHIDElementRef item=(IOHIDElementRef)CFArrayGetValueAtIndex(e.elements,k);
                    if(IOHIDElementGetType(item)==kIOHIDElementTypeOutput)
                        printf("output report=%u page=%04x usage=%04x bits=%u count=%u\n",IOHIDElementGetReportID(item),IOHIDElementGetUsagePage(item),IOHIDElementGetUsage(item),IOHIDElementGetReportSize(item),IOHIDElementGetReportCount(item));
                }
                CFTypeRef desc=IOHIDDeviceGetProperty(d,CFSTR("ReportDescriptor"));
                if(desc && CFGetTypeID(desc)==CFDataGetTypeID() && CFDataGetLength(desc)<=4096){
                    printf("descriptor_hex=");for(CFIndex j=0;j<CFDataGetLength(desc);j++)printf("%02x",CFDataGetBytePtr(desc)[j]);puts("");
                }
                uint8_t mapped[12];
                if(ftec_map_elements(d,&e,mapped))printf("snapshot steering=%u gas=%u brake=%u clutch=%u\n",mapped[4]+256u*mapped[5],mapped[6],mapped[7],mapped[8]);
                if(e.elements)CFRelease(e.elements);
            }
        }
        free(items);
        if(!diagnose && count==1){
            FtecElements e=ftec_elements(selected);const FtecProfile *p=ftec_device_profile(selected);
            if(p->pid!=0x0020 && !allow_experimental){
                fprintf(stderr,"Experimental base: explicitly enable community testing with --allow-experimental; begin at 3%%.\n");
            }else if(e.valid && e.output_id>=0){
                output_id=e.output_id;highres=p->highres;wheel=(IOHIDDeviceRef)CFRetain(selected);
                printf("device=%s USB=0eb7:%04x force_bits=%d output_report=%d\n",p->name,p->pid,highres?16:8,output_id);
            }
            if(e.elements)CFRelease(e.elements);
        }
    }
    if(devices)CFRelease(devices);
    if(diagnose){printf("diagnostic only: no motor writes; candidate bases=%u\n",count);}
    if(diagnose || !wheel){IOHIDManagerClose(manager,0);CFRelease(manager);manager=NULL;}
    if(!diagnose && !wheel)fprintf(stderr,"Need exactly one known Fanatec base with unambiguous X/Z/Rz inputs and a seven-byte output report (found %u, status 0x%08x). Run --diagnose.\n",count,r);
    return diagnose?r==0:wheel!=NULL;
}
int main(int argc,char **argv){
#ifdef DEFAULT_RUNTIME_SOCKET
    if(!runtime_path(runtime_socket,sizeof(runtime_socket),"ffb.sock")){fprintf(stderr,"Cannot resolve runtime socket path\n");return 3;}
#endif
    int diagnose=0,live=0,pulse=0,invert=0,ramp50=0,session=0,parent_pid=0,seconds=30;double gain=0.03;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--diagnose"))diagnose=1;
        else if(!strcmp(argv[i],"--allow-experimental"))allow_experimental=1;
        else if(!strcmp(argv[i],"--live"))live=1;
        else if(!strcmp(argv[i],"--ramp-50"))ramp50=1;
        else if(!strcmp(argv[i],"--session"))session=1;
        else if(!strcmp(argv[i],"--parent-pid")&&i+1<argc){
            parent_pid=atoi(argv[++i]);
            if(parent_pid<=1||parent_pid!=getppid()){fprintf(stderr,"Parent must be the process that launched this receiver\n");return 2;}
        }
        else if(!strcmp(argv[i],"--pulse-test")){live=1;pulse=1;seconds=4;}
        else if(!strcmp(argv[i],"--invert"))invert=1;
        else if(!strcmp(argv[i],"--seconds")&&i+1<argc){
            char *end;const char *arg=argv[++i];long parsed=strtol(arg,&end,10);
            if(!*arg||*end||parsed<0||parsed>3600){fprintf(stderr,"Invalid duration\n");return 2;}
            seconds=(int)parsed;
        }
        else if(!strcmp(argv[i],"--gain")&&i+1<argc){
            char *end; const char *arg=argv[++i]; gain=strtod(arg,&end);
            if(!*arg||*end){fprintf(stderr,"Invalid gain\n");return 2;}
        }
        else {fprintf(stderr,"Usage: %s [--diagnose] | [--allow-experimental] [--live | --pulse-test] [--gain 0.03] [--seconds 30] [--invert] | --ramp-50 [--live] [--invert]\n",argv[0]);return 2;}
    }
    if(!isfinite(gain)||gain<=0||gain>(session?1.0:0.10)||seconds<(session?0:1)||seconds>(session?3600:120)){
        fprintf(stderr,"Test: gain (0,0.10], duration 1..120s; --session: gain (0,1.0], duration 0..3600s (0=unlimited)\n");return 2;
    }
    if(session&&(pulse||ramp50)){fprintf(stderr,"Session cannot combine with pulse/ramp test\n");return 2;}
    if(ramp50){
        if(pulse||seconds!=30||gain!=0.03){fprintf(stderr,"--ramp-50 requires default gain/duration and no pulse test\n");return 2;}
        gain=0.50;
    }
    // A receiver must never inherit the bridge: it would turn physical writes back into captures.
    if(getenv("DYLD_INSERT_LIBRARIES")){fprintf(stderr,"Refusing injected environment\n");return 2;}
    if(diagnose){
        if(argc!=2){fprintf(stderr,"--diagnose must be used alone; it never writes to the motor\n");return 2;}
        return open_wheel(1)?0:4;
    }
    setbuf(stdout,NULL);signal(SIGINT,stop_signal);signal(SIGTERM,stop_signal);signal(SIGHUP,stop_signal);
    int fd=-1;
    if(!pulse){
        if(access(FFB_SOCKET,F_OK)==0){fprintf(stderr,"Socket already exists; another receiver may be running\n");return 2;}
        fd=socket(AF_UNIX,SOCK_DGRAM,0);if(fd<0)return 3;
        struct sockaddr_un addr={0};addr.sun_family=AF_UNIX;
        if(strlen(FFB_SOCKET)>=sizeof(addr.sun_path))return 3;
        strcpy(addr.sun_path,FFB_SOCKET);
        mode_t old=umask(077);int r=bind(fd,(struct sockaddr *)&addr,sizeof(addr));umask(old);
        if(r){perror("bind");close(fd);return 3;}
        fcntl(fd,F_SETFL,O_NONBLOCK);
    }
    if(live && !open_wheel(0)){if(fd>=0){close(fd);unlink(FFB_SOCKET);}return 4;}
    printf("mode=%s gain=%.3f duration=%ds watchdog=250ms CONSTANT_FORCE_ONLY\n",live?"LIVE":"DRY_RUN",gain,seconds);
    if(ramp50)puts("ramp: 0..5s=10%, 5..15s=10->50%, 15..30s=50%, then stop");
    if(session)printf("session: immediate %.0f%% passthrough, duration=%s\n",gain*100,seconds?"bounded":"unlimited until stop");
    if(live)stop_motor();
    uint64_t start=monotonic_ns(),last_force=0;unsigned received=0,translated=0,rejected=0;
    uint8_t last[8]={0};int active=0,last_phase=-1;
    while(!quit_requested && (seconds==0 || monotonic_ns()-start<(uint64_t)seconds*1000000000ull)){
        if(parent_pid && getppid()!=parent_pid){puts("controller exited; stopping");break;}
        uint64_t now=monotonic_ns();
        if(pulse){
            uint64_t ms=(now-start)/1000000;
            int phase=ms<1000?0:ms<1200?1:ms<2200?0:ms<2400?2:0;
            if(phase!=last_phase){
                uint8_t b[8];
                if(phase){ftec_constant((phase==1?1:-1)*gain,gain,b);printf("pulse direction=%d\n",phase==1?1:-1);}
                else {ftec_stop(b);puts("stop");}
                output_force(b);last_phase=phase;
            }
            usleep(5000);continue;
        }
        struct pollfd pollfd={.fd=fd,.events=POLLIN};poll(&pollfd,1,10);
        ForcePacket packet;uint8_t datagram[sizeof(ForcePacket)+1];ssize_t n;
        // Bound each drain so invalid traffic cannot starve timeout/signal handling.
        for(unsigned drained=0;drained<64 && !quit_requested && (n=recv(fd,datagram,sizeof(datagram),0))>=0;drained++){
            received++;now=monotonic_ns();
            if(n!=(ssize_t)sizeof(packet)){rejected++;continue;}
            memcpy(&packet,datagram,sizeof(packet));
            if(!force_packet_valid(&packet,(size_t)n,now)){rejected++;continue;}
            double effective_gain=ramp50?ffb_ramp_gain((now-start)/1000000ull,gain):gain;
            uint8_t b[8];if(!translate_force(packet.bytes,16,effective_gain,invert,b)){rejected++;continue;}
            translated++;last_force=now;active=b[1]!=0x03;
            if(memcmp(last,b,8)){
                if(live)output_force(b);
                memcpy(last,b,8);
                if(translated<=10 || translated%100==0)printf("force packet=%u source=%02x level=%u command=%02x\n",translated,packet.bytes[2],(unsigned)b[3]+((unsigned)b[4]<<8),b[1]);
            }
        }
        now=monotonic_ns();
        if(force_watchdog_expired(active,last_force,now)){
            if(live)stop_motor();ftec_stop(last);active=0;puts("watchdog stop");
        }
    }
    if(live){stop_motor();CFRelease(wheel);IOHIDManagerClose(manager,0);CFRelease(manager);}
    if(fd>=0){close(fd);unlink(FFB_SOCKET);}
    printf("finished received=%u translated=%u rejected=%u motor_write_error=%d\n",received,translated,rejected,failed);
    return failed?5:0;
}
