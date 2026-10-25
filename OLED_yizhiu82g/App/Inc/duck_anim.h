/* duck_anim.h - animated 1-bit portrait data (converted from 图例.jpg) */
#ifndef DUCK_ANIM_H
#define DUCK_ANIM_H

#include <stdint.h>

#define DUCK_FRAME_W 64
#define DUCK_FRAME_H 64
#define DUCK_FRAME_COUNT 3

extern const uint8_t *const duck_frames[DUCK_FRAME_COUNT];
extern const unsigned char star_9x9[];
extern const unsigned char star_7x7[];

#endif
