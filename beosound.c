#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/leds.h>

/*
	The BeoSound 5 is a screen + input device made by Bang & Olufsen.
	It was originally designed to be used with a BeoMaster 5, but can also be
	used as a standalone device.

	The input device consists of some buttons, a "laser pointer" which offers
	absolute positioning, and two infinite wheels.

	The screen (on/off) and indicator LED (on/off/blink) can be controlled.

	The beo5 also has an internal speaker that can receive commands to play
	some sounds (tick, tock, beep, and some easter eggs).
*/

#define LASER_MIN 0
#define LASER_MAX 128

#define BIT_SCREEN 0b01000000
#define BIT_LED_BLINK 0b00010000
#define BIT_LED_SOLID 0b10000000
#define BIT_TICK 0b00000001
#define BIT_TOCK 0b00000110

#define BIT_BUTTON_RIGHT (1<<4)
#define BIT_BUTTON_LEFT  (1<<5)
#define BIT_BUTTON_GO    (1<<6)
#define BIT_BUTTON_POWER (1<<7)

#define DEVICE_NAME "beosound5"


/*
00000001 tick
00000100 beep wahaou
00000110 tock
00000111 tuck
00001011 doh!
00001100 non-stop beep
00001101 non-stop lighter beep
*/

struct beosound
{
	struct input_dev *input;
	struct led_classdev screen;
	struct led_classdev indicator;
	char status_field;
};

static void send_status(struct hid_device *hdev, struct beosound *beosound)
{
	char buf[2];
	buf[0] = 0;
	buf[1] = beosound->status_field;
	hid_info(hdev, ">>>>>> Will send cmd %x\n", beosound->status_field);
	int ret = hid_hw_raw_request(hdev, buf[0], buf, 2, // sizeof(buf),
								 HID_OUTPUT_REPORT, HID_REQ_SET_REPORT);

	hid_info(hdev, ">>>>>> SND with %d -> %d\n", beosound->status_field, ret);
}

static void brightness_set(struct led_classdev *led_cdev,
						   enum led_brightness brightness)
{
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = to_hid_device(dev);
	struct beosound *beosound = hid_get_drvdata(hdev);

	if (led_cdev == &beosound->screen)
	{
		hid_info(hdev, ">>>>>> BRIGHTNESS SCREEN %d\n", brightness);
		if (brightness)
		{
			beosound->status_field |= BIT_SCREEN;
		}
		else
		{
			beosound->status_field &= ~BIT_SCREEN;
		}
	}
	else if(led_cdev == &beosound->indicator)
	{
		hid_info(hdev, ">>>>>> BRIGHTNESS LED %d\n", brightness);
		int brightness_val = (int8_t)brightness;
		switch (brightness_val)
		{
			case 0: // led off
				beosound->status_field &= ~BIT_LED_BLINK;
				beosound->status_field &= ~BIT_LED_SOLID;
				break;
			case 1: // led solid
				beosound->status_field &= ~BIT_LED_BLINK;
				beosound->status_field |= BIT_LED_SOLID;
				break;
			case 2: // led blink
				beosound->status_field |= BIT_LED_BLINK;
				beosound->status_field &= ~BIT_LED_SOLID;
				break;
			default:
				break;
		}
	}

	send_status(hdev, beosound);
}

static int beo5_probe(struct hid_device *hdev,
						const struct hid_device_id *id)
{
	struct beosound *ssc;
	int ret;

	ssc = devm_kzalloc(&hdev->dev, sizeof(*ssc), GFP_KERNEL);
	if (ssc == NULL)
	{
		hid_err(hdev, "can't alloc beosound descriptor\n");
		return -ENOMEM;
	}

	ssc->status_field = BIT_SCREEN;

	hdev->quirks |= HID_QUIRK_HIDINPUT_FORCE;

	hid_set_drvdata(hdev, ssc);

	ret = hid_parse(hdev);
	if (ret)
	{
		hid_err(hdev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret)
	{
		hid_err(hdev, "hw start failed\n");
		return ret;
	}

	// Alloc the two led structures

	// Screen backlight (only on or off)
	ssc->screen.name = DEVICE_NAME "::backlight";
	ssc->screen.max_brightness = 1;
	ssc->screen.brightness_set = brightness_set;
	ret = led_classdev_register(&hdev->dev, &ssc->screen);
	if (ret)
	{
		hid_err(hdev, "Could not register screen backlight device\n");
		return 1;
	}

	// Indicator LED
	ssc->indicator.name = DEVICE_NAME "::indicator";
	ssc->indicator.max_brightness = 2; // off, blink, solid
	ssc->indicator.brightness_set = brightness_set;
	ret = led_classdev_register(&hdev->dev, &ssc->indicator);
	if (ret)
	{
		hid_err(hdev, "Could not register indicator device\n");
		return 1;
	}

	return 0;
}

static int beo5_raw_event(struct hid_device *hdev,
							struct hid_report *report, u8 *raw_data, int size)
{
	struct beosound *ssc = hid_get_drvdata(hdev);

	if (size < 6)
	{
		hid_info(hdev, " Data not long enough\n");
		return -1;
	}

	int right = raw_data[3] & BIT_BUTTON_RIGHT;
	int left  = raw_data[3] & BIT_BUTTON_LEFT;
	int go    = raw_data[3] & BIT_BUTTON_GO;
	int power = raw_data[3] & BIT_BUTTON_POWER;

	input_event(ssc->input, EV_KEY, BTN_DPAD_RIGHT, right ? 1 : 0);
	input_event(ssc->input, EV_KEY, BTN_DPAD_LEFT, left ? 1 : 0);
	input_event(ssc->input, EV_KEY, BTN_SELECT, go ? 1 : 0);
	input_event(ssc->input, EV_KEY, BTN_MODE, power ? 1 : 0);


	int laser_pos = raw_data[2];
	if (laser_pos > LASER_MAX)
	{
		laser_pos = LASER_MAX;
	}
	input_report_abs(ssc->input, ABS_X, laser_pos);

	int selection_wheel = (int8_t)raw_data[0];
	int volume_wheel    = (int8_t)raw_data[1];

	input_report_rel(ssc->input, REL_Y, selection_wheel);
	input_report_rel(ssc->input, REL_Z, volume_wheel);


	return 0;
}


static int input_cfgd(struct hid_device *hdev,
					  struct hid_input *hidinput)
{
	struct input_dev *input_dev = hidinput->input;
	struct beosound *drvd = hid_get_drvdata(hdev);

	if (!drvd)
	{
		hid_err(hdev, "no driver data\n");
		return -ENODEV;
	}

	drvd->input = input_dev;

	// Laser selector is absolute
	input_set_abs_params(input_dev, ABS_X, LASER_MIN, LASER_MAX, 0, 0);

	// Selection wheel + "volume"/2ndary selection wheel
	input_set_capability(input_dev, EV_REL, REL_Y);
	input_set_capability(input_dev, EV_REL, REL_Z);

	// Left button
	input_set_capability(input_dev, EV_KEY, BTN_DPAD_LEFT);
	// Right button
	input_set_capability(input_dev, EV_KEY, BTN_DPAD_RIGHT);
	// "GO" button
	input_set_capability(input_dev, EV_KEY, BTN_SELECT);
	// Power/Sleep/Master button
	input_set_capability(input_dev, EV_KEY, BTN_MODE);


	return 0;
}

static void beosound_remove(struct hid_device *hdev)
{
	struct beosound *beo = hid_get_drvdata(hdev);

	led_classdev_unregister(&beo->screen);
	led_classdev_unregister(&beo->indicator);
	hid_hw_stop(hdev);
}

static const struct hid_device_id beo5_devices[] = {
	{HID_USB_DEVICE(0x0cd4, 0x1112),
	 .driver_data = 1},
	{}};

MODULE_DEVICE_TABLE(hid, beo5_devices);

static struct hid_driver beo5_driver = {
	.name = "beosound5",
	.id_table = beo5_devices,
	.probe = beo5_probe,
	.input_configured = input_cfgd,
	.raw_event = beo5_raw_event,
	.remove = beosound_remove,
};
module_hid_driver(beo5_driver);

MODULE_LICENSE("GPL");
