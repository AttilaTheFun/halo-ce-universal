#ifndef METAL_HUD_MIPMAP_H
#define METAL_HUD_MIPMAP_H

#include <stdint.h>

/* Reduce independent RGBA channels, including the HUD meter's authored green
 * coverage channel. Source and destination may be the same allocation. HUD
 * sheets have power-of-two dimensions; an axis at one texel stays at one. */
static inline void metal_hud_mip_reduce_rgba(unsigned char *pixels,
    uint32_t width, uint32_t height)
{
    uint32_t next_width=width>1 ? width/2:1;
    uint32_t next_height=height>1 ? height/2:1;
    uint32_t count=(width>1 ? 2:1)*(height>1 ? 2:1);
    for (uint32_t y=0;y<next_height;y++) for (uint32_t x=0;x<next_width;x++) {
        uint32_t source_x=width>1 ? 2*x:0,source_y=height>1 ? 2*y:0;
        for (uint32_t channel=0;channel<4;channel++) {
            uint32_t total=0;
            for (uint32_t dy=0;dy<(height>1 ? 2u:1u);dy++)
                for (uint32_t dx=0;dx<(width>1 ? 2u:1u);dx++)
                    total+=pixels[((source_y+dy)*width+source_x+dx)*4+channel];
            pixels[(y*next_width+x)*4+channel]=(unsigned char)((total+count/2)/count);
        }
    }
}

#endif
