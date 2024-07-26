// SPDX-License-Identifier: GPL-2.0

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

/* Implements */
#include <gpexwa_ehmp.h>

/* Depends on CONFIG_SCHED_EHMP being supported */
#include <sched.h>
#include <linux/ehmp.h>
static struct gb_qos_request gb_req = {
		.name = "ehmp_boost",
};

#include <gpexbe_qos.h>
#include <gpexbe_devicetree.h>

#define QOS_TIMEOUT (100000)

/* Uses */
struct ehmp_info {
	struct mutex gpu_sched_hmp_lock;
	bool ctx_need_qos;
	int lock_clock;
} ehmp_info;

void gpexwa_ehmp_set(void) {
	mutex_lock(&ehmp_info.gpu_sched_hmp_lock);
	if (!ehmp_info.ctx_need_qos) {
		ehmp_info.ctx_need_qos = true;
		gb_qos_update_request(&gb_req, 100);
	}
	mutex_unlock(&ehmp_info.gpu_sched_hmp_lock);

	gpexbe_qos_request_update_timeout(PMQOS_MIDDLE | PMQOS_MIN, ehmp_info.lock_clock, QOS_TIMEOUT);
}

void gpexwa_ehmp_unset(void) {
	mutex_lock(&ehmp_info.gpu_sched_hmp_lock);
	if (ehmp_info.ctx_need_qos) {
		ehmp_info.ctx_need_qos = false;
		gb_qos_update_request(&gb_req, 0);
	}
	mutex_unlock(&ehmp_info.gpu_sched_hmp_lock);
}

bool gpexwa_ehmp_skip_ifpo_power_down(void) {
	return ehmp_info.ctx_need_qos;
}

int gpexwa_ehmp_init(void)
{
	mutex_init(&ehmp_info.gpu_sched_hmp_lock);
	ehmp_info.ctx_need_qos = false;

	ehmp_info.lock_clock = gpexbe_devicetree_get_int(gpu_boost_egl_min_lock);

	return 0;
}

void gpexwa_ehmp_term(void)
{
	gpexwa_ehmp_unset();
}
