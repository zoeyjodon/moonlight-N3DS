/*
 * Build-time detection of H.263+ support.
 *
 * H.263+ needs two things that are not part of upstream moonlight-common-c:
 * the VIDEO_FORMAT_H263 flags (added by 3ds/apply-patches.sh) and the
 * h263_util.h picture header parser. When either is missing, the H.263+ code
 * is compiled out, the format flags become 0 and the client behaves as a
 * plain H.264 client.
 *
 * This header is valid C and C++ and is meant to be included after (or in
 * place of) Limelight.h.
 */

#pragma once

#include <Limelight.h>

#if defined(VIDEO_FORMAT_H263) && defined(VIDEO_FORMAT_MASK_H263) &&          \
    defined(__has_include)
#if __has_include("h263_util.h")
#define N3DS_H263_AVAILABLE 1
#endif
#endif

#ifndef N3DS_H263_AVAILABLE
#define N3DS_H263_AVAILABLE 0
#endif

#ifndef VIDEO_FORMAT_H263
#define VIDEO_FORMAT_H263 0x0000
#endif

#ifndef VIDEO_FORMAT_MASK_H263
#define VIDEO_FORMAT_MASK_H263 0x0000
#endif
