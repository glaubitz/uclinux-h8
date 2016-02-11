/*
 *  Copyright (C) 2016 Yoshinori Sato
 *
 *  Based on amikbd.c
 */

/*
 * X68000 keyboard driver for Linux/m68k
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/keyboard.h>
#include <linux/serio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#define X68KKBD_LED_EVENT_BIT	0
#define X68KKBD_REP_EVENT_BIT	1

#define DRIVER_DESC	"X68000 keyboard driver"

MODULE_AUTHOR("Yoshinori Sato <ysato@users.sourceforge.jp>");
MODULE_DESCRIPTION("X68000 keyboard driver");
MODULE_LICENSE("GPL");

static const unsigned char x68kkbd_keycode[0x73] = {
	[1]	 = KEY_ESC,
	[2]	 = KEY_1,
	[3]	 = KEY_2,
	[4]	 = KEY_3,
	[5]	 = KEY_4,
	[6]	 = KEY_5,
	[7]	 = KEY_6,
	[8]	 = KEY_7,
	[9]	 = KEY_8,
	[10]	 = KEY_9,
	[11]	 = KEY_0,
	[12]	 = KEY_MINUS,
	[13]	 = KEY_GRAVE,
	[14]	 = KEY_BACKSLASH,
	[15]	 = KEY_BACKSPACE,
	[16]	 = KEY_TAB,
	[17]	 = KEY_Q,
	[18]	 = KEY_W,
	[19]	 = KEY_E,
	[20]	 = KEY_R,
	[21]	 = KEY_T,
	[22]	 = KEY_Y,
	[23]	 = KEY_U,
	[24]	 = KEY_I,
	[25]	 = KEY_O,
	[26]	 = KEY_P,
	[27]	 = KEY_LEFTBRACE,
	[28]	 = KEY_RIGHTBRACE,
	[29]	 = KEY_ENTER,
	[30]	 = KEY_A,
	[31]	 = KEY_S,
	[32]	 = KEY_D,
	[33]	 = KEY_F,
	[34]	 = KEY_G,
	[35]	 = KEY_H,
	[36]	 = KEY_J,
	[37]	 = KEY_K,
	[38]	 = KEY_L,
	[39]	 = KEY_SEMICOLON,
	[40]	 = KEY_APOSTROPHE,
	[42]	 = KEY_Z,
	[43]	 = KEY_X,
	[44]	 = KEY_C,
	[45]	 = KEY_V,
	[46]	 = KEY_B,
	[47]	 = KEY_N,
	[48]	 = KEY_M,
	[49]	 = KEY_COMMA,
	[50]	 = KEY_DOT,
	[51]	 = KEY_SLASH,
	[53]	 = KEY_SPACE,
	[54]	 = KEY_HOME,
	[55]	 = KEY_DELETE,
	[56]	 = KEY_PAGEDOWN,
	[57]	 = KEY_PAGEUP,
	[58]	 = KEY_END,
	[59]	 = KEY_RIGHT,
	[60]	 = KEY_UP,
	[61]	 = KEY_RIGHT,
	[62]	 = KEY_DOWN,
	[64]	 = KEY_KPSLASH,
	[65]	 = KEY_KPASTERISK,
	[66]	 = KEY_KPMINUS,
	[67]	 = KEY_KP7,
	[68]	 = KEY_KP8,
	[69]	 = KEY_KP9,
	[70]	 = KEY_KPPLUS,
	[71]	 = KEY_KP4,
	[72]	 = KEY_KP5,
	[73]	 = KEY_KP6,
	[74]	 = KEY_KPEQUAL,
	[75]	 = KEY_KP1,
	[76]	 = KEY_KP2,
	[77]	 = KEY_KP3,
	[78]	 = KEY_KPENTER,
	[79]	 = KEY_KP0,
	[80]	 = KEY_KPCOMMA,
	[81]	 = KEY_KPDOT,
	[86]	 = KEY_LEFTALT,
	[87]	 = KEY_RIGHTALT,
	[90]	 = KEY_F11,
	[91]	 = KEY_F12,
	[94]	 = KEY_INSERT,
	[97]	 = KEY_PAUSE,
	[98]	 = KEY_SYSRQ,
	[99]	 = KEY_F1,
	[100]	 = KEY_F2,
	[101]	 = KEY_F3,
	[102]	 = KEY_F4,
	[103]	 = KEY_F5,
	[104]	 = KEY_F6,
	[105]	 = KEY_F7,
	[106]	 = KEY_F8,
	[107]	 = KEY_F9,
	[108]	 = KEY_F10,
	[112]	 = KEY_LEFTSHIFT,
	[113]	 = KEY_LEFTCTRL,
};

struct x68kkbd {
	struct serio *serio;
	struct input_dev *dev;

	/* Written only during init */
	char name[64];
	char phys[32];

	unsigned short id;
	DECLARE_BITMAP(force_release_mask, 0x80);
	unsigned char set;
	bool translated;
	bool extra;
	bool write;
	bool softrepeat;
	bool softraw;
	bool scroll;
	bool enabled;

	/* Accessed only from interrupt */
	unsigned char emul;
	bool resend;
	bool release;
	unsigned long xl_bit;
	unsigned int last;
	unsigned long time;
	unsigned long err_count;

	struct delayed_work event_work;
	unsigned long event_jiffies;
	unsigned long event_mask;

	/* Serializes reconnect(), attr->set() and event work */
	struct mutex mutex;
};

static irqreturn_t x68kkbd_interrupt(struct serio *serio, unsigned char data,
				   unsigned int flags)
{
	struct x68kkbd *x68kkbd = serio_get_drvdata(serio);
	struct input_dev *dev = x68kkbd->dev;
	unsigned char scancode, down;

	scancode = data;
	down = !(scancode & 0x80);
	scancode &= 0x7f;

	if (scancode < 0x73) {
		input_report_key(dev, x68kkbd_keycode[scancode], down);
		if (data & 0x80)
			input_sync(dev);
	}
	return IRQ_HANDLED;
}

static void x68kkbd_command(struct serio *serio, unsigned char cmd)
{
	if (serio->write)
		serio->write(serio, cmd);
}

static int x68kkbd_set_repeat_rate(struct x68kkbd *x68kkbd)
{
	const short period[32] =
		{  30,  35,  50,  75,  110,  155,  210,  275, 350, 435, 530,
		   635, 750, 875, 1010, 1155};
	const short delay[] =
		{ 200, 300, 400, 500, 600, 700, 800, 900,
		  1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700 };

	struct input_dev *dev = x68kkbd->dev;
	int i = 0, j = 0;

	while (i < ARRAY_SIZE(period) - 1 && period[i] < dev->rep[REP_PERIOD])
		i++;
	dev->rep[REP_PERIOD] = period[i];
	x68kkbd_command(x68kkbd->serio, 0x70 | i); 

	while (j < ARRAY_SIZE(delay) - 1 && delay[j] < dev->rep[REP_DELAY])
		j++;
	dev->rep[REP_DELAY] = delay[j];

	x68kkbd_command(x68kkbd->serio, 0x60 | j);
	return 0;
}

static int x68kkbd_set_leds(struct x68kkbd *x68kkbd)
{
	struct input_dev *dev = x68kkbd->dev;
	unsigned char led;

	led =  (test_bit(LED_CAPSL,   dev->led) ? 8 : 0);
	x68kkbd_command(x68kkbd->serio, 0x80 | led);
	return 0;
}

static void x68kkbd_event_work(struct work_struct *work)
{
	struct x68kkbd *x68kkbd = container_of(work, struct x68kkbd, event_work.work);

	mutex_lock(&x68kkbd->mutex);

	if (test_and_clear_bit(X68KKBD_LED_EVENT_BIT, &x68kkbd->event_mask))
		x68kkbd_set_leds(x68kkbd);

	if (test_and_clear_bit(X68KKBD_REP_EVENT_BIT, &x68kkbd->event_mask))
		x68kkbd_set_repeat_rate(x68kkbd);

	mutex_unlock(&x68kkbd->mutex);
}

static int x68kkbd_connect(struct serio *serio, struct serio_driver *drv)
{
	struct x68kkbd *x68kkbd;
	struct input_dev *dev;
	int err = -ENOMEM;
	int i;

	x68kkbd = kzalloc(sizeof(struct x68kkbd), GFP_KERNEL);
	dev = input_allocate_device();
	if (!x68kkbd || !dev)
		goto fail1;

	x68kkbd->dev = dev;
	x68kkbd->serio = serio;
	x68kkbd->dev->name = "X68000 Keyboard";
	INIT_DELAYED_WORK(&x68kkbd->event_work, x68kkbd_event_work);
	mutex_init(&x68kkbd->mutex);
	dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	for (i = 0; i < sizeof(x68kkbd_keycode); i++)
		if (x68kkbd_keycode[i])
			set_bit(i, dev->keybit);

	serio_set_drvdata(serio, x68kkbd);

	err = serio_open(serio, drv);
	if (err)
		goto fail2;

	err = input_register_device(x68kkbd->dev);
	if (err)
		goto fail3;

	return 0;

 fail3:	serio_close(serio);
 fail2:	serio_set_drvdata(serio, NULL);
 fail1:	input_free_device(dev);
	kfree(x68kkbd);
	return err;
}

static struct serio_device_id x68kkbd_serio_ids[] = {
	{
		.type	= SERIO_X68K_MFP,
		.proto	= SERIO_ANY,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ }
};

MODULE_DEVICE_TABLE(serio, x68kkbd_serio_ids);

static struct serio_driver x68kkbd_drv = {
	.driver		= {
		.name	= "x68kkbd",
	},
	.description	= DRIVER_DESC,
	.id_table	= x68kkbd_serio_ids,
	.interrupt	= x68kkbd_interrupt,
	.connect	= x68kkbd_connect,
};

static int __init x68kkbd_init(void)
{
	return serio_register_driver(&x68kkbd_drv);
}

static void __exit x68kkbd_exit(void)
{
	serio_unregister_driver(&x68kkbd_drv);
}

module_init(x68kkbd_init);
module_exit(x68kkbd_exit);
