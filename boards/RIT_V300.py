#!/bin/false
# -*- coding: utf8 -*-

import pinutils
info = {
 'name' : "RIT_V300",
 'link' :  [ "https://robco-industries.org/projects/rit-v300-terminal"],
 'default_console' : "EV_SERIAL3", # USART3 connected to ST-LINK Virtual Com Port by default without changing solder bridges
 'default_console_tx' : "D8", # USART3_TX on PD8
 'default_console_rx' : "D9", # USART3_RX on PD9
 'variables' :  29130, # (512-12)*1024/16-1, minus the 45900-byte TV framebuffer (340*270*4bpp/8)
 'binary_name' : 'espruino_%v_rit_v300.bin',
 'build' : {
   'optimizeflags' : '-Os',
   'libraries' : [
     'GRAPHICS',
     'NET'
   ],
   'makefile' : [
     #'DEFINES+=-DSAVE_ON_FLASH_MATH', 
     #'DEFINES+=-DESPR_PACKED_SYMPTR', # Pack builtin symbols' offset into pointer to save 2 bytes/symbol
     'INCLUDE += -I$(ROOT)/libs/rit-v300',
     'DEFINES+=-DESPR_GRAPHICS_INTERNAL',
     'SOURCES+=libs/graphics/tv.c',
     'WRAPPERSOURCES+=libs/graphics/jswrap_tv.c',
     'WRAPPERSOURCES+=targets/nucleo/jswrap_nucleo.c',
     'WRAPPERSOURCES+=libs/rit-v300/jswrap_ritv300.c',
     'WRAPPERSOURCES+=libs/rit-v300/jswrap_font_fixedsys_16.c',
     'DEFINES+=-DUSE_USB_OTG_FS=1',
     'DEFINES+=-DPIN_NAMES_DIRECT=1', # Package skips out some pins, so we can't assume each port starts from 0
     'STLIB=STM32F767xx',
     'PRECOMPILED_OBJS+=$(ROOT)/targetlibs/stm32f7/lib/startup_stm32f767xx.o'
   ]
  }
};
chip = {
  'part' : "STM32F767ZIT6",
  'family' : "STM32F7",
  'package' : "LQFP144",
  'ram' : 512,
  'flash' : 2048,
  'flash_base' : 0x08000000,
  'speed' : 216,
  'usart' : 8,
  'spi' : 6,
  'i2c' : 4,
  'adc' : 3,
  'dac' : 2,
  'saved_code' : {
    # F767 flash sectors (single-bank, the ST factory default): 4x32KB + 1x128KB + 7x256KB = 2048KB
    # Reserve the last 256KB sector (sector 11, 0x081C_0000 to 0x0820_0000) for saved code
    'address' : 0x081C0000,
    'page_size' :  262144, # 256KB sector
    'pages' : 1,
    'flash_available' : 1792 # 2048 - 256 = 1792KB available for firmware
  },
}

devices = {
  'OSC' : { 'pin_1' : 'H0', # MCO from ST-LINK fixed at 8 Mhz, boards rev MB1136 C-02
            'pin_2' : 'H1' },
  'OSC_RTC' : { 'pin_1' : 'C14', # MB1136 C-02 corresponds to a board configured with on-board 32kHz oscillator
                'pin_2' : 'C15' },
  'LED1' : { 'pin' : 'B0' },
  'LED2' : { 'pin' : 'B7' },
  'LED3' : { 'pin' : 'B14' },
  'BTN1' : { 'pin' : 'C13', # TODO: Fix, this seems non functional
             'inverted' : True, # 1 when unpressed, 0 when pressed! (Espruino board is 1 when pressed)
             'pinstate': 'IN_PULLUP', # to specify INPUT, OUPUT PULL_UP PULL_DOWN..
           },
  'JTAG' : {
        'pin_MS' : 'A13',
        'pin_CK' : 'A14',
        'pin_DI' : 'A15'
          },
  'USB' : { 'pin_dm' : 'A11',
            'pin_dp' : 'A12',
            'pin_vbus' : 'A9',
            'pin_id' : 'A10', },
  'TV' : { 'width' : 340, 'height' : 270, 'bpp' : 4,
           'pin_d0' : 'E0',
           'pin_d1' : 'E1',
           'pin_d2' : 'E2',
           'pin_d3' : 'E3',
           'pin_sync' : 'G1',
           'video_start_us' : 15.2,
           'sync_inverted' : False },
  # TODO: NUCLEO_A and NUCLEO_D Arduino header mappings for Nucleo-144
  # These are temporary values from Nucleo-64 to allow compilation
  'NUCLEO_A' : [ 'A0','A1','A4','B0','C1','C0' ],
  'NUCLEO_D' : [ 'A3','A2','A10','B3','B5','B4','B10','A8','A9','C7','B6','A7','A6','A5','B9','B8' ],
}

def get_pins():
  pins = pinutils.scan_pin_file([], 'stm32f767.csv', 6, 9, 10)
  pins = pinutils.scan_pin_af_file(pins, 'stm32f767_af.csv', 0, 1)
  return pinutils.only_from_package(pinutils.fill_gaps_in_pin_list(pins), chip["package"])
