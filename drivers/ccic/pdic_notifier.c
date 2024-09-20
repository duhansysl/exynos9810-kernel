#include <linux/device.h>

#include <linux/notifier.h>
#include <linux/ccic/pdic_notifier.h>

#define SET_PDIC_NOTIFIER_BLOCK(nb, fn, dev) do {	\
		(nb)->notifier_call = (fn);		\
		(nb)->priority = (dev);			\
	} while (0)

#define DESTROY_PDIC_NOTIFIER_BLOCK(nb)			\
		SET_PDIC_NOTIFIER_BLOCK(nb, NULL, -1)

static struct s2m_pdic_notifier_struct pdic_notifier;

int s2m_pdic_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			muic_notifier_device_t listener)
{
	int ret = 0;

	pr_info("%s: listener=%d register\n", __func__, listener);

	SET_PDIC_NOTIFIER_BLOCK(nb, notifier, listener);
	ret = blocking_notifier_chain_register(&(pdic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_register error(%d)\n",
				__func__, ret);

	/* current pdic's attached_device status notify */
	nb->notifier_call(nb, pdic_notifier.cmd,
			&(pdic_notifier.attached_dev));

	return ret;
}

int s2m_pdic_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	ret = blocking_notifier_chain_unregister(&(pdic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_unregister error(%d)\n",
				__func__, ret);
	DESTROY_PDIC_NOTIFIER_BLOCK(nb);

	return ret;
}

static int s2m_pdic_notifier_notify(void)
{
	int ret = 0;

	pr_info("%s: CMD=%d, DATA=%d\n", __func__, pdic_notifier.cmd,
			pdic_notifier.attached_dev);

	ret = blocking_notifier_call_chain(&(pdic_notifier.notifier_call_chain),
			pdic_notifier.cmd, &(pdic_notifier.attached_dev));

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

void s2m_pdic_notifier_attach_attached_jig_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

	pdic_notifier.cmd = PDIC_MUIC_NOTIFY_CMD_JIG_ATTACH;
	pdic_notifier.attached_dev = new_dev;

	/* pdic's attached_device attach broadcast */
	s2m_pdic_notifier_notify();
}
#if 0
void s2m_pdic_notifier_detach_attached_jig_dev(muic_attached_dev_t cur_dev)
{
	pr_info("%s: (%d)\n", __func__, cur_dev);

	pdic_notifier.cmd = PDIC_MUIC_NOTIFY_CMD_JIG_DETACH;

	if (pdic_notifier.attached_dev != cur_dev)
		pr_warn("%s: attached_dev of pdic_notifier(%d) != pdic_data(%d)\n",
				__func__, pdic_notifier.attached_dev, cur_dev);

	if (pdic_notifier.attached_dev != ATTACHED_DEV_NONE_MUIC) {
		/* pdic's attached_device detach broadcast */
		s2m_pdic_notifier_notify();
	}

	pdic_notifier.attached_dev = ATTACHED_DEV_NONE_MUIC;
}
#endif
void s2m_pdic_notifier_attach_attached_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

	pdic_notifier.cmd = MUIC_NOTIFY_CMD_ATTACH;
	pdic_notifier.attached_dev = new_dev;

	/* pdic's attached_device attach broadcast */
	s2m_pdic_notifier_notify();
}

void s2m_pdic_notifier_detach_attached_dev(muic_attached_dev_t cur_dev)
{
	pr_info("%s: (%d)\n", __func__, cur_dev);

	pdic_notifier.cmd = MUIC_NOTIFY_CMD_DETACH;

	if (pdic_notifier.attached_dev != cur_dev)
		pr_warn("%s: attached_dev of pdic_notifier(%d) != pdic_data(%d)\n",
				__func__, pdic_notifier.attached_dev, cur_dev);

	if (pdic_notifier.attached_dev != ATTACHED_DEV_NONE_MUIC) {
		/* pdic's attached_device detach broadcast */
		s2m_pdic_notifier_notify();
	}

	pdic_notifier.attached_dev = ATTACHED_DEV_NONE_MUIC;
}

void s2m_pdic_notifier_logically_attach_attached_dev(muic_attached_dev_t new_dev)
{
	pr_info("%s: (%d)\n", __func__, new_dev);

	pdic_notifier.cmd = MUIC_NOTIFY_CMD_LOGICALLY_ATTACH;
	pdic_notifier.attached_dev = new_dev;

	/* pdic's attached_device attach broadcast */
	s2m_pdic_notifier_notify();
}

void s2m_pdic_notifier_logically_detach_attached_dev(muic_attached_dev_t cur_dev)
{
	pr_info("%s: (%d)\n", __func__, cur_dev);

	pdic_notifier.cmd = MUIC_NOTIFY_CMD_LOGICALLY_DETACH;
	pdic_notifier.attached_dev = cur_dev;

	/* pdic's attached_device detach broadcast */
	s2m_pdic_notifier_notify();

	pdic_notifier.attached_dev = ATTACHED_DEV_NONE_MUIC;
}

static int __init s2m_pdic_notifier_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	BLOCKING_INIT_NOTIFIER_HEAD(&(pdic_notifier.notifier_call_chain));
	pdic_notifier.cmd = MUIC_NOTIFY_CMD_DETACH;
	pdic_notifier.attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;

	return ret;
}
subsys_initcall(s2m_pdic_notifier_init);

