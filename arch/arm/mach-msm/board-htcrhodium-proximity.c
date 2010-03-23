/*
* board-htcrhodium-proximity.c: Support for rhod's proximity sensor
*
* Licensed under GNU GPLv2
*/

#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/input.h>
#include <asm/gpio.h>
#include "proc_comm_wince.h"

#define PROXIMITY_GPIO 90 // 0x5a
static struct input_dev *inputdev;

static int prox_on=0;
module_param_named(on, prox_on, int, S_IRUGO | S_IWUSR | S_IWGRP);
static int prox_wake=0;
module_param_named(wake, prox_wake, int, S_IRUGO | S_IWUSR | S_IWGRP);

static void rhod_report(void) {
	input_report_abs(inputdev, ABS_DISTANCE, gpio_get_value(PROXIMITY_GPIO));
	input_sync(inputdev);
}

static irqreturn_t rhod_prox_irq(int irq, void *dev_id) {
	rhod_report();
	return IRQ_HANDLED;
}

static int rhod_prox_set(int status) {
	//On  = PULL_DOWN
	//Off = PULL_UP
	msm_gpio_set_function(
			DEX_GPIO_CFG(PROXIMITY_GPIO,
				0,
				GPIO_INPUT,
				status ? GPIO_PULL_DOWN : GPIO_PULL_UP,
				GPIO_16MA,
				0));
	return 0;
}

static int rhod_prox_probe(struct platform_device *pdev) {
	gpio_request(PROXIMITY_GPIO, "proximity");
	request_irq(gpio_to_irq(PROXIMITY_GPIO), rhod_prox_irq, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_TRIGGER_LOW, "proximity", NULL);
	//set_irq_wake ?
	set_irq_wake(gpio_to_irq(PROXIMITY_GPIO), prox_wake);
	inputdev=input_allocate_device();
	inputdev->name="rhod_prox";
	set_bit(EV_ABS, inputdev->evbit);
	input_set_abs_params(inputdev, ABS_DISTANCE, 0, 1, 0, 0);
	input_register_device(inputdev);
	//Power it on/off according to cmdline.
	rhod_prox_set(prox_on);
	return 0;
}


static struct platform_driver rhod_prox_driver = {
	.probe = rhod_prox_probe,
	.driver = {
		.name = "rhodium_proximity",
		.owner = THIS_MODULE,
	},
};

static int __init rhod_prox_init(void) {
	printk("Rhod proximity init\n");
	return platform_driver_register(&rhod_prox_driver);
}

late_initcall(rhod_prox_init);
