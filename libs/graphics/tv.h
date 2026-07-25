/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * TV output - analogue video scanout of a raw framebuffer.
 * Pins come from the board's TV device (TV_PIN_D0.., TV_PIN_SYNC)
 * ----------------------------------------------------------------------------
 */

#include "jsutils.h"

/// Configure GPIO/TIM7 and start PAL scanout of a raw framebuffer. Returns false (with exception) on error
bool tv_start(const unsigned char *framebuffer, int width, int height, int bpp, bool msbFirst);

/// Reposition the luma window: ns from sync pulse start to the first pixel. Safe to call while running
void tv_set_video_start_ns(unsigned int nSec);
