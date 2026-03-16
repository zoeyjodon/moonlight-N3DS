/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "video.hpp"
#include <stdexcept>

static std::unique_ptr<MockVideoDecoder> instance = nullptr;

MockVideoDecoder::MockVideoDecoder(int videoFormat, int width, int height,
                                   int redrawRate, void *context, int drFlags)
    : VideoDecoderBase(width, height) {}

MockVideoDecoder::~MockVideoDecoder() = default;

static int fakeDrSetup(int videoFormat, int width, int height, int redrawRate,
                       void *context, int drFlags) {
    try {
        instance = std::make_unique<MockVideoDecoder>(
            videoFormat, width, height, redrawRate, context, drFlags);
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "Failed to initialize N3DS soft decoder: %s\n",
                e.what());
        return -1;
    }
}
static void fakeDrStart(void) {}
static void fakeDrStop(void) {}
static void fakeDrCleanup(void) { instance = nullptr; }
static int fakeDrSubmitDecodeUnit(PDECODE_UNIT decodeUnit) { return DR_OK; }

DECODER_RENDERER_CALLBACKS decoder_callbacks_mock = {
    .setup = fakeDrSetup,
    .start = fakeDrStart,
    .stop = fakeDrStop,
    .cleanup = fakeDrCleanup,
    .submitDecodeUnit = fakeDrSubmitDecodeUnit,
};
