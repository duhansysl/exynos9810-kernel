/*
 * ALSA SoC - Samsung Abox adaptation driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_ADAPTATION_RICHTEK
#define __SEC_ADAPTATION_RICHTEK

#ifdef CONFIG_SEC_SND_ADAPTATION_RICHTEK
int richtek_spm_write(const void *data, int size);
int richtek_spm_read(void *data, int size);
#else
static inline int richtek_spm_write(const void *data, int size)
{
	return -ENOSYS;
}

static inline int richtek_spm_read(void *data, int size)
{
	return -ENOSYS;
}
#endif /* CONFIG_SEC_SND_ADAPTATION_RICHTEK */

static inline int32_t dsm_read_write(void *data)
{
	return -ENOSYS;
}

#endif /* __SEC_ADAPTATION_RICHTEK */

