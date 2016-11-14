#ifndef DW_HDMI_CEC_H
#define DW_HDMI_CEC_H

struct dw_hdmi;
struct dw_hdmi_cec_ops {
	void (*enable)(void *);
	void (*disable)(void *);
	void (*write)(struct dw_hdmi *hdmi, u8 val, int offset);
	u8 (*read)(struct dw_hdmi *hdmi, int offset);
};

struct dw_hdmi_cec_data {
	struct dw_hdmi* hdmi;
	int irq;
	const struct dw_hdmi_cec_ops *ops;
};

#endif
