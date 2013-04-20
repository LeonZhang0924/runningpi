#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/types.h>
#include <asm/uaccess.h>

#define GPIO_SERVO_PWM_PIN 22
#define GPIO_MOTOR_PWM_PIN 17
#define GPIO_MOTOR_EN_PIN 23

static int servo_fsm_state = 0;
static int motor_fsm_state = 0;

struct pwm_device {
	struct kobject kobj;
	struct hrtimer *servo_timer;
	struct hrtimer *motor_timer;

	int servo_pwm_duty;
	int motor_pwm_duty;

	int enabled;
};

static ssize_t pwm_sysfs_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	int var = 0;
	struct pwm_device *dev = container_of(kobj, struct pwm_device, kobj);

	if (strcmp(attr->name, "servo_pwm_duty") == 0) {
		var = dev->servo_pwm_duty;
	} else if (strcmp(attr->name, "enabled") == 0) {
		var = dev->enabled;
	} else if (strcmp(attr->name, "motor_pwm_duty") == 0) {
		var = dev->motor_pwm_duty;
	}

	return sprintf(buffer, "%d\n", var);
}

static ssize_t pwm_sysfs_store(struct kobject *kobj, struct attribute *attr,
		const char *buffer, size_t size)
{
	int var = 0;
	struct pwm_device *dev = container_of(kobj, struct pwm_device, kobj);

	sscanf(buffer, "%du", &var);

	if (strcmp(attr->name, "servo_pwm_duty") == 0) {
		if (var >= 200000 && var <= 2500000)
			dev->servo_pwm_duty = var;
		else
			dev->servo_pwm_duty = 0;
	} else if (strcmp(attr->name, "enabled") == 0) {
		if (var == 1) {
			dev->enabled = 1;
			gpio_set_value(GPIO_MOTOR_EN_PIN, 1);
			servo_fsm_state = 0;
			motor_fsm_state = 0;

			hrtimer_start(dev->servo_timer, ktime_set(0, 0), HRTIMER_MODE_REL);
			hrtimer_start(dev->motor_timer, ktime_set(0, 0), HRTIMER_MODE_REL);
		} else {
			dev->enabled = 0;
			gpio_set_value(GPIO_MOTOR_EN_PIN, 0);

			hrtimer_cancel(dev->servo_timer);
			hrtimer_cancel(dev->motor_timer);
			gpio_set_value(GPIO_MOTOR_PWM_PIN, 0);
		}
	} else if (strcmp(attr->name, "motor_pwm_duty") == 0) {
		if (var >= 0 && var <= 500000)
			dev->motor_pwm_duty = var;
		else
			dev->motor_pwm_duty = 0;
	}

	return size;
}

void pwm_kobj_release(struct kobject *kobj)
{
	struct pwm_device *dev = container_of(kobj, struct pwm_device, kobj);

	printk(KERN_INFO "%s", __FUNCTION__);

	hrtimer_cancel(dev->motor_timer);
	hrtimer_cancel(dev->servo_timer);
	kfree(dev->motor_timer);
	kfree(dev->servo_timer);

	gpio_set_value(GPIO_SERVO_PWM_PIN, 0);
	gpio_set_value(GPIO_MOTOR_PWM_PIN, 0);
	gpio_set_value(GPIO_MOTOR_EN_PIN, 0);

	gpio_free(GPIO_SERVO_PWM_PIN);
	gpio_free(GPIO_MOTOR_PWM_PIN);
	gpio_free(GPIO_MOTOR_EN_PIN);

	kfree(dev);
}

struct attribute motor_pwm_duty_attr = {
		.name = "motor_pwm_duty",
		.mode = 0666,
};

struct attribute servo_pwm_duty_attr = {
		.name = "servo_pwm_duty",
		.mode = 0666,
};

struct attribute enabled_attr = {
		.name = "enabled",
		.mode = 0666,
};

struct attribute *attributes[] = {
		&motor_pwm_duty_attr,
		&servo_pwm_duty_attr,
		&enabled_attr,
		NULL,
};

static struct sysfs_ops sysfs_ops = {
		.show = pwm_sysfs_show,
		.store = pwm_sysfs_store,
};

static struct kobj_type kobj_type = {
		.release = pwm_kobj_release,
		.sysfs_ops = &sysfs_ops,
		.default_attrs = attributes,
};

static struct pwm_device *pwm_dev;

static enum hrtimer_restart servo_pwm_cb(struct hrtimer *timer)
{
	ktime_t now;
	ktime_t latency;
	ktime_t delay;
	ktime_t diff;
	static ktime_t frame_start;

	now = ktime_get();

	if (servo_fsm_state == 0) {
		gpio_set_value(GPIO_SERVO_PWM_PIN, 1);
		now = ktime_get();
		frame_start = now;
		servo_fsm_state = 1;

		delay = ktime_set(0, pwm_dev->servo_pwm_duty);
		printk(KERN_INFO "%s: start servo_now: %lld\n", __FUNCTION__, now.tv64);
	} else {
		gpio_set_value(GPIO_SERVO_PWM_PIN, 0);
		now = ktime_get();
		diff = ktime_sub(now, frame_start); // ~ 1 500 000
		delay = ktime_sub(ktime_set(0, 20000000), diff);
		printk(KERN_INFO "%s: middle servo_now: %lld\n", __FUNCTION__, now.tv64);

		servo_fsm_state = 0;
	}

	hrtimer_start(timer, delay, HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart motor_pwm_cb(struct hrtimer *timer)
{
	ktime_t now;
	//ktime_t latency;
	ktime_t delay;
	ktime_t diff;
	static ktime_t frame_start;

	//now = ktime_get();
	//latency = ktime_sub(now, starttime);
	//printk(KERN_INFO "%s: latency: %lld\n", __FUNCTION__, latency.tv64);

	if (motor_fsm_state == 0) {
		gpio_set_value(GPIO_MOTOR_PWM_PIN, 1);
		now = ktime_get();
		frame_start = now;
		motor_fsm_state = 1;

		delay = ktime_set(0, pwm_dev->motor_pwm_duty);
	} else {
		gpio_set_value(GPIO_MOTOR_PWM_PIN, 0);
		now = ktime_get();
		diff = ktime_sub(now, frame_start);
		delay = ktime_sub(ktime_set(0, 500000), diff);
		motor_fsm_state = 0;
	}

	hrtimer_start(timer, delay, HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static int __init hrtimerdemo_init(void)
{
	int ret;
	//ktime_t now;
	//struct timespec tp;

	pwm_dev = kzalloc(sizeof(*pwm_dev), GFP_KERNEL);
	if (!pwm_dev) {
		return -ENOMEM;
	}
	pwm_dev->servo_pwm_duty = 1000000;
	pwm_dev->motor_pwm_duty = 250000;

	kobject_init(&pwm_dev->kobj, &kobj_type);
	ret = kobject_add(&pwm_dev->kobj, NULL, "pwmdev");
	if (ret) {
		kfree(pwm_dev);
		return -EFAULT;
	}

	pwm_dev->servo_timer = kzalloc(sizeof(struct hrtimer), GFP_KERNEL);
	if (IS_ERR(pwm_dev->servo_timer)) {
		ret = PTR_ERR(pwm_dev->servo_timer);
		printk(KERN_CRIT "kzalloc_timer failed.\n");
		goto kzalloc_timer;
	}

	hrtimer_init(pwm_dev->servo_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pwm_dev->servo_timer->function = servo_pwm_cb;

	pwm_dev->motor_timer = kzalloc(sizeof(struct hrtimer), GFP_KERNEL);
	if (IS_ERR(pwm_dev->motor_timer)) {
		ret = PTR_ERR(pwm_dev->motor_timer);
		printk(KERN_CRIT "kzalloc_timer failed.\n");
		goto kzalloc_timer;
	}

	hrtimer_init(pwm_dev->motor_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pwm_dev->motor_timer->function = motor_pwm_cb;

	/*
	now = ktime_get();
	starttime = now;
	printk("%s: now: %lld\n", __FUNCTION__, now.tv64);

	hrtimer_get_res(CLOCK_MONOTONIC, &tp);
	printk(KERN_INFO "hrtimer res: tv_nsec: %ld\n", tp.tv_nsec);
	printk(KERN_INFO "hrtimer res: tv_sec: %ld\n", tp.tv_sec);
*/

	ret = gpio_request(GPIO_SERVO_PWM_PIN, "GPIO_SERVO_PWM_PIN");
	if (ret) {
		printk(KERN_CRIT "gpio_request failed: %d.\n", ret);
	}

	ret = gpio_direction_output(GPIO_SERVO_PWM_PIN, 0);
	if (ret) {
		printk(KERN_CRIT "gpio_direction_input failed: %d\n", ret);
	}

	ret = gpio_request(GPIO_MOTOR_PWM_PIN, "GPIO_MOTOR_PWM_PIN");
	if (ret) {
		printk(KERN_CRIT "gpio_request failed: %d.\n", ret);
	}

	ret = gpio_direction_output(GPIO_MOTOR_PWM_PIN, 0);
	if (ret) {
		printk(KERN_CRIT "gpio_direction_input failed: %d\n", ret);
	}

	ret = gpio_request(GPIO_MOTOR_EN_PIN, "GPIO_MOTOR_EN_PIN");
	if (ret) {
		printk(KERN_CRIT "gpio_request failed: %d.\n", ret);
	}

	ret = gpio_direction_output(GPIO_MOTOR_EN_PIN, 0);
	if (ret) {
		printk(KERN_CRIT "gpio_direction_input failed: %d\n", ret);
	}

	printk(KERN_INFO "%s inited.\n", __FUNCTION__);

	return 0;

kzalloc_timer:
	return ret;
}

static void __exit hrtimerdemo_exit(void)
{
	kobject_put(&pwm_dev->kobj);
}

module_init(hrtimerdemo_init);
module_exit(hrtimerdemo_exit);

MODULE_AUTHOR("Nicolae Rosia <nicolae.rosia@gmail.com>");
MODULE_LICENSE("GPL");
