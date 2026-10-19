#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "ffb_wire.h"
int main(int argc,char **argv){
    assert(argc==2);
    int fd=socket(AF_UNIX,SOCK_DGRAM,0);assert(fd>=0);
    struct sockaddr_un a={.sun_family=AF_UNIX};assert(strlen(argv[1])<sizeof(a.sun_path));strcpy(a.sun_path,argv[1]);
    ForcePacket p={.magic=FFB_MAGIC,.pid=(uint32_t)getpid(),.type=1,.length=16,.bytes={0x11,0x08,255,0x80}};
    p.time_ns=monotonic_ns();assert(sendto(fd,&p,sizeof(p),0,(struct sockaddr *)&a,sizeof(a))==sizeof(p));
    uint8_t oversized[sizeof(p)+1];memcpy(oversized,&p,sizeof(p));oversized[sizeof(p)]=1;
    assert(sendto(fd,oversized,sizeof(oversized),0,(struct sockaddr *)&a,sizeof(a))==sizeof(oversized));
    p.bytes[0]=0xf8;p.bytes[1]=0x12;p.time_ns=monotonic_ns();
    assert(sendto(fd,&p,sizeof(p),0,(struct sockaddr *)&a,sizeof(a))==sizeof(p));
    usleep(400000);p.bytes[0]=0x11;p.bytes[1]=0x08;p.time_ns=monotonic_ns()-1000000000ull;
    assert(sendto(fd,&p,sizeof(p),0,(struct sockaddr *)&a,sizeof(a))==sizeof(p));close(fd);
}
