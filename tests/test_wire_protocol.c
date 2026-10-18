#include <assert.h>
#include <stdio.h>
#include "ffb_wire.h"
int main(void) {
    ForcePacket p={.magic=FFB_MAGIC,.type=1,.length=16,.time_ns=100};
    assert(force_packet_valid(&p,sizeof(p),100));
    assert(force_packet_valid(&p,sizeof(p),100000100));
    assert(!force_packet_valid(NULL,sizeof(p),100));
    assert(!force_packet_valid(&p,0,100));
    p.magic=0;assert(!force_packet_valid(&p,sizeof(p),100));p.magic=FFB_MAGIC;
    p.type=0;assert(!force_packet_valid(&p,sizeof(p),100));p.type=1;
    p.report_id=1;assert(!force_packet_valid(&p,sizeof(p),100));p.report_id=0;
    p.length=15;assert(!force_packet_valid(&p,sizeof(p),100));p.length=16;
    p.reserved=1;assert(!force_packet_valid(&p,sizeof(p),100));p.reserved=0;
    assert(!force_packet_valid(&p,sizeof(p),99));
    assert(!force_packet_valid(&p,sizeof(p),100000101));
    assert(!force_watchdog_expired(0,0,300000000));
    assert(!force_watchdog_expired(1,100,99));
    assert(!force_watchdog_expired(1,100,250000100));
    assert(force_watchdog_expired(1,100,250000101));
    p.product_id=0x20;p.location_id=123;
    assert(force_packet_for_device(&p,0x20,123));
    assert(!force_packet_for_device(NULL,0x20,123));
    assert(!force_packet_for_device(&p,0,123));assert(!force_packet_for_device(&p,0x20,0));
    assert(!force_packet_for_device(&p,6,123));assert(!force_packet_for_device(&p,0x20,456));
    p.magic=0x31424646u;assert(!force_packet_valid(&p,sizeof(p),100));
    assert(monotonic_ns()>0);
    puts("PASS: packet metadata, freshness boundaries and watchdog boundaries");
}
