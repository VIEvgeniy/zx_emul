#pragma once
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "zx_emu/zx_machine.h"


#define JOY_MODE_COUNT 5
#define JOY_KEMPSTON_MODE 0
#define JOY_SINCLAIR1_MODE 1
#define JOY_SINCLAIR2_MODE 2
#define JOY_CURSOR_MODE 3
#define JOY_KEYBOARD_MODE 4


typedef struct
{
    const uint8_t data_pin;
    const uint8_t clock_pin;
    const uint8_t latch_pin;
    uint8_t data;
    uint8_t old_data;
    uint8_t mode;
  
} Joy;

void d_sleep_us(uint us);
void Joy_init(Joy *j);
void Joy_get_data(Joy *j);
bool Joy_get_left(Joy *j);
bool Joy_is_left(Joy *j);
bool Joy_get_right(Joy *j);
bool Joy_is_right(Joy *j);
bool Joy_get_down(Joy *j);
bool Joy_is_down(Joy *j);
bool Joy_get_up(Joy *j);
bool Joy_is_up(Joy *j);
bool Joy_get_A(Joy *j);
bool Joy_is_A(Joy *j);
bool Joy_get_B(Joy *j);
bool Joy_is_B(Joy *j);
bool Joy_get_select(Joy *j);
bool Joy_is_select(Joy *j);
bool Joy_get_start(Joy *j);
bool Joy_is_start(Joy *j);
uint8_t Joy_get_kempstom_data(Joy *j);
bool Joy_is_new_data(Joy *j);
void Joy_to_zx (Joy *J, ZX_Input_t *zx_input);