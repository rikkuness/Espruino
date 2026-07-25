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
#include "jsvar.h"

void jswrap_tv_init(void);
