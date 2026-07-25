/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * This file is designed to be parsed during the build process
 *
 * TV output - creates the board's `g` Graphics instance and starts PAL scanout
 * ----------------------------------------------------------------------------
 */
#include "jsutils.h"
#include "jsparse.h"
#include "jsinteractive.h"
#include "graphics.h"
#include "lcd_arraybuffer.h"
#include "jswrap_tv.h"
#include "tv.h"

#define TV_BUFFER_SIZE ((TV_WIDTH*TV_HEIGHT*TV_BPP+7)/8)
static unsigned char tvFramebuffer[TV_BUFFER_SIZE];

/*JSON{
  "type" : "init",
  "generate" : "jswrap_tv_init"
}*/
void jswrap_tv_init() {
  JsVar *parent = jspNewObject("g", "Graphics");
  if (!parent) return;
  JsVar *parentObj = jsvSkipName(parent);
  jsvObjectSetChild(execInfo.hiddenRoot, JS_GRAPHICS_VAR, parentObj);

  JsGraphics *gfx = &graphicsInternal;
  graphicsStructInit(gfx, TV_WIDTH, TV_HEIGHT, TV_BPP);
  gfx->graphicsVar = parentObj;
  gfx->data.type = JSGRAPHICSTYPE_ARRAYBUFFER;
  gfx->data.flags = JSGRAPHICSFLAGS_ARRAYBUFFER_MSB;

  JsVar *str = jsvNewNativeString((char*)tvFramebuffer, TV_BUFFER_SIZE);
  JsVar *buf = jsvNewArrayBufferFromString(str, TV_BUFFER_SIZE);
  jsvUnLock(str);
  lcdInit_ArrayBuffer(gfx, buf);
  jsvUnLock(buf);
  lcdSetCallbacks_ArrayBuffer(gfx);

#ifndef ESPR_GRAPHICS_NO_SPLASH
  graphicsSplash(gfx);
#endif
  graphicsSetVarInitial(gfx);
  jsvUnLock2(parentObj, parent);

  tv_start(tvFramebuffer, TV_WIDTH, TV_HEIGHT, TV_BPP, true);
}
