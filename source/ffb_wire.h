#ifndef FFB_WIRE_H
#define FFB_WIRE_H
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#define FFB_MAGIC 0x31424646u
typedef struct {
    uint32_t magic,pid;
    uint64_t time_ns;
    uint32_t type,report_id,length,reserved;
    uint8_t bytes[16];
} ForcePacket;
static inline int force_packet_valid(const ForcePacket *p,size_t size,uint64_t now) {
    return p && size==sizeof(*p) && p->magic==FFB_MAGIC && p->type==1 && p->report_id==0 &&
        p->length==16 && p->reserved==0 && now>=p->time_ns && now-p->time_ns<=100000000ull;
}
static inline int force_watchdog_expired(int active,uint64_t last,uint64_t now) {
    return active && now>=last && now-last>250000000ull;
}
static inline uint64_t monotonic_ns(void){
    struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec;
}
#endif
