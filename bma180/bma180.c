#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>

#define BMA180_I2C_ADDRESS			0x40

struct bma180_meas_data {
	uint16_t accel_x;
	uint16_t accel_y;
	uint16_t accel_z;
	uint8_t temp;
};

static const unsigned short normal_i2c[] =
		{ BMA180_I2C_ADDRESS, I2C_CLIENT_END };

static uint32_t bma180_read_data(struct i2c_client *client,
		struct bma180_meas_data *data) {
	uint32_t result = i2c_smbus_read_i2c_block_data(client, 0x02, 7, (u8*) data);
	return result;
}

static ssize_t show_data(struct device *dev, struct device_attribute *attr,
		char *buf) {
	struct i2c_client *client = to_i2c_client(dev);
	uint32_t result;

	struct bma180_meas_data data;
	result = bma180_read_data(client, &data);
	if (result > 0) {
		return sprintf(buf, "%d,%d,%d\n", data.accel_x >> 2, data.accel_y >> 2,
				data.accel_z >> 2);
	}
	return result;
}

static DEVICE_ATTR(coord, S_IRUGO, show_data, NULL);

static struct attribute *bma180_attributes[] = { &dev_attr_coord.attr, NULL };

static const struct attribute_group bma180_attr_group = { .attrs =
		bma180_attributes, };

static int bma180_detect(struct i2c_client *client, struct i2c_board_info *info) {
	strlcpy(info->type, "bma180", I2C_NAME_SIZE);
	return 0;
}

static int bma180_probe(struct i2c_client *client,
		const struct i2c_device_id *id) {
	int err = 0;
	uint8_t serial[8];
	err = sysfs_create_group(&client->dev.kobj, &bma180_attr_group);
	if (err) {
		dev_err(&client->dev, "registering with sysfs failed!\n");
		goto exit;
	}
	dev_info(&client->dev, "probe succeeded!\n");
	exit: return err;
}

static int bma180_remove(struct i2c_client *client) {
	sysfs_remove_group(&client->dev.kobj, &bma180_attr_group);
	return 0;
}

static const struct i2c_device_id bma180_id[] = { { "bma180", 0 }, { } };

MODULE_DEVICE_TABLE(i2c, bma180_id);

static struct i2c_driver bma180_driver = { .driver = { .owner = THIS_MODULE,
		.name = "bma180", }, .id_table = bma180_id, .probe = bma180_probe,
		.remove = bma180_remove, .class = I2C_CLASS_HWMON, .detect =
				bma180_detect, .address_list = normal_i2c, };

static int __init bma180_init(void) {
	return i2c_add_driver(&bma180_driver);
}

static void __exit bma180_exit(void) {
	i2c_del_driver(&bma180_driver);
}

MODULE_AUTHOR("");
MODULE_DESCRIPTION("BMA180 driver");
MODULE_LICENSE("GPL");

module_init(bma180_init);
module_exit(bma180_exit);
