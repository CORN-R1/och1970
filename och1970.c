#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/input-polldev.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/sched/rt.h>
#include "och1970.h"

static struct och1970_data *t_och1970 = NULL;

static int och1970_i2c_read(uint8_t reg, uint8_t len, uint8_t *data)
{
    int err;

    err = i2c_smbus_read_i2c_block_data(t_och1970->client, reg, len, data);
    if (err < 0) {
        OCH_ERR("%s: i2c_smbus_read_i2c_block_data failed.", __func__);
        return err;
    }

    return 0;
}

static int och1970_i2c_write(uint8_t reg, uint8_t len, uint8_t *data)
{
    int err;

    err = i2c_smbus_write_i2c_block_data(t_och1970->client, reg, len, data);
    if (err < 0) {
        OCH_ERR("%s: i2c_smbus_read_i2c_block_data failed.", __func__);
        return err;
    }

    return 0;
}

static void och1970_chip_reset(void)
{
    if(gpio_is_valid(t_och1970->rst_gpio)){
        gpio_set_value(t_och1970->rst_gpio, 1);
        msleep(20);
        gpio_set_value(t_och1970->rst_gpio, 0);
        msleep(20);
        gpio_set_value(t_och1970->rst_gpio, 1);
        msleep(200);
    }
}

static int check_chip_id(struct och1970_data *och1970)
{
    uint8_t chip_id[2];

    och1970_chip_reset();

    och1970_i2c_read(OCH1970_REG_WIA, 2, chip_id);

    OCH_INFO("%s chip_id0: 0x%x chip_id1: 0x%x\n", __func__, chip_id[0], chip_id[1]);

    if ((chip_id[0] == 0x48)&&(chip_id[1] == 0xC0))
        return 0;
    else
        return -EIO;
}

void och1970_init(void)
{
    uint8_t data_temp,now_value;

    och1970_i2c_read(OCH1970_REG_CNTL2, 1, &now_value);
    data_temp = (now_value & 0xF0) | 0x00;
    och1970_i2c_write(OCH1970_REG_CNTL2, 1, &data_temp);
    data_temp |= OCH1970_VALUE_CONTINUOUS_MODE_7_500Hz;
    och1970_i2c_write(OCH1970_REG_CNTL2, 1, &data_temp);

    och1970_i2c_read(OCH1970_REG_CNTL2, 1, &now_value);
    data_temp = now_value & ~bit4;
    och1970_i2c_write(OCH1970_REG_CNTL2, 1, &data_temp);

    och1970_i2c_read(OCH1970_REG_CNTL2, 1, &now_value);
    data_temp = (now_value & ~bit5) | bit5;
    och1970_i2c_write(OCH1970_REG_CNTL2, 1, &data_temp);
}
