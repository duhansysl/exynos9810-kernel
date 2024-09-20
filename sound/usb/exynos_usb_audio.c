/*
 *   USB Audio offloading Driver for Exynos
 *
 *   Copyright (c) 2017 by Kyounghye Yun <k-hye.yun@samsung.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */


#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>

#include <sound/samsung/abox.h>
#include <linux/usb/exynos_usb_audio.h>

#define DEBUG 1
struct exynos_usb_audio *usb_audio;

void exynos_usb_audio_set_device(struct usb_device *udev)
{
	usb_audio->udev = udev;
	usb_audio->is_audio = 1;
}

int exynos_usb_audio_map_buf(struct usb_device *udev)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	struct hcd_hw_info *hwinfo = &udev->hwinfo;
	int ret;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s ", __func__);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_PCM_BUF;

	usb_audio->in_buf_addr = hwinfo->in_dma;

	if (DEBUG) {
		pr_info("pcm in data buffer pa addr : %#08x %08x \n",
				upper_32_bits(le64_to_cpu(hwinfo->in_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->in_dma)));
	}

	ret = abox_iommu_map(dev, USB_AUDIO_PCM_INBUF,
					hwinfo->in_dma, PAGE_SIZE * 15);
	if (ret) {
		pr_err("abox iommu mapping for pcm buf is failed \n");
		return ret;
	}

	return 0;
}

int exynos_usb_audio_pcmbuf(struct usb_device *udev)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	struct hcd_hw_info *hwinfo = &udev->hwinfo;
	u64 out_dma;
	int ret;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	out_dma = abox_iova_to_phys(dev, USB_AUDIO_PCM_OUTBUF);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_PCM_BUF;

	erap_usb->param1 = lower_32_bits(le64_to_cpu(out_dma));
	erap_usb->param2 = upper_32_bits(le64_to_cpu(out_dma));
	erap_usb->param3 = lower_32_bits(le64_to_cpu(hwinfo->in_dma));
	erap_usb->param4 = upper_32_bits(le64_to_cpu(hwinfo->in_dma));

	if (DEBUG) {
		pr_info("pcm out data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(out_dma)),
				lower_32_bits(le64_to_cpu(out_dma)));
		pr_info("pcm in data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(hwinfo->in_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->in_dma)));
		pr_info("erap param2 : %#08x param1 : %08x\n",
				erap_usb->param2, erap_usb->param1);
		pr_info("erap param4 : %#08x param3 : %08x\n",
				erap_usb->param4, erap_usb->param3);
	}

	ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
	if (ret) {
		dev_err(&usb_audio->udev->dev, "erap usb transfer pcm buffer is failed\n");
		return -1;
	}

	return 0;
}

int exynos_usb_audio_setrate(int iface, int rate, int alt)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	int ret;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_SAMPLE_RATE;

	erap_usb->param1 = iface;
	erap_usb->param2 = rate;
	erap_usb->param3 = alt;

	ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
	if (ret) {
		dev_err(&usb_audio->udev->dev, "erap usb transfer sample rate is failed\n");
		return -1;
	}

	return 0;
}

int exynos_usb_audio_setintf(struct usb_device *udev, int iface, int alt, int direction)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	struct hcd_hw_info *hwinfo = &udev->hwinfo;
	u64 offset, in_offset, out_offset;
	int ret;

	if (DEBUG)
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s ", __func__);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_SET_INTF;

	erap_usb->param1 = alt;
	erap_usb->param2 = iface;

	mutex_lock(&usb_audio->lock);
	if (direction) {
		/* IN EP */
		dev_dbg(&udev->dev, "in deq : %#08llx\n", hwinfo->in_deq);
		offset = hwinfo->old_in_deq % PAGE_SIZE;
		ret = abox_iommu_unmap(dev, USB_AUDIO_IN_DEQ, (hwinfo->old_in_deq - offset), PAGE_SIZE);
		if (ret) {
			pr_err("abox iommu un-mapping for in buf is failed \n");
			return ret;
		}

		in_offset = hwinfo->in_deq % PAGE_SIZE;
		ret = abox_iommu_map(dev, USB_AUDIO_IN_DEQ, (hwinfo->in_deq - in_offset), PAGE_SIZE);
		if (ret) {
			pr_err("abox iommu mapping for in buf is failed \n");
			return ret;
		}

		erap_usb->param3 = lower_32_bits(le64_to_cpu(hwinfo->in_deq));
		erap_usb->param4 = upper_32_bits(le64_to_cpu(hwinfo->in_deq));
	} else {
		/* OUT EP */
		dev_dbg(&udev->dev, "out deq : %#08llx \n", hwinfo->out_deq);
		offset = hwinfo->old_out_deq % PAGE_SIZE;
		ret = abox_iommu_unmap(dev, USB_AUDIO_OUT_DEQ, (hwinfo->old_out_deq - offset), PAGE_SIZE);
		if (ret) {
			pr_err("abox iommu un-mapping for in buf is failed \n");
			return ret;
		}

		out_offset = hwinfo->out_deq % PAGE_SIZE;
		ret = abox_iommu_map(dev, USB_AUDIO_OUT_DEQ, (hwinfo->out_deq - out_offset), PAGE_SIZE);
		if (ret) {
			pr_err("abox iommu mapping for out buf is failed \n");
			return ret;
		}

		erap_usb->param3 = lower_32_bits(le64_to_cpu(hwinfo->out_deq));
		erap_usb->param4 = upper_32_bits(le64_to_cpu(hwinfo->out_deq));
	}

	ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
	if (ret) {
		dev_err(&usb_audio->udev->dev, "erap usb hcd control failed\n");
		return -1;
	}
	mutex_unlock(&usb_audio->lock);

	if (DEBUG) {
		dev_info(&udev->dev, "Alt#%d / Intf#%d / Direction %s / EP DEQ : %#08x %08x\n",
				erap_usb->param1, erap_usb->param2,
				direction ? "IN" : "OUT",
				erap_usb->param4, erap_usb->param3);
	}

	return 0;
}

int exynos_usb_audio_hcd(struct usb_device *udev)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	struct hcd_hw_info *hwinfo = &udev->hwinfo;
	int ret;

	if (DEBUG) {
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s\n", __func__);
		dev_info(&udev->dev, "=======[Check HW INFO] ========\n");
		dev_info(&udev->dev, "slot_id : %d\n", hwinfo->slot_id);
		dev_info(&udev->dev, "dcbaa : %#08llx\n", hwinfo->dcbaa_dma);
		dev_info(&udev->dev, "save : %#08llx\n", hwinfo->save_dma);
		dev_info(&udev->dev, "in_ctx : %#08llx\n", hwinfo->in_ctx);
		dev_info(&udev->dev, "out_ctx : %#08llx\n", hwinfo->out_ctx);
		dev_info(&udev->dev, "erst : %#08x %08x\n",
					upper_32_bits(le64_to_cpu(hwinfo->erst_addr)),
					lower_32_bits(le64_to_cpu(hwinfo->erst_addr)));
		dev_info(&udev->dev, "===============================\n");
	}

	/* back up each address for unmap */
	usb_audio->dcbaa_dma = hwinfo->dcbaa_dma;
	usb_audio->save_dma = hwinfo->save_dma;
	usb_audio->in_ctx = hwinfo->in_ctx;
	usb_audio->out_ctx = hwinfo->out_ctx;
	usb_audio->erst_addr = hwinfo->erst_addr;
	usb_audio->speed = hwinfo->speed;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : SFR MAPPING!\n");

	mutex_lock(&usb_audio->lock);
	ret = abox_iommu_map(dev, USB_AUDIO_XHCI_BASE, USB_AUDIO_XHCI_BASE, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for in buf is failed\n");
		return ret;
	}

	/*DCBAA mapping*/
	ret = abox_iommu_map(dev, USB_AUDIO_SAVE_RESTORE, hwinfo->save_dma, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for save_restore buffer is failed\n");
		return ret;
	}

	/*Device Context mapping*/
	ret = abox_iommu_map(dev, USB_AUDIO_DEV_CTX, hwinfo->out_ctx, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for device ctx is failed\n");
		return ret;
	}

	/*Input Context mapping*/
	ret = abox_iommu_map(dev, USB_AUDIO_INPUT_CTX, hwinfo->in_ctx, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for input ctx is failed\n");
		return ret;
	}

	/*ERST mapping*/
	ret = abox_iommu_map(dev, USB_AUDIO_ERST, hwinfo->erst_addr, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for erst is failed\n");
		return ret;
	}

	/* notify to Abox descriptor is ready*/
	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_XHCI;

	erap_usb->param1 = 1;
	erap_usb->param2 = hwinfo->slot_id;
	erap_usb->param3 = lower_32_bits(le64_to_cpu(hwinfo->erst_addr));
	erap_usb->param4 = upper_32_bits(le64_to_cpu(hwinfo->erst_addr));

	ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
	if (ret) {
		dev_err(&usb_audio->udev->dev, "erap usb hcd control failed\n");
		return -1;
	}
	mutex_unlock(&usb_audio->lock);

	return 0;
}

int exynos_usb_audio_desc(struct usb_device *udev)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	struct usb_host_config *host_cfg = udev->config;
	struct usb_config_descriptor cfg_desc =  host_cfg->desc;
	int ret;

	int cfgno = cfg_desc.bConfigurationValue;
	unsigned char *buffer;
	unsigned int len = udev->rawdesc_length;
	u64 desc_addr;
	u64 offset;

	if (DEBUG)
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s\n", __func__);

	/* need to memory mapping for usb descriptor */
	buffer = udev->rawdescriptors[cfgno-1];
	desc_addr = virt_to_phys(buffer);
	offset = desc_addr % PAGE_SIZE;

	/* store address information */
	usb_audio->desc_addr = desc_addr;
	usb_audio->offset = offset;

	desc_addr -= offset;

	mutex_lock(&usb_audio->lock);
	ret = abox_iommu_map(dev, USB_AUDIO_DESC, desc_addr, (PAGE_SIZE * 2));
	if (ret) {
		dev_err(&udev->dev, "USB AUDIO: abox iommu mapping for usb descriptor is failed\n");
		return ret;
	}

	/* notify to Abox descriptor is ready*/
	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_DESC;

	erap_usb->param1 = 1;
	erap_usb->param2 = len;
	erap_usb->param3 = offset;
	erap_usb->param4 = usb_audio->speed;

	if (DEBUG)
		dev_info(&udev->dev, "paddr : %#08llx / offset : %#08llx / len : %d / speed : %d\n",
							desc_addr + offset , offset, len, usb_audio->speed);

	ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
	if (ret) {
		dev_err(&usb_audio->udev->dev, "erap usb desc  control failed\n");
		return -1;
	}
	mutex_unlock(&usb_audio->lock);

	dev_info(&udev->dev, "USB AUDIO: Mapping descriptor for using on Abox USB F/W & Nofity mapping is done!");

	return 0;
}

int exynos_usb_audio_conn(int is_conn)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	int ret;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	pr_info("USB DEVICE IS %s\n", is_conn? "CONNECTION" : "DISCONNECTION");

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_CONN;

	erap_usb->param1 = is_conn;

	if (!is_conn) {
		if (usb_audio->is_audio) {
			ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
			if (ret) {
				pr_err("erap usb dis_conn control failed\n");
				return -1;
			}

			wait_for_completion_timeout(&usb_audio->out_task_done,
						msecs_to_jiffies(1000));
			/* free mapped iommu & memory */
			ret = exynos_usb_audio_exit();
			if (ret) {
				pr_err("ERROR : usb audio exit is failed\n");
				return -1;
			}
			usb_audio->is_audio = 0;
		} else {
			pr_err("Is not USB Audio device\n");
		}
	} else {
		ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
		if (ret) {
			pr_err("erap usb conn control failed\n");
			return -1;
		}

	}

	return 0;
}

int exynos_usb_audio_pcm(int is_open, int direction)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	int ret;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	dev_info(dev, "PCM  %s\n", is_open? "OPEN" : "CLOSE");

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_PCM_OPEN;

	erap_usb->param1 = is_open;
	erap_usb->param2 = direction;

	ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
	if (ret) {
		dev_err(&usb_audio->udev->dev, "ERAP USB PCM control failed\n");
		return -1;
	}

	return 0;
}

int exynos_usb_audio_l2(int is_l2)
{
	ABOX_IPC_MSG msg;
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_USB_AUDIO_PARAM *erap_usb = &erap_msg->param.usbaudio;
	int ret;

	if (!usb_audio->is_audio)
		return 0;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);
	dev_info(dev, " USB Audio  %s the L2 power mode\n", is_l2 ? "ENTER" : "EXIT");

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_USB;
	erap_usb->type = IPC_USB_L2;

	/* 1: device entered L2 , 0: device exited L2 */
	erap_usb->param1 = is_l2;

	ret = abox_start_ipc_transaction(dev, msg.ipcid, &msg, sizeof(msg), 0, 1);
	if (ret) {
		dev_err(&usb_audio->udev->dev, "ERAP USB L2 control failed\n");
		return -1;
	}

	return 0;
}

irqreturn_t exynos_usb_audio_irq_handler(int irq, void *dev_id, ABOX_IPC_MSG *msg)
{
	struct IPC_ERAP_MSG *erap_msg = &msg->msg.erap;

	if (DEBUG)
		pr_info("%s: IRQ = %d (5 = ERAP)\n", __func__, irq);

	if ( irq == IPC_ERAP ) {
		if (erap_msg->msgtype == REALTIME_USB) {
			switch (erap_msg->param.usbaudio.type) {
			case IPC_USB_TASK:
				if (erap_msg->param.usbaudio.param1) {
					pr_info("irq : %d /* param1 : 1 , IN EP task done */\n", irq);
					complete(&usb_audio->in_task_done);
				} else {
					pr_info("irq : %d /* param0 : 0 , OUT EP task done */\n", irq);
					complete(&usb_audio->out_task_done);
				}
				break;
			default:
				pr_err("%s: unknown usb msg type\n", __func__);
				break;
			}
		} else {
			pr_err("%s: unknown message type\n", __func__);
		}
	} else {
		pr_err("%s: unknown command\n", __func__);
	}
	/* FIXME */
	return IRQ_HANDLED;
}

int exynos_usb_audio_init(struct device *dev, struct platform_device *pdev)
{
	struct device_node *np = dev->of_node;
	struct device_node *np_abox;
	struct platform_device *pdev_abox;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	usb_audio = kmalloc(sizeof(struct exynos_usb_audio), GFP_KERNEL);

	np_abox =of_parse_phandle(np, "abox", 0);
	if(!np_abox) {
		dev_err(dev, "Failed to get abox device node\n");
		return -EPROBE_DEFER;
	}

	pdev_abox = of_find_device_by_node(np_abox);
	if (!pdev_abox) {
		dev_err(&usb_audio->udev->dev, "Failed to get abox platform device\n");
		return -EPROBE_DEFER;
	}

	mutex_init(&usb_audio->lock);
	init_completion(&usb_audio->out_task_done);
	init_completion(&usb_audio->in_task_done);
	usb_audio->abox = pdev_abox;
	usb_audio->hcd_pdev = pdev;
	usb_audio->udev = NULL;
	usb_audio->is_audio = 0;

	abox_register_irq_handler(&pdev_abox->dev, IPC_ERAP,
				exynos_usb_audio_irq_handler, &usb_audio);

	return 0;
}

int exynos_usb_audio_exit(void)
{
	struct platform_device *pdev = usb_audio->abox;
	struct device *dev = &pdev->dev;
	int ret;
	u64 addr;
	u64 offset;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	/* unmapping in pcm buffer */
	addr = usb_audio->in_buf_addr;
	if (DEBUG)
		pr_info("PCM IN BUFFER FREE: paddr = %#08llx\n", addr);

	ret = abox_iommu_unmap(dev, USB_AUDIO_PCM_INBUF, addr, PAGE_SIZE * 15);
	if (ret) {
		pr_err("abox iommu unmapping for pcm buf is failed\n");
		return ret;
	}

	/* unmapping usb descriptor */
	addr = usb_audio->desc_addr;
	offset = usb_audio->offset;

	if (DEBUG)
		pr_info("DESC BUFFER :: paddr : %#08llx / offset : %#08llx\n",
				addr, offset);
	ret = abox_iommu_unmap(dev, USB_AUDIO_DESC, addr - offset, (PAGE_SIZE *2));
	if (ret) {
		pr_err("USB AUDIO: abox iommu unmapping for usb descriptor is failed\n");
		return ret;
	}

	ret = abox_iommu_unmap(dev, USB_AUDIO_SAVE_RESTORE, usb_audio->save_dma, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu unmapping for dcbaa is failed\n");
		return ret;
	}

	/*Device Context unmapping*/
	ret = abox_iommu_unmap(dev, USB_AUDIO_DEV_CTX, usb_audio->out_ctx, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu unmapping for device ctx is failed\n");
		return ret;
	}

	/*Input Context unmapping*/
	ret = abox_iommu_unmap(dev, USB_AUDIO_INPUT_CTX, usb_audio->in_ctx, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu unmapping for input ctx is failed\n");
		return ret;
	}

	/*ERST unmapping*/
	ret = abox_iommu_unmap(dev, USB_AUDIO_ERST, usb_audio->erst_addr, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu Un-mapping for erst is failed\n");
		return ret;
	}

	if ( DEBUG ) {
		dev_info(dev, "USB_AUDIO_IPC : SFR MAPPING!\n");
	}
	ret = abox_iommu_unmap(dev, USB_AUDIO_XHCI_BASE, USB_AUDIO_XHCI_BASE, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu Un-mapping for in buf is failed\n");
		return ret;
	}

	return 0;
}

