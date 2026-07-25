/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * PAL TV output on STM32F7 (LL drivers).
 * Composite sync bit-banged on TV_PIN_SYNC by a TIM7 line state machine. Video scanned out of a
 * packed framebuffer onto parallel GPIO pins TV_PIN_D0.. (external DAC, eg R2R ladder) by DMA2,
 * paced by TIM8 and hardware-triggered each line from TIM1 for jitter-free pixel timing.
 * ----------------------------------------------------------------------------
 */

#include "jshardware.h"
#include "jsutils.h"
#include "graphics.h"
#include "tv.h"
#include "stm32f7xx.h"
#include "stm32f7xx_ll_tim.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_dma.h"

#define PAL_VBLANK 32 // lines of blank before the first video line

#define PAL_LINE 64
#define PAL_PULSE_SHORT_ON 5
#define PAL_PULSE_LONG_ON 27
#define PAL_FRONTPORCH 10
#define PAL_BACKPORCH 2

// ns from sync pulse start to first pixel - positions the luma window at TIM1-tick (~4.6ns)
// resolution, finer than the PAL_* us values above can. Override via board.py's video_start_us.
#ifndef TV_VIDEO_START_NS
#define TV_VIDEO_START_NS ((PAL_PULSE_SHORT_ON+PAL_FRONTPORCH)*1000)
#endif

static const Pin tvVideoPins[] = {
  TV_PIN_D0,
#ifdef TV_PIN_D1
  TV_PIN_D1,
#endif
#ifdef TV_PIN_D2
  TV_PIN_D2,
#endif
#ifdef TV_PIN_D3
  TV_PIN_D3,
#endif
#ifdef TV_PIN_D4
  TV_PIN_D4,
#endif
#ifdef TV_PIN_D5
  TV_PIN_D5,
#endif
#ifdef TV_PIN_D6
  TV_PIN_D6,
#endif
#ifdef TV_PIN_D7
  TV_PIN_D7,
#endif
};
#define TV_VIDEO_BITS ((int)(sizeof(tvVideoPins)/sizeof(Pin)))
#ifndef TV_SYNC_INVERTED
#define TV_SYNC_INVERTED 0
#endif

static const uint8_t *tvPixelPtr;
static unsigned short tvWidth, tvHeight;
static unsigned char tvBpp;
static int tvCurrentLine;
static unsigned short ticksPerLine;

#ifndef TV_WIDTH
#define TV_WIDTH 384
#endif
#define TV_VIDEO_MASK ((1u<<TV_VIDEO_BITS)-1)
// double-buffered: one scanned out by DMA while the other is unpacked from tvPixelPtr. +1 pixel
// per row so the DMA itself blanks the pins right after the last real pixel. 32-byte aligned and
// padded so SCB_CleanDCache_by_Addr's fixed cache-line stride covers each row exactly
#define TV_LINEBUF_SIZE (((TV_WIDTH+1)+31)&~31)
static uint8_t tvLineBuf[2][TV_LINEBUF_SIZE] __attribute__((aligned(32)));
static int tvLineBufReady; // index of the buffer ready for tv_arm_line_video()
static int tvVideoLine; // increments once per active video line, reset each frame

static GPIO_TypeDef *tvVideoPort;
static int tvVideoShift; // bit of port the LSB video pin is on (0 or 8)
static uint32_t tvVideoBlankMask; // BSRR value to force all video pins to black (0)

static GPIO_TypeDef *tvSyncPort;
static uint32_t tvSyncOn, tvSyncOff; // precomputed BSRR values

unsigned int jshGetTimerFreq(TIM_TypeDef *TIMx);

static void (*tvTimerFunc)();

void TIM7_IRQHandler() {
  jshInterruptOff();
  LL_TIM_ClearFlag_UPDATE(TIM7);
  tvTimerFunc();
  jshInterruptOn();
}

static ALWAYS_INLINE void setTimer(unsigned int uSec) {
  TIM7->ARR = (uint32_t)(ticksPerLine * uSec / 64) - 1; // ARR+1 ticks per period
}

static ALWAYS_INLINE void sync_start() {
  tvSyncPort->BSRR = tvSyncOn;
}

static ALWAYS_INLINE void sync_end() {
  tvSyncPort->BSRR = tvSyncOff;
}

#define PAL_LINES_PER_FRAME 312 // non-interlaced, ~50Hz at 15625Hz line rate
#define PAL_HALF_LINE (PAL_LINE/2)

static bool tvIsVideo() {
  return tvCurrentLine>=PAL_VBLANK && tvCurrentLine<PAL_VBLANK+tvHeight;
}

// vsync region wraps the frame boundary: 3 short/short (pre) + 2 long/long (broad) + 1 long/short
// (transition) + 2 short/short (post) = 8 lines, each half-line pulsed independently (2x line
// rate). Everywhere else is one short pulse per full line
static bool tvIsVsyncRegion() {
  return tvCurrentLine>=PAL_LINES_PER_FRAME-3 || tvCurrentLine<5;
}

static void tvSyncWidths(unsigned int *w1, unsigned int *w2) {
  if (tvCurrentLine<2) { // broad
    *w1 = *w2 = PAL_PULSE_LONG_ON;
  } else if (tvCurrentLine==2) { // transition
    *w1 = PAL_PULSE_LONG_ON;
    *w2 = PAL_PULSE_SHORT_ON;
  } else { // equalizing (lines 3-4, and the 3 lines just before the frame wraps)
    *w1 = *w2 = PAL_PULSE_SHORT_ON;
  }
}

/// unpack one row of the (possibly packed) framebuffer into a byte-per-pixel line buffer, ready for DMA to the port
static void tv_fill_line_buffer(uint8_t *buf, int y) {
  buf[tvWidth] = 0; // trailing black pixel, scanned out by DMA right after the last real pixel
  if (y<0 || y>=tvHeight) {
    for (int x=0;x<tvWidth;x++) buf[x] = 0;
    return;
  }
  const uint8_t *row = tvPixelPtr;
  for (int x=0;x<tvWidth;x++) {
    int p = x + y*tvWidth; // pixel index (assumes MSB-first packing, see tv_start)
    unsigned int v;
    switch (tvBpp) {
      case 1: v = ((unsigned int)row[p>>3] >> (7-(p&7))) & 1; break;
      case 2: { int b=(p&3)<<1; v = ((unsigned int)row[p>>2] >> (6-b)) & 3; break; }
      case 4: { int b=(p&1)<<2; v = ((unsigned int)row[p>>1] >> (4-b)) & 15; break; }
      default: v = row[p]; break;
    }
    buf[x] = (uint8_t)(v & TV_VIDEO_MASK);
  }
}

// Disabling TIM8 here (well before TIM1's trigger, TV_VIDEO_START_NS into the line) stops it
// free-running on stale count; Combined Reset+Trigger mode then unconditionally resets CNT=0 and
// restarts it right on the trigger edge, with no "only starts an already-stopped timer" ambiguity.
static void tv_arm_line_video() {
  LL_TIM_DisableCounter(TIM8);
  // a timer's DMA request stays asserted until a DMA acknowledges it, so TIM8 free-running past
  // the last active window leaves one pending; if not gated off here it fires the instant the
  // stream re-enables below, putting pixel 0 on the pins ~15us before TIM1's real trigger
  LL_TIM_DisableDMAReq_UPDATE(TIM8);
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_1);
  LL_DMA_ClearFlag_TC1(DMA2);
  LL_DMA_ClearFlag_HT1(DMA2);
  LL_DMA_ClearFlag_TE1(DMA2);
  LL_DMA_ClearFlag_DME1(DMA2);
  LL_DMA_ClearFlag_FE1(DMA2);
  tvVideoPort->BSRR = tvVideoBlankMask; // startup/short-line safety net - normally already black

  if (!tvIsVideo()) return;

  LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_1, (uint32_t)tvLineBuf[tvLineBufReady]);
  LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_1, (uint32_t)(tvWidth+1)); // +1 trailing black pixel
  LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_1);
  LL_TIM_EnableDMAReq_UPDATE(TIM8);
}

// unpacks the row for the next video line into the buffer not currently being scanned. Slow
// (~28us) - only called from vid_start, which has the 44us video window's budget to spend
static void tv_prepare_next_line() {
  int fillBuf = 1-tvLineBufReady;
  tvVideoLine++;
  tv_fill_line_buffer(tvLineBuf[fillBuf], tvVideoLine % tvHeight);
  SCB_CleanDCache_by_Addr((uint32_t*)tvLineBuf[fillBuf], (int32_t)tvWidth+1); // DMA reads SRAM directly
  tvLineBufReady = fillBuf;
}

static unsigned int tvSyncW1, tvSyncW2; // this line's half-pulse widths, valid only in the vsync region
static uint32_t tvTim1PerTim7; // TIM1 ticks per TIM7 tick, for the per-frame phase lock
static bool tvStarted; // true once hardware init has run, see tv_start

static void tv_pal_irq_sync1_start();
static void tv_pal_irq_sync1_end();
static void tv_pal_irq_sync2_start();
static void tv_pal_irq_sync2_end();
static void tv_pal_irq_vid_start();
static void tv_pal_irq_vid_backporch();

static void tv_pal_irq_sync1_start() {
  if (++tvCurrentLine >= PAL_LINES_PER_FRAME) { tvCurrentLine=0; tvVideoLine=0; }

  // TIM7 is still counting from entry - a smaller ARR only takes effect after CNT wraps, so
  // reprogram it before anything else
  if (tvIsVsyncRegion()) {
    tvSyncWidths(&tvSyncW1, &tvSyncW2);
    setTimer(tvSyncW1);
  } else {
    setTimer(PAL_PULSE_SHORT_ON);
  }
  sync_start();
  tvTimerFunc = tv_pal_irq_sync1_end;

  // re-phase-lock TIM1 to TIM7 once per frame, on line 0 (blank, so a bad correction can't
  // shift a visible line). TIM7's CNT measures how late this ISR is running, so the lock is
  // immune to ISR latency and self-heals any drift TIM7 picked up from an interrupts-off
  // stretch (soft reset, flash save) within one frame
  if (tvCurrentLine==0) {
    LL_TIM_SetCounter(TIM1, LL_TIM_GetCounter(TIM7)*tvTim1PerTim7);
    LL_TIM_EnableCounter(TIM1); // no-op once already running
  }

  tv_arm_line_video();
}

static void tv_pal_irq_sync1_end() {
  sync_end();
  if (tvIsVsyncRegion()) {
    setTimer(PAL_HALF_LINE - tvSyncW1);
    tvTimerFunc = tv_pal_irq_sync2_start;
  } else if (tvIsVideo()) {
    setTimer(PAL_FRONTPORCH);
    tvTimerFunc = tv_pal_irq_vid_start;
  } else {
    setTimer(PAL_LINE - PAL_PULSE_SHORT_ON);
    tvTimerFunc = tv_pal_irq_sync1_start;
  }
}

static void tv_pal_irq_sync2_start() {
  setTimer(tvSyncW2);
  sync_start();
  tvTimerFunc = tv_pal_irq_sync2_end;
}

static void tv_pal_irq_sync2_end() {
  setTimer(PAL_HALF_LINE - tvSyncW2);
  sync_end();
  tvTimerFunc = tv_pal_irq_sync1_start;
}

static void tv_pal_irq_vid_start() {
  setTimer(PAL_LINE-(PAL_PULSE_SHORT_ON+PAL_FRONTPORCH+PAL_BACKPORCH));
  tvTimerFunc = tv_pal_irq_vid_backporch;
  tv_prepare_next_line(); // slow unpack - runs inside the 44us video window's own budget
}

static void tv_pal_irq_vid_backporch() {
  setTimer(PAL_BACKPORCH);
  tvTimerFunc = tv_pal_irq_sync1_start;
}

// ---------------------------------------------------------------- setup

static GPIO_TypeDef *pinToPort(Pin pin) {
  return (GPIO_TypeDef*)(GPIOA_BASE + 0x400u*(uint32_t)((pinInfo[pin].port&JSH_PORT_MASK)-JSH_PORTA));
}

/// reposition the luma window: ns from sync pulse start to the first pixel. Safe to call while running
void tv_set_video_start_ns(unsigned int nSec) {
  uint32_t ticks = (uint32_t)((uint64_t)jshGetTimerFreq(TIM1) * nSec / 1000000000ULL);
  uint32_t arr = LL_TIM_GetAutoReload(TIM1);
  if (ticks > arr) ticks = arr; // a compare past ARR never matches - would blank the whole line
  LL_TIM_OC_SetCompareCH1(TIM1, ticks);
}

/// sanity-check the board def's video pins - consecutive bits of one byte lane of one port
static bool tv_check_video_pins() {
  int port = pinInfo[tvVideoPins[0]].port & JSH_PORT_MASK;
  int bit0 = pinInfo[tvVideoPins[0]].pin;
  if (bit0!=0 && bit0!=8) { // DMA writes a whole byte lane of ODR
    jsExceptionHere(JSET_ERROR, "TV_PIN_D0 must be bit 0 or 8 of a port");
    return false;
  }
  for (int i=1;i<TV_VIDEO_BITS;i++) {
    if ((pinInfo[tvVideoPins[i]].port&JSH_PORT_MASK)!=port ||
        pinInfo[tvVideoPins[i]].pin != bit0+i) {
      jsExceptionHere(JSET_ERROR, "TV video pins must be consecutive bits of one port");
      return false;
    }
  }
  tvVideoPort = pinToPort(tvVideoPins[0]);
  tvVideoShift = bit0;
  tvVideoBlankMask = (TV_VIDEO_MASK << tvVideoShift) << 16; // BSRR reset bits - forces video pins to 0
  return true;
}

bool tv_start(const unsigned char *framebuffer, int width, int height, int bpp, bool msbFirst) {
  if (!tv_check_video_pins()) return false;
  if (bpp!=1 && bpp!=2 && bpp!=4 && bpp!=8) {
    jsExceptionHere(JSET_ERROR, "TV framebuffer must be 1, 2, 4 or 8 bpp");
    return false;
  }
  if (!msbFirst) {
    jsExceptionHere(JSET_ERROR, "TV framebuffer must be MSB-first");
    return false;
  }
  if (width<=0 || width>TV_WIDTH) {
    jsExceptionHere(JSET_ERROR, "TV framebuffer width must be 1..%d", TV_WIDTH);
    return false;
  }
  tvPixelPtr = framebuffer;
  tvWidth = (unsigned short)width;
  tvHeight = (unsigned short)height;
  tvBpp = (unsigned char)bpp;

  // pins are marked IS_PIN_USED_INTERNALLY so a soft reset won't touch them, but re-assert the
  // modes on every call anyway as cheap insurance
  uint32_t syncMask = 1u << pinInfo[TV_PIN_SYNC].pin;
  tvSyncPort = pinToPort(TV_PIN_SYNC);
  tvSyncOn  = TV_SYNC_INVERTED ? syncMask : syncMask<<16; // active low unless inverted
  tvSyncOff = TV_SYNC_INVERTED ? syncMask<<16 : syncMask;
  jshPinOutput(TV_PIN_SYNC, !TV_SYNC_INVERTED); // idle state
  for (int i=0;i<TV_VIDEO_BITS;i++)
    jshPinOutput(tvVideoPins[i], 0);

  // everything past here runs once only - the init hook re-runs tv_start on every soft reset
  // while the scanout hardware is already running; reconfiguring it mid-line would corrupt the
  // running state machine instead of leaving it alone
  if (tvStarted) return true;
  tvStarted = true;
  tvCurrentLine = 0;

  // TIM7 drives the PAL line state machine
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7);
  ticksPerLine = (unsigned short)(jshGetTimerFreq(TIM7) / 15625); // 64uS per line
  LL_TIM_SetPrescaler(TIM7, 0);
  LL_TIM_SetAutoReload(TIM7, ticksPerLine-1); // ARR+1 ticks per period, see setTimer()
  tvTimerFunc = tv_pal_irq_sync1_start;
  NVIC_SetPriority(TIM7_IRQn, 0);
  NVIC_EnableIRQ(TIM7_IRQn);
  LL_TIM_ClearFlag_UPDATE(TIM7);
  LL_TIM_EnableIT_UPDATE(TIM7);
  LL_TIM_EnableCounter(TIM7);

  // TIM8 is the pixel clock: one DMA-to-GPIO transfer per active-line pixel
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8);
  unsigned int activeWindowUs = PAL_LINE-(PAL_PULSE_SHORT_ON+PAL_FRONTPORCH+PAL_BACKPORCH);
  unsigned int pixelTicks = (unsigned int)((uint64_t)jshGetTimerFreq(TIM8) * activeWindowUs / (unsigned int)width / 1000000ULL);
  LL_TIM_SetPrescaler(TIM8, 0);
  LL_TIM_SetAutoReload(TIM8, pixelTicks-1); // ARR+1 ticks per period
  // EnableDMAReq_UPDATE deliberately omitted - tv_arm_line_video gates it off/on every line

  // DMA2 stream1/channel7 (TIM8_UP) writes one pixel byte to tvVideoPort's low byte lane per tick
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_1);
  LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_1, LL_DMA_CHANNEL_7);
  LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_1, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
  LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_1, LL_DMA_PRIORITY_HIGH);
  LL_DMA_SetMode(DMA2, LL_DMA_STREAM_1, LL_DMA_MODE_NORMAL);
  LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_1, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_1, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_1, LL_DMA_PDATAALIGN_BYTE);
  LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_1, LL_DMA_MDATAALIGN_BYTE);
  LL_DMA_DisableFifoMode(DMA2, LL_DMA_STREAM_1);
  LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_1, (uint32_t)&tvVideoPort->ODR + (tvVideoShift>=8 ? 1 : 0));

  LL_TIM_SetTriggerInput(TIM8, LL_TIM_TS_ITR0);
  LL_TIM_SetSlaveMode(TIM8, LL_TIM_SLAVEMODE_COMBINED_RESETTRIGGER);

  tvLineBufReady = 0;
  tvVideoLine = 0;
  tv_fill_line_buffer(tvLineBuf[0], 0);
  SCB_CleanDCache_by_Addr((uint32_t*)tvLineBuf[0], (int32_t)width+1);

  // TIM1: free-running 64us reference; OC1 compare match (TV_VIDEO_START_NS into the line) is
  // its TRGO, wired to TIM8 (ITR0) above. Only started/phase-locked by the line-0 ISR, see
  // tv_pal_irq_sync1_start
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
  tvTim1PerTim7 = jshGetTimerFreq(TIM1) / jshGetTimerFreq(TIM7);
  unsigned int ticksPerLine1 = (unsigned int)(jshGetTimerFreq(TIM1) / 15625);
  LL_TIM_SetPrescaler(TIM1, 0);
  LL_TIM_SetAutoReload(TIM1, ticksPerLine1-1); // ARR+1 ticks per period - must match TIM7 exactly or they drift apart
  LL_TIM_OC_SetMode(TIM1, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM2);
  tv_set_video_start_ns(TV_VIDEO_START_NS);
  LL_TIM_SetTriggerOutput(TIM1, LL_TIM_TRGO_OC1REF);
  LL_TIM_EnableAllOutputs(TIM1); // TIM1 is an advanced timer - MOE gates its outputs incl. OC1REF

  return true;
}
