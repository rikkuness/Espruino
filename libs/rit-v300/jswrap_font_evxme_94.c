/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * This file is designed to be parsed during the build process
 *
 * Contains Custom Fonts
 * ----------------------------------------------------------------------------
 */
/* DO_NOT_INCLUDE_IN_DOCS - this is a special token for common.py */
#include "jswrap_font_evxme_94.h"
#include "jswrap_graphics.h"

// Evxme94 (VGA text-mode font, converted from EVXME94.F08) at 1bpp. Fixed width: 6px, height: 8px, chars 32-126

static const unsigned char fontBitmap[] = { 0, 0, 0, 0, 0, 0, 0, 96, 250, 96, 0, 0, 0, 224, 0, 224, 0, 0, 40, 254, 40, 254, 40, 0, 36, 84, 214, 84, 72, 0, 98, 76, 16, 34, 70, 0, 12, 82, 178, 76, 18, 0, 0, 32, 192, 0, 0, 0, 56, 68, 130, 0, 0, 0, 0, 130, 68, 56, 0, 0, 16, 84, 56, 84, 16, 0, 0, 16, 16, 124, 16, 16, 0, 1, 6, 0, 0, 0, 0, 16, 16, 16, 16, 0, 0, 0, 6, 0, 0, 0, 2, 4, 8, 16, 32, 64, 124, 142, 146, 162, 124, 0, 34, 66, 254, 2, 2, 0, 70, 138, 154, 146, 102, 0, 68, 130, 146, 146, 108, 0, 24, 40, 74, 254, 10, 0, 244, 146, 146, 146, 140, 0, 60, 82, 146, 146, 12, 0, 192, 142, 144, 160, 192, 0, 108, 146, 146, 146, 108, 0, 96, 146, 146, 146, 124, 0, 0, 0, 102, 0, 0, 0, 0, 1, 102, 0, 0, 0, 16, 40, 68, 130, 0, 0, 36, 36, 36, 36, 36, 0, 0, 130, 68, 40, 16, 0, 64, 128, 138, 144, 96, 0, 124, 130, 186, 170, 120, 0, 62, 72, 136, 72, 62, 0, 130, 254, 146, 146, 108, 0, 56, 68, 130, 130, 68, 0, 130, 254, 130, 130, 124, 0, 130, 254, 146, 146, 198, 0, 130, 254, 146, 144, 192, 0, 56, 68, 130, 138, 78, 0, 254, 16, 16, 16, 254, 0, 0, 130, 254, 130, 0, 0, 12, 2, 130, 252, 128, 0, 130, 254, 16, 108, 130, 2, 130, 254, 130, 2, 6, 0, 254, 64, 48, 64, 254, 0, 254, 64, 32, 16, 254, 0, 124, 130, 130, 130, 124, 0, 130, 254, 146, 144, 96, 0, 124, 130, 134, 130, 125, 1, 130, 254, 152, 148, 98, 0, 100, 146, 146, 146, 76, 0, 192, 130, 254, 130, 192, 0, 252, 2, 2, 2, 252, 0, 248, 4, 2, 4, 248, 0, 252, 2, 28, 2, 252, 0, 130, 108, 16, 108, 130, 0, 192, 34, 30, 34, 192, 0, 198, 138, 146, 162, 198, 0, 254, 130, 130, 0, 0, 0, 192, 32, 16, 8, 4, 2, 130, 130, 254, 0, 0, 0, 48, 64, 128, 64, 48, 0, 1, 1, 1, 1, 1, 1, 0, 0, 192, 32, 0, 0, 4, 42, 42, 42, 30, 2, 130, 252, 10, 18, 12, 0, 28, 34, 34, 34, 20, 0, 12, 18, 18, 140, 254, 2, 28, 42, 42, 42, 24, 0, 18, 126, 146, 128, 64, 0, 25, 37, 37, 5, 62, 0, 130, 254, 16, 32, 30, 0, 0, 34, 190, 2, 0, 0, 6, 1, 1, 33, 190, 0, 128, 254, 8, 20, 34, 0, 0, 130, 254, 2, 0, 0, 62, 32, 30, 32, 30, 0, 62, 16, 32, 32, 30, 0, 28, 34, 34, 34, 28, 0, 33, 63, 37, 36, 24, 0, 24, 36, 37, 31, 33, 0, 34, 62, 18, 32, 48, 0, 18, 42, 42, 42, 36, 0, 32, 252, 34, 34, 4, 0, 60, 2, 2, 4, 62, 0, 56, 4, 2, 4, 56, 0, 60, 2, 28, 2, 60, 0, 34, 20, 8, 20, 34, 0, 57, 5, 5, 5, 62, 0, 34, 38, 42, 50, 34, 0, 16, 16, 108, 130, 130, 0, 0, 0, 238, 0, 0, 0, 130, 130, 108, 16, 16, 0, 64, 128, 64, 64, 128, 0 };

static const unsigned char fontWidths[] = { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 };

/*JSON{
  "type" : "method",
  "class" : "Graphics",
  "name" : "setFontEvxme94",
  "generate" : "jswrap_graphics_setFontEvxme94",
  "params" : [
    ["scale","int","[optional] If >1 the font will be scaled up by that amount"]
  ],
  "return" : ["JsVar","The instance of Graphics this was called on, to allow call chaining"],
  "return_object" : "Graphics"
}
Set the current font to Evxme94 (1 bpp, 8px tall)
*/
JsVar *jswrap_graphics_setFontEvxme94(JsVar *parent, int scale) {
  if (scale<1) scale=1;
  JsVar *bitmap = jsvNewNativeString((char *)fontBitmap, sizeof(fontBitmap));
  JsVar *widths = jsvNewNativeString((char *)fontWidths, sizeof(fontWidths));
  JsVar *r = jswrap_graphics_setFontCustom(parent, bitmap, 32, widths, 8 + (scale<<8) + (1<<16)); // 1 bit, total height 8px
  jsvUnLock2(bitmap, widths);
  return r;
}
