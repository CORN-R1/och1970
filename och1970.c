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

static uint8_t och1970_check_drdy(void)
{
    uint8_t drdy_flag[2];

    och1970_i2c_read(OCH1970_REG_STATUS1, 2, drdy_flag);

    return ((drdy_flag[1] & bit0) == bit0);
}

static void och1970_set_threshold_x1(uint8_t *value)
{
    t_och1970->bop_x1 = (uint16_t)(((value[0] << 8) & 0xff00) | (value[1] & 0xff));
    t_och1970->brp_x1 = (uint16_t)(((value[2] << 8) & 0xff00) | (value[3] & 0xff));
    och1970_i2c_write(OCH1970_REG_THRE_X1, 4, value);
}

static void och1970_set_threshold_y1(uint8_t *value)
{
    t_och1970->bop_y1 = (uint16_t)(((value[0] << 8) & 0xff00) | (value[1] & 0xff));
    t_och1970->brp_y1 = (uint16_t)(((value[2] << 8) & 0xff00) | (value[3] & 0xff));
    och1970_i2c_write(OCH1970_REG_THRE_Y1, 4, value);
}

static void och1970_set_threshold_z1(uint8_t *value)
{
    t_och1970->bop_z1 = (uint16_t)(((value[0] << 8) & 0xff00) | (value[1] & 0xff));
    t_och1970->brp_z1 = (uint16_t)(((value[2] << 8) & 0xff00) | (value[3] & 0xff));
    och1970_i2c_write(OCH1970_REG_THRE_Z1, 4, value);
}

static void och1970_en_drdy(bool flage)
{
    uint8_t now_value[2];

    och1970_i2c_read(OCH1970_REG_CNTL1, 2, now_value);
    if(flage)
       now_value[1] = (now_value[1] & ~bit0) | bit0;
    else
       now_value[1] = now_value[1] & ~bit0;

    och1970_i2c_write(OCH1970_REG_CNTL1, 2, now_value);
}

static void och1970_en_swx1(bool flage)
{
    uint8_t now_value[2];

    och1970_i2c_read(OCH1970_REG_CNTL1, 2, now_value);
    if(flage)
       now_value[1] = (now_value[1] & ~bit1) | bit1;
    else
       now_value[1] = now_value[1] & ~bit1;

    och1970_i2c_write(OCH1970_REG_CNTL1, 2, now_value);
}

static void och1970_en_swy1(bool flage)
{
    uint8_t now_value[2];

    och1970_i2c_read(OCH1970_REG_CNTL1, 2, now_value);
    if(flage)
       now_value[1] = (now_value[1] & ~bit3) | bit3;
    else
       now_value[1] = now_value[1] & ~bit3;

    och1970_i2c_write(OCH1970_REG_CNTL1, 2, now_value);
}

static void och1970_en_swz1(bool flage)
{
    uint8_t now_value[2];

    och1970_i2c_read(OCH1970_REG_CNTL1, 2, now_value);
    if(flage)
       now_value[1] = (now_value[1] & ~bit5) | bit5;
    else
       now_value[1] = now_value[1] & ~bit5;

    och1970_i2c_write(OCH1970_REG_CNTL1, 2, now_value);
}

static void och1970_odinten(bool flage)
{
    uint8_t now_value[2];

    och1970_i2c_read(OCH1970_REG_CNTL1, 2, now_value);
    if(flage)
       now_value[0] = (now_value[0] & ~bit2) | bit2;
    else
       now_value[0] = now_value[0] & ~bit2;

    och1970_i2c_write(OCH1970_REG_CNTL1, 2, now_value);
}

static void och1970_get_x_data(void)
{
    uint8_t och1970_x_data_array[4] = {0};

    och1970_i2c_read(OCH1970_REG_DATAX, 4, och1970_x_data_array);
    t_och1970->x_data = (int16_t)(((uint16_t)och1970_x_data_array[2]<<8) | (uint16_t)och1970_x_data_array[3]);

}

static void och1970_get_y_data(void)
{
    uint8_t och1970_y_data_array[4] = {0};

    och1970_i2c_read(OCH1970_REG_DATAY, 4, och1970_y_data_array);
    t_och1970->y_data = (int16_t)(((uint16_t)och1970_y_data_array[2]<<8) | (uint16_t)och1970_y_data_array[3]);

}

static void och1970_get_z_data(void)
{
    uint8_t och1970_z_data_array[4] = {0};

    och1970_i2c_read(OCH1970_REG_DATAZ, 4, och1970_z_data_array);
    t_och1970->z_data = (int16_t)(((uint16_t)och1970_z_data_array[2]<<8) | (uint16_t)och1970_z_data_array[3]);
}

static void och1970_get_xy_data(void)
{
    uint8_t och1970_xy_data_array[6] = {0};

    och1970_i2c_read(OCH1970_REG_DATAX_Y, 6, och1970_xy_data_array);
    t_och1970->x_data = (int16_t)(((uint16_t)och1970_xy_data_array[4]<<8) | (uint16_t)och1970_xy_data_array[5]);
    t_och1970->y_data = (int16_t)(((uint16_t)och1970_xy_data_array[2]<<8) | (uint16_t)och1970_xy_data_array[3]);

}
