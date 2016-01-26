#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>

MODULE_DESCRIPTION("OmniVision ov 7692 sensor driver");
MODULE_LICENSE("GPL v2");

#define REG_OV7692_MODEL_ID_MSB	0x0A
#define REG_OV7692_MODEL_ID_LSB	0x0B

struct ov7692_ctrls {
	struct v4l2_ctrl_handler handler;

	struct {
		struct v4l2_ctrl *hflip;
		struct v4l2_ctrl *vflip;
	};
	struct v4l2_ctrl *pixel_rate;
};

struct ov7692 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct i2c_client *client;
	struct v4l2_mbus_framefmt format;
	struct ov7692_ctrls ctrls;
	int streaming;
	int power;
};

static const u8 reset_registers[] = {
       	0x12, 0x80, // Reset
	0xEE, 0x00,
};

static const u8 initial_registers[] = {
	0x0e, 0x08, // Sleep mode
	0x69, 0x52,
	0x1e, 0xb3,
	0x48, 0x42,
	0xff, 0x01,
	0xb5, 0x30,
	0xff, 0x00,
	0x16, 0x03,
	0x62, 0x10,
	0x12, 0x01,
	0x17, 0x65,
	0x18, 0xa4,
	0x19, 0x0c,
	0x1a, 0xf6,
	0x37, 0x04,
	0x3e, 0x20,
	0x81, 0x3f,
	0xcc, 0x02,
	0xcd, 0x80,
	0xce, 0x01,
	0xcf, 0xe0,
	0x82, 0x01,
	0xc8, 0x02,
	0xc9, 0x80,
	0xca, 0x01,
	0xcb, 0xe0,
	0xd0, 0x28,
	0x0e, 0x00,
	0x70, 0x00,
	0x71, 0x34,
	0x74, 0x28,
	0x75, 0x98,
	0x76, 0x00,
	0x77, 0x64,
	0x78, 0x01,
	0x79, 0xc2,
	0x7a, 0x4e,
	0x7b, 0x1f,
	0x7c, 0x00,
	0x11, 0x01,
	0x20, 0x00,
	0x21, 0x57,
	0x50, 0x4d,
	0x51, 0x40,
	0x4c, 0x7d,
	0x0e, 0x00,
	0x80, 0x7f,
	0x85, 0x00,
	0x86, 0x00,
	0x87, 0x00,
	0x88, 0x00,
	0x89, 0x2a,
	0x8a, 0x22,
	0x8b, 0x20,
	0xbb, 0xab,
	0xbc, 0x84,
	0xbd, 0x27,
	0xbe, 0x0e,
	0xbf, 0xb8,
	0xc0, 0xc5,
	0xc1, 0x1e,
	0xb7, 0x05,
	0xb8, 0x09,
	0xb9, 0x00,
	0xba, 0x18,
	0x5a, 0x1f,
	0x5b, 0x9f,
	0x5c, 0x69,
	0x5d, 0x42,
	0x24, 0x78,
	0x25, 0x68,
	0x26, 0xb3,
	0xa3, 0x0b,
	0xa4, 0x15,
	0xa5, 0x29,
	0xa6, 0x4a,
	0xa7, 0x58,
	0xa8, 0x65,
	0xa9, 0x70,
	0xaa, 0x7b,
	0xab, 0x85,
	0xac, 0x8e,
	0xad, 0xa0,
	0xae, 0xb0,
	0xaf, 0xcb,
	0xb0, 0xe1,
	0xb1, 0xf1,
	0xb2, 0x14,
	0x8e, 0x92,
	0x96, 0xff,
	0x97, 0x00,
	0x14, 0x3b,
	0x0e, 0x00,
	0xEE, 0x00,
};

static const u8 start_registers[] = {
//	0x12, 0x01, // Bayer RAW
//	0x0c, 0x01,
	0x0e, 0x00, // Normal mode (no sleep)
	//0x61, 0x00,
	0xEE, 0x00,
};

static const u8 stop_registers[] = {
	0x0e, 0x08, // Sleep mode
	0xEE, 0x00, // 0xEE is a safe marker
};

static const u8 full_registers[] = {
	0xcc, 0x02,
	0xcd, 0x80,
	0xce, 0x01,
	0xcf, 0xe0,
	0xc8, 0x02,
	0xc9, 0x80,
	0xca, 0x01,
	0xcb, 0xe0,
	0x61, 0x60,
	0xEE, 0x00,
};

static int write_regs(struct i2c_client *client, const u8 *regs)
{
	int i;

	for (i = 0; regs[i] != 0xEE; i += 2)
		v4l_info(client, "SMBus write - regs:0x%02x data:0x%02x\n", regs[i], regs[i+1]);
		if (i2c_smbus_write_byte_data(client, regs[i], regs[i + 1]) < 0){
			v4l_err(client, "SMBus Write Failed!\n");
			return -1;
		}
	return 0;
}

static inline struct ov7692 *to_ov7692(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov7692, sd);
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct ov7692, ctrls.handler)->sd;
}

static int write_regs_i2c(struct i2c_client *client, const u8 *regs)
{
	int i;
	int ret;

	u8 buf[2];

	for (i = 0; regs[i] != 0xEE; i += 2) {
		buf[0] = regs[i];
		buf[1] = regs[i+1];

		ret = i2c_master_send(client, buf, 2);
		if (ret < 0)
			v4l_err(client, "i2c send failed!\n");
	}

	return 0;	
}

static int read_regs(struct i2c_client *client, const u8 *regs)
{
	int i;
	u8 data;

	for (i = 0; regs[i] != 0xEE; i += 2){
		data = i2c_smbus_read_byte_data(client, regs[i]);

		if (data < 0){
			v4l_err(client, "SMBus Read Failed!\n");
			return -1;
		}
		v4l_info(client, "SMBus read  - regs:0x%02x data 0x%02x\n", regs[i], data);
	}
	return 0;
}

static int read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
	*data = i2c_smbus_read_byte_data(client, reg);

	if (data < 0){
		v4l_err(client, "SMBus Read Failed!\n");
			return -1;
	}
	v4l_info(client, "SMBus read  - reg:0x%02x data 0x%02x\n", reg, *data);

	return 0;
}

static void __ov7692_set_power(struct ov7692 *ov7692, int on)
{
	ov7692->streaming = 0;
	ov7692->power = on;
}

static int ov7692_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov7692 *ov7692 = to_ov7692(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);


	__ov7692_set_power(ov7692, on);
	if (on) {
		v4l2_info(client, "Resetting...\n");
		write_regs_i2c(client, reset_registers);
		usleep_range(25000, 26000);
		v4l2_info(client, "Initializing... \n");
		write_regs_i2c(client, initial_registers);
	}
	else {
		v4l2_info(client, "Enabling sleep mode... \n");
		write_regs_i2c(client, stop_registers);
	}
		
	usleep_range(150000, 170000);

	return 0;
};

static int ov7692_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_YUYV8_2X8;	
	return 0;
}

static int ov7692_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_info(client, "Setting frame sizes... \n");

	fse->code = V4L2_MBUS_FMT_YUYV8_2X8;
	fse->min_width = 640;
	fse->min_height = 480;
	fse->max_width = 640;
	fse->max_height = 480;

	return 0;
}

static int ov7692_s_stream(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);


	if(on) {
		if (write_regs_i2c(client, start_registers) < 0) {
			v4l_err(client, "err starting a stream\n");
		}
	}
	else {
		if (write_regs_i2c(client, stop_registers) < 0) {
			v4l_err(client, "err closing a stream\n");
		}	
	}
		
	return 0;
}

static void ov7692_get_default_format(struct v4l2_mbus_framefmt *mf)
{
	mf->width = 640;
	mf->height = 480;
}


static int ov7692_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct ov7692 *ov7692 = to_ov7692(sd);

	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_info(client, "Getting format... \n");

	fmt->format = ov7692->format;

	return 0;
}

static int ov7692_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct ov7692 *ov7692 = to_ov7692(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	v4l2_info(client, "Setting format... \n");

	ov7692->format = fmt->format;

	return ret;
}

static const struct v4l2_subdev_core_ops ov7692_core_ops = {
	.s_power = ov7692_s_power,
	.log_status = v4l2_ctrl_subdev_log_status,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_pad_ops ov7692_pad_ops = {
	.enum_mbus_code = ov7692_enum_mbus_code,
	.enum_frame_size = ov7692_enum_frame_sizes,
	.get_fmt = ov7692_get_fmt,
	.set_fmt = ov7692_set_fmt,
};

static int ov7692_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	//struct ov7692 *ov7692 = to_ov7692(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_info(client, "Setting g_frame rate\n");
	// fi->interval = ov7692->fiv->interval;

	return 0;
}

static int ov7692_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	//struct ov7692 *ov7692 = to_ov7692(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_info(client, "Setting s_frame rate\n");
	
	return 0;	
}

static const struct v4l2_subdev_video_ops ov7692_video_ops = {
	.s_stream = ov7692_s_stream,
	.g_frame_interval = ov7692_g_frame_interval,
	.s_frame_interval = ov7692_s_frame_interval,
};

static const struct v4l2_subdev_ops ov7692_ops = {
	.core = &ov7692_core_ops,
	.pad = &ov7692_pad_ops,
	.video = &ov7692_video_ops,
};

static int ov7692_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *mf = v4l2_subdev_get_try_format(fh, 0);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_info(client, "Getting default format ... \n");
	ov7692_get_default_format(mf);

	v4l2_info(client, "Opening the device ... \n");

	return 0;
}

static int ov7692_registered(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_info(client, "Registered the device ... \n");

	return 0;
}

static int ov7692_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct ov7692 *ov7692 = to_ov7692(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (ov7692->power == 0) {
		v4l2_info(client, "Power is down, can't apply ctrls\n");
	}

	v4l2_info(client, "Setting controls... \n");

	switch (ctrl->id){
	case V4L2_CID_PIXEL_RATE:
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ov7692_ctrl_ops = {
	.s_ctrl = ov7692_s_ctrl,
};

static const char * const test_pattern_menu[] = {
	"Disabled",
	"Color bars",
	NULL
};

static int ov7692_initialize_controls(struct ov7692 *ov7692)
{
	const struct v4l2_ctrl_ops *ops = &ov7692_ctrl_ops;
	struct ov7692_ctrls *ctrls = &ov7692->ctrls;
	struct v4l2_ctrl_handler *hdl = &ctrls->handler;
	int ret;
	struct v4l2_subdev *sd = &ov7692->sd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	v4l2_info(client, "Initializing controls... \n");

	ret = v4l2_ctrl_handler_init(hdl, 16);
	if (ret < 0) {
		v4l2_info(client, "v4l2_ctrl_handler_init returns error %d\n", ret);
		return ret;
	}

	ctrls->hflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	ctrls->vflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);
	ctrls->pixel_rate = v4l2_ctrl_new_std(&ov7692->ctrls.handler, &ov7692_ctrl_ops,
			  V4L2_CID_PIXEL_RATE, 1, INT_MAX, 1, 1);
	
	v4l2_ctrl_new_std_menu_items(hdl, ops, V4L2_CID_TEST_PATTERN,
                                ARRAY_SIZE(test_pattern_menu) - 1, 0, 0,
                                test_pattern_menu);
        if (hdl->error) {
		v4l2_info(client, "v4l2_ctrl_new_std_menu_itmes returns error %d\n", ret);
                ret = hdl->error;
                v4l2_ctrl_handler_free(hdl);
                return ret;
        }

	v4l2_ctrl_cluster(2, &ctrls->hflip);

        ov7692->sd.ctrl_handler = hdl;
	v4l2_info(client, "ov7692_initialize_controls exiting\n");

        return 0;
}

static const struct v4l2_subdev_internal_ops ov7692_sd_internal_ops = {
	.registered = ov7692_registered,
	.open = ov7692_open,
};

static int ov7692_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov7692 *ov7692;
	struct i2c_adapter *adapter = client->adapter;
	struct v4l2_subdev *sd;
	struct clk *clk;
	int ret;
	u8 model_id_msb;
	u8 model_id_lsb;
	u16 model_id;	

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		v4l_info(client, "i2c_check_functionality failed\n");
		return -ENODEV;
	}

	clk = devm_clk_get(&client->dev, NULL);
	if (IS_ERR(clk))
		return -EPROBE_DEFER;

	ret = clk_set_rate(clk, 26600000);
	if (ret < 0)
	{
		v4l_err(client, "clk_set_rate failed\n");
		return ret;
	}
	
	ret = clk_prepare_enable(clk);

	if (ret < 0)
	{
		v4l_err(client, "clk_enable failed\n");
		return ret;
	}

	udelay(1);

	ov7692 = devm_kzalloc(&client->dev, sizeof(*ov7692), GFP_KERNEL);
	sd = &ov7692->sd;

	if (sd == NULL){
		v4l_info(client, "sd == NULL\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(sd, client, &ov7692_ops);
	strlcpy(sd->name, "OV7692", sizeof(sd->name));

	sd->internal_ops = &ov7692_sd_internal_ops;

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	
	client->flags = I2C_CLIENT_SCCB;

	v4l_info(client, "chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);


	/* read model ID */
	read_reg(client, REG_OV7692_MODEL_ID_MSB, &model_id_msb);
	read_reg(client, REG_OV7692_MODEL_ID_LSB, &model_id_lsb);

	model_id = (model_id_msb << 8) | ((model_id_lsb & 0x00FF)) ;

	v4l_info(client, "Model ID: 0x%x, 0x%x, 0x%x\n", model_id, model_id_msb, model_id_lsb);

	// if (write_regs_i2c(client, full_registers) < 0) {
	// 	v4l_err(client, "err initializing OV7692\n");
	// 	return -ENODEV;
	// }

	/* check initial values */
	//read_regs(client, full_registers);

	ov7692->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&sd->entity, 1, &ov7692->pad, 0); 
	if (ret < 0){
		v4l_err(client, "media entity init failed\n");
		return ret;
	}	

	ret = ov7692_initialize_controls(ov7692);
	if (ret < 0){
		v4l_err(client, "controls init failed\n");
		return ret;
	}

	ret = v4l2_async_register_subdev(sd);
	if (ret < 0){
		v4l_err(client, "Sub-device registration failed\n");
		return ret;
	}

	return 0;
}

static int ov7692_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);

	return 0;
}

static const struct i2c_device_id ov7692_id[] = {
	{ "ov7692", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov7692_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov7692_of_match[] = {
	{ .compatible = "omnivision,ov7692", },
	{	},
};
#endif

static struct i2c_driver ov7692_driver = {
	.driver = {
		.of_match_table = of_match_ptr(ov7692_of_match),
		.name  = "ov7692",
	},
	.probe = ov7692_probe,
	.remove = ov7692_remove,
	.id_table = ov7692_id,
};
module_i2c_driver(ov7692_driver);
