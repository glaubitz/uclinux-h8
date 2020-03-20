/*
 *  X68000 68901 serial driver for Linux
 *
 *  Copyright (c) 2016 Yoshinori Sato
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/err.h>
#include <linux/rcupdate.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/io.h>

MODULE_AUTHOR("Yoshinori Sato <ysato@users.sourceforge.jp>");
MODULE_DESCRIPTION("X68000 MFP serial driver");
MODULE_LICENSE("GPL");

#define MFP_USART (void *)0xffe88027
#define MFP_CSR (MFP_USART + 0)
#define MFP_UCR (MFP_USART + 2)
#define MFP_RSR (MFP_USART + 4)
#define MFP_TSR (MFP_USART + 6)
#define MFP_UDR (MFP_USART + 8)

#define MFP_RSR_BF	(0x80)
#define MFP_RSR_OE	(0x40)
#define MFP_RSR_PE	(0x20)
#define MFP_RSR_FE	(0x10)
#define MFP_TSR_BE	(0x80)

#define MFP_CTL_TIMEOUT	1000
#define MFP_KBD_PHYS_DESC "X68Kbus/serio"
#define MFP_KBD_IRQ 20

/*
 * mfp_lock protects serialization between mfp_command and
 * the interrupt handler.
 */
static DEFINE_SPINLOCK(mfp_lock);

/*
 * Writers to AUX and KBD ports as well as users issuing mfp_command
 * directly should acquire mfp_mutex (by means of calling
 * mfp_lock_chip() and mfp_unlock_ship() helpers) to ensure that
 * they do not disturb each other (unfortunately in many mfp
 * implementations write to one of the ports will immediately abort
 * command that is being processed by another port).
 */
static DEFINE_MUTEX(mfp_mutex);

struct mfp_port {
	struct serio *serio;
	int irq;
	bool exists;
	bool driver_bound;
} mfp_port;

static bool mfp_kbd_irq_registered;
static struct platform_device *mfp_platform_device;
static struct notifier_block mfp_kbd_bind_notifier_block;

static irqreturn_t mfp_interrupt(int irq, void *dev_id);
/*
 * The mfp_wait_read() and mfp_wait_write functions wait for the mfp to
 * be ready for reading values from it / writing values to it.
 * Called always with mfp_lock held.
 */

static int mfp_wait_write(void)
{
	int i = 0;

	while (!(__raw_readb(MFP_TSR) & MFP_TSR_BE) && (i < MFP_CTL_TIMEOUT)) {
		udelay(50);
		i++;
	}
	return -(i == MFP_CTL_TIMEOUT);
}

static int mfp_write(struct serio *port, unsigned char c)
{
	unsigned long flags;
	int retval = 0;

	spin_lock_irqsave(&mfp_lock, flags);

	if (!(retval = mfp_wait_write())) {
		__raw_writeb(c, MFP_UDR);
	}

	spin_unlock_irqrestore(&mfp_lock, flags);

	return retval;
}

/*
 * mfp_interrupt() is the most important function in this driver -
 * it handles the interrupts from the mfp, and sends incoming bytes
 * to the upper layers.
 */

static irqreturn_t mfp_interrupt(int irq, void *dev_id)
{
	unsigned long flags;
	unsigned char rsr, data;
	unsigned int dfl;

	spin_lock_irqsave(&mfp_lock, flags);

	rsr = __raw_readb(MFP_RSR);
	data = __raw_readb(MFP_UDR);


	dfl = (rsr & (MFP_RSR_PE | MFP_RSR_OE | MFP_RSR_FE));

	spin_unlock_irqrestore(&mfp_lock, flags);

	serio_interrupt(mfp_port.serio, data, dfl);

	return IRQ_HANDLED;
}

/*
 * mfp_panic_blink() will turn the keyboard LEDs on or off and is called
 * when kernel panics. Flashing LEDs is useful for users running X who may
 * not see the console and will help distinguishing panics from "real"
 * lockups.
 *
 * Note that DELAY has a limit of 10ms so we will not get stuck here
 * waiting for KBC to free up even if KBD interrupt is off
 */

#define DELAY do { mdelay(1); if (++delay > 10) return delay; } while(0)

static long mfp_panic_blink(int state)
{
	long delay = 0;
	char led;

	led = (state) ? 0x01 | 0xff : 0x80;
	while (!(__raw_readb(MFP_TSR) & MFP_TSR_BE))
		DELAY;
	__raw_writeb(0x80, MFP_UDR);
	DELAY;
	while (!(__raw_readb(MFP_TSR) & MFP_TSR_BE))
		DELAY;
	DELAY;
	__raw_writeb(led, MFP_UDR);
	DELAY;
	return delay;
}

#undef DELAY

static int __init mfp_create_kbd_port(void)
{
	struct serio *serio;

	serio = kzalloc(sizeof(struct serio), GFP_KERNEL);
	if (!serio)
		return -ENOMEM;

	serio->id.type		= SERIO_X68K_MFP;
	serio->write		= mfp_write;
	serio->port_data	= &mfp_port;
	serio->dev.parent	= &mfp_platform_device->dev;
	strlcpy(serio->name, "X68000 MFP serial", sizeof(serio->name));
	strlcpy(serio->phys, MFP_KBD_PHYS_DESC, sizeof(serio->phys));

	mfp_port.serio = serio;
	mfp_port.irq = MFP_KBD_IRQ;

	return 0;
}

static void __init mfp_free_kbd_port(void)
{
	kfree(mfp_port.serio);
	mfp_port.serio = NULL;
}

static void __init mfp_register_ports(void)
{
	struct serio *serio = mfp_port.serio;

	if (serio) {
		printk(KERN_INFO "serio: %s at %#lx irq %d\n",
		       serio->name,
		       (unsigned long) MFP_UDR,
		       mfp_port.irq);
		serio_register_port(serio);
		device_set_wakeup_capable(&serio->dev, true);
	}
}

static void mfp_unregister_ports(void)
{
	if (mfp_port.serio) {
		serio_unregister_port(mfp_port.serio);
		mfp_port.serio = NULL;
	}
}

static void mfp_free_irqs(void)
{
	if (mfp_kbd_irq_registered)
		free_irq(MFP_KBD_IRQ, mfp_platform_device);

	mfp_kbd_irq_registered = false;
}

static int __init mfp_setup_kbd(void)
{
	int error;

	error = mfp_create_kbd_port();
	if (error)
		return error;

	error = request_irq(MFP_KBD_IRQ, mfp_interrupt, IRQF_SHARED,
			    "mfp-serial", mfp_platform_device);
	if (error)
		goto err_free_port;

	mfp_kbd_irq_registered = true;
	return 0;

 err_free_port:
	mfp_free_kbd_port();
	return error;
}

static int mfp_kbd_bind_notifier(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct device *dev = data;
	struct serio *serio = to_serio_port(dev);
	struct mfp_port *port = serio->port_data;

	if (serio != mfp_port.serio)
		return 0;

	switch (action) {
	case BUS_NOTIFY_BOUND_DRIVER:
		port->driver_bound = true;
		break;

	case BUS_NOTIFY_UNBIND_DRIVER:
		port->driver_bound = false;
		break;
	}

	return 0;
}

static int __init mfp_probe(struct platform_device *dev)
{
	int error;

	mfp_platform_device = dev;

	error = mfp_setup_kbd();
	if (error)
		goto out_fail;
/*
 * Ok, everything is ready, let's register all serio ports
 */
	mfp_register_ports();

	return 0;

 out_fail:
	mfp_free_irqs();
	mfp_platform_device = NULL;

	return error;
}

static int mfp_remove(struct platform_device *dev)
{
	mfp_unregister_ports();
	mfp_free_irqs();
	mfp_platform_device = NULL;

	return 0;
}

static struct platform_driver mfp_driver = {
	.driver		= {
		.name	= "X68K-mfp",
	},
	.remove		= mfp_remove,
};

static struct notifier_block mfp_kbd_bind_notifier_block = {
	.notifier_call = mfp_kbd_bind_notifier,
};

static int __init mfp_init(void)
{
	struct platform_device *pdev;
	int err;

	pdev = platform_create_bundle(&mfp_driver, mfp_probe, NULL, 0, NULL, 0);
	if (IS_ERR(pdev)) {
		err = PTR_ERR(pdev);
		goto err_platform_exit;
	}

	bus_register_notifier(&serio_bus, &mfp_kbd_bind_notifier_block);
	panic_blink = mfp_panic_blink;

	return 0;

 err_platform_exit:
	return err;
}

static void __exit mfp_exit(void)
{
	platform_device_unregister(mfp_platform_device);
	platform_driver_unregister(&mfp_driver);

	bus_unregister_notifier(&serio_bus, &mfp_kbd_bind_notifier_block);
	panic_blink = NULL;
}

module_init(mfp_init);
module_exit(mfp_exit);
