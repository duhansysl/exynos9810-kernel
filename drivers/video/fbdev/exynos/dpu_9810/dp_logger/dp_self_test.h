/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DP self test
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DP_SELF_TEST_
#define _DP_SELF_TEST_
#include "../displayport.h"

int self_test_get_edid(u8 *edid);
int self_test_on_process(void);
enum dex_support_type self_test_get_dp_adapter_type(void);

void self_test_init(struct displayport_device *displayport, struct class *dp_class);
void self_test_resolution_update(u32 xres, u32 yres, u32 fps);
void self_test_audio_param_update(u32 ch, u32 fs, u32 bit);

extern void displayport_hpd_changed(int state);

#endif
