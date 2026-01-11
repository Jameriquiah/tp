#ifndef TP_FPS_H
#define TP_FPS_H

#include "dolphin/types.h"

#define TP_BASE_FPS 30
#define TP_TARGET_FPS 60
#define TP_BASE_FPS_F 30.0f
#define TP_TARGET_FPS_F 60.0f

static inline f32 tpFrameScale() {
    return (f32)TP_BASE_FPS / (f32)TP_TARGET_FPS;
}

static inline f32 tpFrameMul() {
    return (f32)TP_TARGET_FPS / (f32)TP_BASE_FPS;
}

static inline f32 tpFramesF32(f32 frames) {
    return frames * tpFrameMul();
}

static inline s32 tpFramesS32(s32 frames) {
    return (s32)(frames * TP_TARGET_FPS / TP_BASE_FPS);
}

static inline s16 tpFramesS16(s16 frames) {
    return (s16)tpFramesS32(frames);
}

static inline u16 tpFramesU16(u16 frames) {
    return (u16)tpFramesS32(frames);
}

static inline u8 tpFramesU8(u8 frames) {
    return (u8)tpFramesS32(frames);
}

static inline f32 tpPerFrameFromPerSecond(f32 per_sec) {
    return per_sec / (f32)TP_TARGET_FPS;
}

static inline s32 tpFrameIndex(s32 tick) {
    return (s32)(tick * TP_BASE_FPS / TP_TARGET_FPS);
}

static inline f32 tpFrameIndexF(f32 tick) {
    return tick * tpFrameScale();
}

#endif
