#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>

// FILTER
#define F10HZ (uint16_t)0
#define F20HZ (uint16_t)1
#define F40HZ (uint16_t)2
#define F75HZ (uint16_t)3
#define F150HZ (uint16_t)4
#define F300HZ (uint16_t)5
#define F600HZ (uint16_t)6
#define F1200HZ (uint16_t)7
#define HIGHPASS (uint16_t)8
#define BANDPASS (uint16_t)9

// GSENSITIVITY
#define G1 (uint16_t)0
#define G15 (uint16_t)1
#define G2 (uint16_t)2
#define G3 (uint16_t)3
#define G4 (uint16_t)4
#define G8 (uint16_t)5
#define G16 (uint16_t)6

#define BMA180_I2C_ADDRESS			0x40
struct i2c_client *g_client;
struct bma180_meas_data {
	uint8_t buff[7];
};

	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	uint8_t temp;
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
	int16_t lsb, msb;

	struct bma180_meas_data data;
	result = bma180_read_data(client, &data);

	lsb = data.buff[0]>>2;
	msb = data.buff[1];
	accel_x = (msb<<6)+lsb; 
	if ((accel_x)&0x2000) (accel_x)|=0xc000; // set full 2 complement for neg values
	lsb = data.buff[2]>>2;
	msb = data.buff[3];
	accel_y = (msb<<6)+lsb;
	if ((accel_y)&0x2000) (accel_y)|=0xc000;
	lsb = data.buff[4]>>2;
	msb = data.buff[5];
	accel_z = (msb<<6)+lsb;
	if ((accel_z)&0x2000) (accel_z)|=0xc000;
	temp = data.buff[6];
	if (temp&0x80) temp|=0xff00;

	if (result > 0) {
		return sprintf(buf, "%d,%d,%d\n", accel_x, accel_y,
				accel_z);
	}
	return result;
}

uint16_t BMA180_getRegValue(uint8_t adr)
{
	char buff[1];
	uint8_t read = i2c_smbus_read_i2c_block_data(g_client, adr, 1, (u8*)buff); //Wire.requestFrom((int16_t)address, 1);
	printk(KERN_INFO "GET REG VALUE");
	if (read == 1) {
		return buff[0];
	} else {
		printk(KERN_INFO "Failed to write :<");
		return 0xFFFF;
	}
}

int BMA180_setRegValue(uint8_t regAdr, uint8_t val, uint8_t maskPreserve)
{
	uint16_t tmp = BMA180_getRegValue(regAdr);
	uint8_t preserve = (uint8_t)tmp;
	uint8_t orgval = preserve & maskPreserve;
	uint8_t ret = i2c_smbus_write_byte_data(g_client, regAdr, orgval|val);

	if (tmp == 0xFFFF) {
		printk(KERN_INFO "Aiureaaaa");
		return -1;
	}

	if (ret != 0) {
		printk(KERN_INFO "failed to write");
		//failCount++;
		return -1;
	}

	return 0;
}

int BMA180_setGSensitivty(uint8_t maxg) //1, 1.5 2 3 4 8 16
{
	return BMA180_setRegValue(0x35,maxg<<1,0xF1);
}

int BMA180_SetFilter(uint8_t f) // 10,20,40,75,150,300,600,1200, HP 1HZ,BP 0.2-300, higher values not authorized
{
	return BMA180_setRegValue(0x20,f<<4,0x0F);
}

int BMA180_SetISRMode() // you must provide a ISR function on the pin selected (pin 2 or 3,. so INT0 or INT1)
{
	return BMA180_setRegValue(0x21,2,0xFD);
}

int BMA180_SoftReset() // all values will be default
{
	bool ret = BMA180_setRegValue(0x10,0xB6,0);
	udelay(100);
	return ret;
}

int BMA180_SetSMPSkip()
{
	return BMA180_setRegValue(0x35, 1, 0xFE);
}

int BMA180_enableWrite(void)
{
	//ctrl_reg1 register set ee_w bit to enable writing to regs.
	int ret = BMA180_setRegValue(0x0D,0x10,~0x10);
	udelay(10);
	return ret;
}


int BMA180_disableWrite(void)
{
	int ret = BMA180_setRegValue(0x0D,0x0,~0x10);
	udelay(10);
	return ret;
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
	printk(KERN_INFO "HALLLLLLLOO");
	g_client = client;

	BMA180_SoftReset();
 	BMA180_enableWrite();
	BMA180_SetFilter(F10HZ);
	BMA180_setGSensitivty(G15);
	BMA180_SetSMPSkip();
	BMA180_disableWrite();

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
