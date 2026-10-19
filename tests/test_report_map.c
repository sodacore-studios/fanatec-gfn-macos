#include <assert.h>
#include <stdio.h>
#include "report_map.h"
int main(void) {
    uint8_t source[34]={1},dest[12];
    source[1]=8;source[18]=128;source[19]=source[20]=source[21]=source[22]=255;
    assert(map_report(source,34,dest,0));
    assert(dest[0]==8 && dest[4]==0 && dest[5]==128);
    assert(dest[6]==255 && dest[7]==255 && dest[8]==255);
    source[17]=source[18]=255;source[19]=source[20]=0;
    source[4]=255;
    assert(map_report(source,34,dest,0));
    assert(dest[4]==255 && dest[5]==255 && dest[6]==0 && dest[7]==255);
    assert(dest[3]==31);
    assert(map_report(source,34,dest,1) && dest[6]==255 && dest[7]==0);
    assert(!map_report(source,33,dest,0));
    source[0]=2;assert(!map_report(source,34,dest,0));
    source[0]=255;assert(!map_report(source,34,dest,0));
    assert(!map_report(NULL,34,dest,0));
    assert(!map_report(source,34,NULL,0));
    puts("PASS: steering extremes/centre, independent pedals, clutch released, padding, wrong report rejection");
}
