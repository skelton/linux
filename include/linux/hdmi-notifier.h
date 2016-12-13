/*
 * hdmi-notifier.h - notify interested parties of (dis)connect and EDID
 * events
 *
 * Copyright 2016 Russell King <rmk+kernel@arm.linux.org.uk>
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef LINUX_HDMI_NOTIFIER_H
#define LINUX_HDMI_NOTIFIER_H

#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/kref.h>

enum {
	HDMI_CONNECTED,
	HDMI_DISCONNECTED,
	HDMI_NEW_EDID,
	HDMI_NEW_ELD,
};

struct device;

struct hdmi_notifier {
	struct mutex lock;
	struct list_head head;
	struct kref kref;
	struct blocking_notifier_head notifiers;
	struct device *dev;

	/* Current state */
	bool connected;
	bool has_eld;
	unsigned char eld[128];
	void *edid;
	size_t edid_size;
	size_t edid_allocated_size;
};

/**
 * hdmi_notifier_get - find or create a new hdmi_notifier for the given device.
 * @dev: device that sends the events.
 *
 * If a notifier for device @dev already exists, then increase the refcount
 * and return that notifier.
 *
 * If it doesn't exist, then allocate a new notifier struct and return a
 * pointer to that new struct.
 *
 * Return NULL if the memory could not be allocated.
 */
struct hdmi_notifier *hdmi_notifier_get(struct device *dev);

/**
 * hdmi_notifier_put - decrease refcount and delete when the refcount reaches 0.
 * @n: notifier
 */
void hdmi_notifier_put(struct hdmi_notifier *n);

/**
 * hdmi_notifier_register - register the notifier with the notifier_block.
 * @n: the HDMI notifier
 * @nb: the notifier_block
 */
int hdmi_notifier_register(struct hdmi_notifier *n, struct notifier_block *nb);

/**
 * hdmi_notifier_unregister - unregister the notifier with the notifier_block.
 * @n: the HDMI notifier
 * @nb: the notifier_block
 */
int hdmi_notifier_unregister(struct hdmi_notifier *n, struct notifier_block *nb);

/**
 * hdmi_event_connect - send a connect event.
 * @n: the HDMI notifier
 *
 * Send an HDMI_CONNECTED event to any registered parties.
 */
void hdmi_event_connect(struct hdmi_notifier *n);

/**
 * hdmi_event_disconnect - send a disconnect event.
 * @n: the HDMI notifier
 *
 * Send an HDMI_DISCONNECTED event to any registered parties.
 */
void hdmi_event_disconnect(struct hdmi_notifier *n);

/**
 * hdmi_event_new_edid - send a new EDID event.
 * @n: the HDMI notifier
 *
 * Send an HDMI_NEW_EDID event to any registered parties.
 * This function will make a copy the EDID so it can return -ENOMEM if
 * no memory could be allocated.
 */
int hdmi_event_new_edid(struct hdmi_notifier *n, const void *edid, size_t size);

/**
 * hdmi_event_new_eld - send a new ELD event.
 * @n: the HDMI notifier
 *
 * Send an HDMI_NEW_ELD event to any registered parties.
 */
void hdmi_event_new_eld(struct hdmi_notifier *n, const u8 eld[128]);

#endif
