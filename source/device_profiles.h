// SPDX-License-Identifier: GPL-2.0-only
#ifndef DEVICE_PROFILES_H
#define DEVICE_PROFILES_H
#include <stddef.h>
#include <stdint.h>
typedef struct {uint16_t pid; int highres; const char *name;} FtecProfile;
// Source: gotzl/hid-fanatecff hid-ftec.h and devices[]. Native PC-mode IDs.
// 0020 is shared; USB PID alone cannot identify a specific DD model.
static const FtecProfile ftec_profiles[]={
    {0x0020,1,"CSL DD / GT DD Pro / ClubSport DD family"},
    {0x0006,1,"Podium DD1 (experimental)"},
    {0x0007,1,"Podium DD2 (experimental)"},
    {0x0e03,1,"CSL Elite (experimental)"},
    {0x0005,1,"CSL Elite PS4 (experimental)"},
    {0x0001,0,"ClubSport V2 (experimental)"},
    {0x0004,0,"ClubSport V2.5 (experimental)"}
};
#define FTEC_PROFILE_COUNT (sizeof(ftec_profiles)/sizeof(ftec_profiles[0]))
static inline const FtecProfile *ftec_profile(int vendor,int product){
    if(vendor!=0x0eb7)return NULL;
    for(size_t i=0;i<FTEC_PROFILE_COUNT;i++)if(product==ftec_profiles[i].pid)return &ftec_profiles[i];
    return NULL;
}
// Convert the internal high-resolution command to the legacy 8-bit format.
// Explicit slot/global stops remain unchanged; neutral becomes 0x80, not zero.
static inline void ftec_encode_resolution(uint8_t packet[8],int highres){
    if(!highres && packet[1]==1 && packet[2]==8){
        packet[3]=packet[4];packet[4]=0;packet[7]=0;
    }
}
static inline int ftec_output_format(unsigned bits,unsigned count,unsigned id){
    return (bits==8 || bits==56) && count==7 && id<=1;
}
static inline uint32_t ftec_scale(int64_t value,int64_t minimum,int64_t maximum,uint32_t target){
    if(maximum<=minimum)return 0;
    if(value<minimum)value=minimum;
    if(value>maximum)value=maximum;
    return (uint32_t)(((uint64_t)(value-minimum)*target)/(uint64_t)(maximum-minimum));
}
#endif
