
#define pr_fmt(fmt)	"[MUIC] " fmt

#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/switch.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/sec_class.h>

/*
  * The src & dest addresses of the noti.
  * keep the same value defined in ccic_notifier.h
  *     b'0001 : CCIC
  *     b'0010 : MUIC
  *     b'1111 : Broadcasting
  */
#define NOTI_ADDR_SRC (1 << 1)
#define NOTI_ADDR_DST (0xf)

/* ATTACH Noti. ID */
#define NOTI_ID_ATTACH (1)

#define SET_MUIC_NOTIFIER_BLOCK(nb, fn, dev) do {	\
		(nb)->notifier_call = (fn);		\
		(nb)->priority = (dev);			\
	} while (0)

#define DESTROY_MUIC_NOTIFIER_BLOCK(nb)			\
		SET_MUIC_NOTIFIER_BLOCK(nb, NULL, -1)

static struct muic_notifier_struct muic_notifier;
#ifdef CONFIG_USE_CCIC
static struct muic_notifier_struct muic_ccic_notifier;
#endif

static int muic_uses_new_noti;
#ifdef CONFIG_USE_CCIC
static int muic_ccic_uses_new_noti;
#endif

#if !defined(CONFIG_MUIC_SYSFS)
struct device *switch_device;
#endif

#if IS_ENABLED(CONFIG_SEC_FACTORY)
struct switch_dev switch_muic_dev = {
	.name = "attached_muic_cable",
};

static void send_muic_cable_intent(int type)
{
	pr_info("%s: MUIC attached_muic_cable type(%d)\n", __func__, type);
	switch_set_state(&switch_muic_dev, type);
}
#endif

void muic_notifier_set_new_noti(bool flag)
{
	muic_uses_new_noti = flag ? 1 : 0;
}

static void __set_noti_cxt(int attach, int type)
{
	if (type < 0) {
		muic_notifier.cmd = attach;
#if defined(CONFIG_CCIC_NOTIFIER)
		muic_notifier.cxt.attach = attach;
#endif
		return;
	}

	/* Old Interface */
	muic_notifier.cmd = attach;
	muic_notifier.attached_dev = type;

#if defined(CONFIG_CCIC_NOTIFIER)
	/* New Interface */
	muic_notifier.cxt.src = NOTI_ADDR_SRC;
	muic_notifier.cxt.dest = NOTI_ADDR_DST;
	muic_notifier.cxt.id = NOTI_ID_ATTACH;
	muic_notifier.cxt.attach = attach;
	muic_notifier.cxt.cable_type = type;
	muic_notifier.cxt.rprd = 0;
#endif
}

#ifdef CONFIG_USE_CCIC
static void __set_ccic_noti_cxt(int attach, int type)
{
	if (type < 0) {
		muic_ccic_notifier.cmd = attach;
#if defined(CONFIG_CCIC_NOTIFIER)
		muic_ccic_notifier.cxt.attach = attach;
#endif
		return;
	}

	/* Old Interface */
	muic_ccic_notifier.cmd = attach;
	muic_ccic_notifier.attached_dev = type;

#if defined(CONFIG_CCIC_NOTIFIER)
	/* New Interface */
	muic_ccic_notifier.cxt.src = NOTI_ADDR_SRC;
	muic_ccic_notifier.cxt.dest = NOTI_ADDR_DST;
	muic_ccic_notifier.cxt.id = NOTI_ID_ATTACH;
	muic_ccic_notifier.cxt.attach = attach;
	muic_ccic_notifier.cxt.cable_type = type;
	muic_ccic_notifier.cxt.rprd = 0;
#endif
}
#endif

#if defined(CONFIG_MUIC_SM5504_POGO)
static void __set_pogo_noti_cxt(int attach, int type)
{
#if defined(CONFIG_CCIC_NOTIFIER)
	if (type < 0) {
		muic_notifier.pogo_cxt.attach = attach;
		return;
	}
	/* New Interface */
	muic_notifier.pogo_cxt.src = NOTI_ADDR_SRC;
	muic_notifier.pogo_cxt.dest = NOTI_ADDR_DST;
	muic_notifier.pogo_cxt.id = CCIC_NOTIFY_ID_POGO;
	muic_notifier.pogo_cxt.attach = attach;
	muic_notifier.pogo_cxt.cable_type = type;
	muic_notifier.pogo_cxt.rprd = 0;
#endif
}
#endif /* CONFIG_MUIC_SM5504_POGO */

int muic_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			muic_notifier_device_t listener)
{
	int ret = 0;
#if defined(CONFIG_CCIC_NOTIFIER)
	void *pcxt;
#endif

	pr_info("%s: listener=%d register\n", __func__, listener);

	SET_MUIC_NOTIFIER_BLOCK(nb, notifier, listener);
	ret = blocking_notifier_chain_register(&(muic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_register error(%d)\n",
				__func__, ret);

#if defined(CONFIG_CCIC_NOTIFIER)
	pcxt = muic_uses_new_noti ? &(muic_notifier.cxt) :
			(void *)&(muic_notifier.attached_dev);

	/* current muic's attached_device status notify */
	nb->notifier_call(nb, muic_notifier.cxt.attach, pcxt);
#if defined(CONFIG_MUIC_SM5504_POGO)
	nb->notifier_call(nb, muic_notifier.pogo_cxt.attach, &(muic_notifier.pogo_cxt));
#endif /* CONFIG_MUIC_SM5504_POGO */
#else
	nb->notifier_call(nb, muic_notifier.cmd,
			&(muic_notifier.attached_dev));
#endif

	return ret;
}

int muic_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	ret = blocking_notifier_chain_unregister(&(muic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_unregister error(%d)\n",
				__func__, ret);
	DESTROY_MUIC_NOTIFIER_BLOCK(nb);

	return ret;
}

static int muic_notifier_notify(void)
{
	int ret = 0;
#if defined(CONFIG_CCIC_NOTIFIER)
	void *pcxt;

	pr_info("%s: CMD=%d, DATA=%d\n", __func__, muic_notifier.cxt.attach,
			muic_notifier.cxt.cable_type);

	pcxt = muic_uses_new_noti ? &(muic_notifier.cxt) :
			(void *)&(muic_notifier.attached_dev);

	ret = blocking_notifier_call_chain(&(muic_notifier.notifier_call_chain),
			muic_notifier.cxt.attach, pcxt);
#else
	pr_info("%s: CMD=%d, DATA=%d\n", __func__, muic_notifier.cmd,
			muic_notifier.attached_dev);
	ret = blocking_notifier_call_chain(&(muic_notifier.notifier_call_chain),
			muic_notifier.cmd, &(muic_notifier.attached_dev));
#endif

#if IS_ENABLED(CONFIG_SEC_FACTORY)
#if defined(CONFIG_MUIC_SUPPORT_CCIC) && defined(CONFIG_CCIC_NOTIFIER)
	if (muic_notifier.cxt.attach != 0)
		send_muic_cable_intent(muic_notifier.cxt.cable_type);
	else
		send_muic_cable_intent(0);
#if defined(CONFIG_MUIC_SM5504_POGO)
	if (muic_notifier.pogo_cxt.cable_type)
		send_muic_cable_intent(muic_notifier.pogo_cxt.cable_type);
#endif /* CONFIG_MUIC_SM5504_POGO */
#else
	if (muic_notifier.cmd != 0)
		send_muic_cable_intent(muic_notifier.attached_dev);
	else
		send_muic_cable_intent(0);
#endif	/* CONFIG_MUIC_SUPPORT_CCIC */
#endif	/* CONFIG_SEC_FACTORY */

	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error occur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}

#if defined(CONFIG_USE_CCIC)
int muic_ccic_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			muic_notifier_device_t listener)
{
	int ret = 0;
#if defined(CONFIG_CCIC_NOTIFIER)
	void *pcxt;
#endif

	pr_info("%s: listener=%d register\n", __func__, listener);

	SET_MUIC_NOTIFIER_BLOCK(nb, notifier, listener);
	ret = blocking_notifier_chain_register(&(muic_ccic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_register error(%d)\n",
				__func__, ret);

#if defined(CONFIG_CCIC_NOTIFIER)
	pcxt = muic_ccic_uses_new_noti ? &(muic_ccic_notifier.cxt) : \
			(void *)&(muic_ccic_notifier.attached_dev);

	/* current muic's attached_device status notify */
	nb->notifier_call(nb, muic_ccic_notifier.cxt.attach, pcxt);
#else
	nb->notifier_call(nb, muic_ccic_notifier.cmd,
			&(muic_ccic_notifier.attached_dev));
#endif

	return ret;
}

int muic_ccic_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	ret = blocking_notifier_chain_unregister(&(muic_ccic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_unregister error(%d)\n",
				__func__, ret);
	DESTROY_MUIC_NOTIFIER_BLOCK(nb);

	return ret;
}

static int muic_ccic_notifier_notify(void)
{
	int ret = 0;
#if defined(CONFIG_CCIC_NOTIFIER)
	void *pcxt;

	pr_info("%s: CMD=%d, DATA=%d\n", __func__, muic_ccic_notifier.cxt.attach,
			muic_ccic_notifier.cxt.cable_type);

	pcxt = muic_ccic_uses_new_noti ? &(muic_ccic_notifier.cxt) : \
			(void *)&(muic_ccic_notifier.attached_dev);

	ret = blocking_notifier_call_chain(&(muic_ccic_notifier.notifier_call_chain),
			muic_ccic_notifier.cxt.attach, pcxt);
#else
	pr_info("%s: CMD=%d, DATA=%d\n", __func__, muic_ccic_notifier.cmd,
			muic_ccic_notifier.attached_dev);
	ret = blocking_notifier_call_chain(&(muic_ccic_notifier.notifier_call_chain),
			muic_ccic_notifier.cmd, &(muic_ccic_notifier.attached_dev));
#endif

	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error occur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}
#endif

#if defined(CONFIG_MUIC_SM5504_POGO)
static int muic_pogo_notifier_notify(void)
{
	int ret = 0;
#if defined(CONFIG_CCIC_NOTIFIER)
	void *pcxt = &(muic_notifier.pogo_cxt);

	pr_info("%s: CMD=%d, DATA=%d\n", __func__, muic_notifier.pogo_cxt.attach,
			muic_notifier.pogo_cxt.cable_type);

	ret = blocking_notifier_call_chain(&(muic_notifier.notifier_call_chain),
			muic_notifier.pogo_cxt.attach, pcxt);
#endif

#if IS_ENABLED(CONFIG_SEC_FACTORY)
#if defined(CONFIG_MUIC_SUPPORT_CCIC) && defined(CONFIG_CCIC_NOTIFIER)
	if (muic_notifier.pogo_cxt.attach != 0)
		send_muic_cable_intent(muic_notifier.pogo_cxt.cable_type);
	else
		send_muic_cable_intent(0);
	if (muic_notifier.cxt.cable_type)
		send_muic_cable_intent(muic_notifier.cxt.cable_type);
#endif	/* CONFIG_MUIC_SUPPORT_CCIC */
#endif	/* CONFIG_SEC_FACTORY */

	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error occur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}

void muic_pogo_notifier_attach_attached_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

	__set_pogo_noti_cxt(MUIC_NOTIFY_CMD_ATTACH, new_dev);

	/* muic's attached_device attach broadcast */
	muic_pogo_notifier_notify();
}

void muic_pogo_notifier_detach_attached_dev(muic_attached_dev_t cur_dev)
{
	pr_info("%s: (%d)\n", __func__, cur_dev);

	__set_pogo_noti_cxt(MUIC_NOTIFY_CMD_DETACH, -1);

#if defined(CONFIG_CCIC_NOTIFIER)
	if (muic_notifier.pogo_cxt.cable_type != cur_dev)
		pr_warn("%s: attached_dev of muic_notifier(%d) != muic_data(%d)\n",
			__func__, muic_notifier.pogo_cxt.cable_type, cur_dev);

	if (muic_notifier.pogo_cxt.cable_type != ATTACHED_DEV_NONE_MUIC) {
		/* muic's attached_device detach broadcast */
		muic_pogo_notifier_notify();
	}
#endif

	__set_pogo_noti_cxt(0, ATTACHED_DEV_NONE_MUIC);
}
#endif /* CONFIG_MUIC_SM5504_POGO */

void muic_notifier_attach_attached_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

	__set_noti_cxt(MUIC_NOTIFY_CMD_ATTACH, new_dev);

	/* muic's attached_device attach broadcast */
	muic_notifier_notify();
}

void muic_pdic_notifier_attach_attached_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

#ifdef CONFIG_USE_CCIC
	__set_ccic_noti_cxt(MUIC_PDIC_NOTIFY_CMD_ATTACH, new_dev);

	/* muic's attached_device attach broadcast */
	muic_ccic_notifier_notify();
#else
	__set_noti_cxt(MUIC_PDIC_NOTIFY_CMD_ATTACH, new_dev);

	/* muic's attached_device attach broadcast */
	muic_notifier_notify();
#endif
}

void muic_pdic_notifier_detach_attached_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

#ifdef CONFIG_USE_CCIC
	__set_ccic_noti_cxt(MUIC_PDIC_NOTIFY_CMD_DETACH, new_dev);

	/* muic's attached_device attach broadcast */
	muic_ccic_notifier_notify();
#else
	__set_noti_cxt(MUIC_PDIC_NOTIFY_CMD_DETACH, muic_notifier.attached_dev);
	/* muic's attached_device attach broadcast */
	muic_notifier_notify();
#endif
}

void muic_notifier_detach_attached_dev(muic_attached_dev_t cur_dev)
{
	pr_info("%s: (%d)\n", __func__, cur_dev);

	__set_noti_cxt(MUIC_NOTIFY_CMD_DETACH, -1);

#if defined(CONFIG_CCIC_NOTIFIER)
	if (muic_notifier.cxt.cable_type != cur_dev)
		pr_warn("%s: attached_dev of muic_notifier(%d) != muic_data(%d)\n",
			__func__, muic_notifier.cxt.cable_type, cur_dev);

	if (muic_notifier.cxt.cable_type != ATTACHED_DEV_NONE_MUIC) {
		/* muic's attached_device detach broadcast */
		muic_notifier_notify();
	}
#else
	if (muic_notifier.attached_dev != cur_dev)
		pr_warn("%s: attached_dev of muic_notifier(%d) != muic_data(%d)\n",
			__func__, muic_notifier.attached_dev, cur_dev);

	if (muic_notifier.attached_dev != ATTACHED_DEV_NONE_MUIC) {
		/* muic's attached_device detach broadcast */
		muic_notifier_notify();
	}
#endif

	__set_noti_cxt(0, ATTACHED_DEV_NONE_MUIC);
}

void muic_notifier_logically_attach_attached_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

	__set_noti_cxt(MUIC_NOTIFY_CMD_ATTACH, new_dev);

	/* muic's attached_device attach broadcast */
	muic_notifier_notify();
}

void muic_notifier_logically_detach_attached_dev(muic_attached_dev_t cur_dev)
{
	pr_info("%s: (%d)\n", __func__, cur_dev);

	__set_noti_cxt(MUIC_NOTIFY_CMD_DETACH, cur_dev);

	/* muic's attached_device detach broadcast */
	muic_notifier_notify();

	__set_noti_cxt(0, ATTACHED_DEV_NONE_MUIC);
}

static int __init muic_notifier_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);
#if defined(CONFIG_CCIC_NOTIFIER)
	muic_uses_new_noti = 1;
#endif
	BLOCKING_INIT_NOTIFIER_HEAD(&(muic_notifier.notifier_call_chain));
	__set_noti_cxt(0, ATTACHED_DEV_NONE_MUIC);
#ifdef CONFIG_USE_CCIC
	BLOCKING_INIT_NOTIFIER_HEAD(&(muic_ccic_notifier.notifier_call_chain));
	__set_ccic_noti_cxt(0, ATTACHED_DEV_NONE_MUIC);
	muic_ccic_uses_new_noti = 1;
#endif
#if defined(CONFIG_MUIC_SM5504_POGO)
	__set_pogo_noti_cxt(0, ATTACHED_DEV_NONE_MUIC);
#endif /* CONFIG_MUIC_SM5504_POGO */

#if !defined(CONFIG_MUIC_SYSFS)
	switch_device = sec_device_create(NULL, "switch");
	if (IS_ERR(switch_device)) {
		pr_err("%s Failed to create device(switch)!\n", __func__);
		ret = -ENODEV;
		return ret;
	}
#endif

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	ret = switch_dev_register(&switch_muic_dev);
	if (ret < 0)
		pr_err("%s: Failed to register attached_muic_cable switch(%d)\n",
				__func__, ret);
#endif
	return ret;
}
device_initcall(muic_notifier_init);


