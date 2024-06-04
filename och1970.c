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

static void och1970_get_xz_data(void)
{
    uint8_t och1970_xz_data_array[6] = {0};

    och1970_i2c_read(OCH1970_REG_DATAX_Z, 6, och1970_xz_data_array);
    t_och1970->x_data = (int16_t)(((uint16_t)och1970_xz_data_array[4]<<8) | (uint16_t)och1970_xz_data_array[5]);
    t_och1970->z_data = (int16_t)(((uint16_t)och1970_xz_data_array[2]<<8) | (uint16_t)och1970_xz_data_array[3]);

}

static void och1970_get_yz_data(void)
{
    uint8_t och1970_yz_data_array[6] = {0};

    och1970_i2c_read(OCH1970_REG_DATAY_Z, 6, och1970_yz_data_array);
    t_och1970->y_data = (int16_t)(((uint16_t)och1970_yz_data_array[4]<<8) | (uint16_t)och1970_yz_data_array[5]);
    t_och1970->z_data = (int16_t)(((uint16_t)och1970_yz_data_array[2]<<8) | (uint16_t)och1970_yz_data_array[3]);

}

static void och1970_get_xyz_data(void)
{
    uint8_t och1970_xyz_data_array[8] = {0};

    och1970_i2c_read(OCH1970_REG_DATAX_Y_Z, 8, och1970_xyz_data_array);
    t_och1970->x_data = (int16_t)(((uint16_t)och1970_xyz_data_array[6]<<8) | (uint16_t)och1970_xyz_data_array[7]);
    t_och1970->y_data = (int16_t)(((uint16_t)och1970_xyz_data_array[4]<<8) | (uint16_t)och1970_xyz_data_array[5]);
    t_och1970->z_data = (int16_t)(((uint16_t)och1970_xyz_data_array[2]<<8) | (uint16_t)och1970_xyz_data_array[3]);

}

static void och1970_work_func(struct work_struct *work)
{
    struct och1970_data *och1970 = container_of((struct delayed_work *)work, struct och1970_data, work);

    if(och1970_check_drdy()==1){      //data is ready
        och1970_get_xyz_data();
        input_report_rel(och1970->input, REL_X, och1970->x_data);
        input_report_rel(och1970->input, REL_Y, och1970->y_data);
        input_report_rel(och1970->input, REL_Z, och1970->z_data);
        input_sync(och1970->input);
        OCH_INFO("%s start    ###############\n", __func__);
        OCH_INFO("Xdata: %d \n", och1970->x_data);
        OCH_INFO("Ydata: %d \n", och1970->y_data);
        OCH_INFO("Zdata: %d \n", och1970->z_data);
        OCH_INFO("%s end     #################\n", __func__);
    }

    schedule_delayed_work(&och1970->work, msecs_to_jiffies(och1970->delay));
}

static void och1970_irq_work_fun(struct work_struct *work)
{
    mutex_lock(&t_och1970->irq_mutex);

    och1970_get_xyz_data();
    if (device_may_wakeup(&t_och1970->client->dev)) {
        pm_relax(&t_och1970->client->dev);
    }
    OCH_INFO("%s start     @@@@@@@@@@@@@@@@@@@\n", __func__);
    OCH_INFO("Xdata: %d \n", t_och1970->x_data);
    OCH_INFO("Ydata: %d \n", t_och1970->y_data);
    OCH_INFO("Zdata: %d \n", t_och1970->z_data);
    OCH_INFO("%s end     @@@@@@@@@@@@@@@@@@@@\n", __func__);
    if((t_och1970->x_data < t_och1970->brp_x1) && (t_och1970->y_data < t_och1970->brp_y1)){
        if(t_och1970->z_data < 0){
            input_report_key(t_och1970->input, KEY_F2, 1);
            input_report_key(t_och1970->input, KEY_F2, 0);
        }else {
            input_report_key(t_och1970->input, KEY_F3, 1);
            input_report_key(t_och1970->input, KEY_F3, 0);
        }
    }else if((t_och1970->x_data < t_och1970->brp_x1) && (t_och1970->y_data > t_och1970->brp_y1 * 3)){//°Î³ö ·§ÖµËæthreshold±ä»¯
        //och1970_en_swy1(true);
        input_report_key(t_och1970->input, KEY_F8, 1);
        input_report_key(t_och1970->input, KEY_F8, 0);
    }else if (t_och1970->x_data > t_och1970->bop_x1){
        //och1970_en_swy1(false);
        input_report_key(t_och1970->input, KEY_F7, 1);
        input_report_key(t_och1970->input, KEY_F7, 0);
    }

    input_sync(t_och1970->input);
    msleep(100);
    enable_irq(t_och1970->irq_number);

    mutex_unlock(&t_och1970->irq_mutex);

}

static irqreturn_t och1970_handle_fun(int irq, void *desc)
{
    disable_irq_nosync(irq);
    wake_lock_timeout(&t_och1970->time_lock,5*HZ);
    if (device_may_wakeup(&t_och1970->client->dev)) {
        pm_stay_awake(&t_och1970->client->dev);
    }

    schedule_work(&t_och1970->irq_work);

    return IRQ_HANDLED;
}

static ssize_t och1970_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", t_och1970->enable);
}

static ssize_t och1970_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    unsigned int enable = simple_strtoul(buf, NULL, 10);

    OCH_INFO("#######enable=%d\n", enable);
    if (enable == 1){
        device_init_wakeup(&t_och1970->client->dev, true);
        och1970_en_swx1(true);
    }else if(enable == 2){
        schedule_delayed_work(&t_och1970->work, msecs_to_jiffies(t_och1970->delay));
    }else if(enable == 3){
        och1970_en_swx1(true);
        schedule_delayed_work(&t_och1970->work, msecs_to_jiffies(t_och1970->delay));
    }else{
        device_init_wakeup(&t_och1970->client->dev, false);
        //och1970_en_swy1(false);
        och1970_en_swx1(false);
        cancel_delayed_work_sync(&t_och1970->work);
    }

    return count;
}

static ssize_t och1970_wakeup_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", t_och1970->wakeup ? "true" : "false");
}

static ssize_t och1970_wakeup_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    unsigned int wakeup = simple_strtoul(buf, NULL, 10);

    OCH_INFO("#######wakeup=%d\n", wakeup);
    if (wakeup == 1){
        t_och1970->wakeup = true;
    }else{
        t_och1970->wakeup = false;
    }
    device_init_wakeup(&t_och1970->client->dev, t_och1970->wakeup);

    return count;
}

static ssize_t show_chipinfo_value(struct device *dev,
        struct device_attribute *attr, char *buf){

        return sprintf(buf, "och1970");
}

static ssize_t och1970_thresholdx1_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    uint8_t threshold[4];

    och1970_i2c_read(OCH1970_REG_THRE_X1, 4, threshold);

    return sprintf(buf, "BOP:%d\nBRP:%d\n", (((uint16_t)threshold[0]<<8) | (uint16_t)threshold[1]), (((uint16_t)threshold[2]<<8) | (uint16_t)threshold[3]));
}

static ssize_t och1970_thresholdx1_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    int i, j;
    uint8_t threshold[4];
    uint8_t bop_buf[100] = {0};
    uint8_t brp_buf[100] = {0};
    unsigned int bop_data = 0;
    unsigned int brp_data = 0;

    for(i = 0; i<100; i++){
        if(buf[i] == '\n'){
             memcpy(brp_buf, buf + j + 1, i-j);
             brp_data = simple_strtoul(brp_buf, NULL, 10);
             OCH_INFO("%s brp_data:%d\n", __func__, brp_data);
             break;
        }
        if(buf[i] == ' '){
            j = i;
            memcpy(bop_buf, buf, i);
            bop_data = simple_strtoul(bop_buf, NULL, 10);
            OCH_INFO("%s bop_data:%d\n", __func__, bop_data);
        }
    }
    if((0 < bop_data) && (bop_data < 0xffff) && (0 < brp_data) && (brp_data < 0xffff)){
        threshold[0] = (uint8_t)((bop_data & 0xff00) >> 8);
        threshold[1] = (uint8_t)(bop_data & 0xff);
        threshold[2] = (uint8_t)((brp_data & 0xff00) >> 8);
        threshold[3] = (uint8_t)(brp_data & 0xff);
        och1970_set_threshold_x1(threshold);
    }

    return count;
}

static ssize_t och1970_thresholdy1_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    uint8_t threshold[4];

    och1970_i2c_read(OCH1970_REG_THRE_Y1, 4, threshold);

    return sprintf(buf, "BOP:%d\nBRP:%d\n", (((uint16_t)threshold[0]<<8) | (uint16_t)threshold[1]), (((uint16_t)threshold[2]<<8) | (uint16_t)threshold[3]));
}

static ssize_t och1970_thresholdy1_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    int i, j;
    uint8_t threshold[4];
    uint8_t bop_buf[100] = {0};
    uint8_t brp_buf[100] = {0};
    unsigned int bop_data = 0;
    unsigned int brp_data = 0;

    for(i = 0; i<100; i++){
        if(buf[i] == '\n'){
            memcpy(brp_buf, buf + j + 1, i-j);
            brp_data = simple_strtoul(brp_buf, NULL, 10);
            OCH_INFO("%s brp_data:%d\n", __func__, brp_data);
            break;
        }
        if(buf[i] == ' '){
            j = i;
            memcpy(bop_buf, buf, i);
            bop_data = simple_strtoul(bop_buf, NULL, 10);
            OCH_INFO("%s bop_data:%d\n", __func__, bop_data);
        }
    }
    if((0 < bop_data) && (bop_data < 0xffff) && (0 < brp_data) && (brp_data < 0xffff)){
        threshold[0] = (uint8_t)((bop_data & 0xff00) >> 8);
        threshold[1] = (uint8_t)(bop_data & 0xff);
        threshold[2] = (uint8_t)((brp_data & 0xff00) >> 8);
        threshold[3] = (uint8_t)(brp_data & 0xff);
        och1970_set_threshold_y1(threshold);
    }

    return count;
}

static ssize_t och1970_thresholdz1_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    uint8_t threshold[4];

    och1970_i2c_read(OCH1970_REG_THRE_Z1, 4, threshold);

    return sprintf(buf, "BOP:%d\nBRP:%d\n", (((uint16_t)threshold[0]<<8) | (uint16_t)threshold[1]), (((uint16_t)threshold[2]<<8) | (uint16_t)threshold[3]));
}

static ssize_t och1970_thresholdz1_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    int i, j;
    uint8_t threshold[4];
    uint8_t bop_buf[100] = {0};
    uint8_t brp_buf[100] = {0};
    unsigned int bop_data = 0;
    unsigned int brp_data = 0;

     for(i = 0; i<100; i++){
         if(buf[i] == '\n'){
              memcpy(brp_buf, buf + j + 1, i-j);
              brp_data = simple_strtoul(brp_buf, NULL, 10);
              OCH_INFO("%s brp_data:%d\n", __func__, brp_data);
              break;
         }
         if(buf[i] == ' '){
              j = i;
              memcpy(bop_buf, buf, i);
              bop_data = simple_strtoul(bop_buf, NULL, 10);
              OCH_INFO("%s bop_data:%d\n", __func__, bop_data);
         }
     }
     if((0 < bop_data) && (bop_data < 0xffff) && (0 < brp_data) && (brp_data < 0xffff)){
         threshold[0] = (uint8_t)((bop_data & 0xff00) >> 8);
         threshold[1] = (uint8_t)(bop_data & 0xff);
         threshold[2] = (uint8_t)((brp_data & 0xff00) >> 8);
         threshold[3] = (uint8_t)(brp_data & 0xff);
         och1970_set_threshold_z1(threshold);
     }

     return count;
}

static DEVICE_ATTR(enable,         0644, och1970_enable_show, och1970_enable_store);
static DEVICE_ATTR(wakeup,         0644, och1970_wakeup_show, och1970_wakeup_store);
static DEVICE_ATTR(chipinfo,       0644, show_chipinfo_value, NULL);
static DEVICE_ATTR(thresholdx1, 0644, och1970_thresholdx1_show, och1970_thresholdx1_store);
static DEVICE_ATTR(thresholdy1, 0644, och1970_thresholdy1_show, och1970_thresholdy1_store);
static DEVICE_ATTR(thresholdz1, 0644, och1970_thresholdz1_show, och1970_thresholdz1_store);

static struct attribute *och1970_attributes[] = {
     &dev_attr_enable.attr,
     &dev_attr_wakeup.attr,
     &dev_attr_chipinfo.attr,
     &dev_attr_thresholdx1.attr,
     &dev_attr_thresholdy1.attr,
     &dev_attr_thresholdz1.attr,
     NULL
};

static struct attribute_group och1970_attribute_grout = {
     .attrs = och1970_attributes
};


#ifdef CONFIG_OF
static int och1970_parse_dt(struct och1970_data *och1970)
{
     struct device_node *np = och1970->client->dev.of_node;
     int ret;

     och1970->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
     if(gpio_is_valid(och1970->irq_gpio)){
         ret = gpio_request(och1970->irq_gpio, "och1970-irq");
         gpio_direction_input(och1970->irq_gpio);
     }else{
         OCH_ERR("%s: irq-gpio not find !!!\n", __func__);
         return -1;
     }

     och1970->rst_gpio = of_get_named_gpio(np, "rst-gpio", 0);
     if(gpio_is_valid(och1970->rst_gpio)){
         ret = gpio_request(och1970->rst_gpio, "och1970-rst");
         gpio_direction_output(och1970->rst_gpio, 0);
     }else{
         OCH_ERR("%s: rst-gpio not find !!!\n", __func__);
         gpio_free(och1970->irq_gpio);
         return -1;
     }

     return 0;
}
#endif

static int och1970_input_init(struct och1970_data *och1970)
{
     struct input_dev *dev = NULL;
     int err = 0;

    OCH_INFO("%s start\n", __func__);
    dev = input_allocate_device();
    if (!dev) {
         OCH_ERR("%s: can't allocate device!\n", __func__);
         return -ENOMEM;
    }

    dev->name = OCH1970_INPUT_NAME;
    dev->id.bustype = BUS_I2C;
    dev->evbit[0] = BIT_MASK(EV_REL);
    dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y) | BIT_MASK(REL_Z) | BIT_MASK(REL_MISC);
    __set_bit(KEY_F2, dev->keybit);
    __set_bit(KEY_F3, dev->keybit);
    __set_bit(KEY_F7, dev->keybit);
    __set_bit(KEY_F8, dev->keybit);
    set_bit(EV_KEY, dev->evbit);

    input_set_drvdata(dev, och1970);

    err = input_register_device(dev);
    if (err < 0) {
         OCH_ERR("%s: can't register device!\n", __func__);
         input_free_device(dev);
         return err;
    }
    och1970->input = dev;

    OCH_INFO("%s successful\n", __func__);
    return 0;
}

static void och1970_input_deinit(struct och1970_data *och1970)
{
    struct input_dev *dev = och1970->input;

    OCH_INFO("%s \n", __func__);
    input_unregister_device(dev);
    //input_free_device(dev);
}

static int och1970_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err = 0;
    uint8_t threshold_x1[] = {0x11, 0x94, 0x11, 0x94};
    uint8_t threshold_y1[] = {0x02, 0x58, 0x02, 0x58};
    uint8_t threshold_z1[] = {0x05, 0x46, 0x05, 0x46};
    struct och1970_data *och1970 = NULL;

    OCH_INFO("%s start\n", __func__);

    /* check i2c function */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
         OCH_ERR("%s: i2c function not support!\n", __func__);
         err = -ENODEV;
         goto exit;
    }
    /* setup private data */
    och1970 = kzalloc(sizeof(struct och1970_data), GFP_KERNEL);
    if (!och1970) {
         OCH_ERR("%s: can't allocate memory for och1970_data!\n", __func__);
         err = -ENOMEM;
         goto exit;
    }
    t_och1970 = och1970;
    och1970->wakeup = true;
    och1970->client = client;

    i2c_set_clientdata(client, och1970);

#ifdef CONFIG_OF
    if(och1970->client->dev.of_node){
         if(och1970_parse_dt(och1970))
             goto exit;
    }
#endif

    mutex_init(&och1970->irq_mutex);

    if(check_chip_id(och1970)){
         OCH_ERR("%s: check chip id failed\n", __func__);
         goto exit1;
    }

    och1970_init();

    err = och1970_input_init(och1970);
    if (err < 0) {
         OCH_ERR("input init fail!\n");
         goto exit1;
    }

    och1970->delay = 100;
    wake_lock_init(&och1970->time_lock, WAKE_LOCK_SUSPEND, "och1970-time");
    device_init_wakeup(&och1970->client->dev, och1970->wakeup);
    INIT_DELAYED_WORK(&och1970->work, och1970_work_func);
    INIT_WORK(&och1970->irq_work, och1970_irq_work_fun);
    och1970_set_threshold_x1(threshold_x1);
    och1970_set_threshold_y1(threshold_y1);
    och1970_set_threshold_z1(threshold_z1);
    och1970_en_swx1(false);
    och1970_en_swy1(true);
    och1970_en_swz1(false);
    och1970_odinten(true);
    och1970_en_drdy(false);
    if(gpio_is_valid(och1970->irq_gpio)){
         och1970->irq_number = gpio_to_irq(och1970->irq_gpio);
         err = request_irq(och1970->irq_number, och1970_handle_fun, IRQF_TRIGGER_LOW | IRQF_ONESHOT, "och1970-irq", NULL);
    }

    err = sysfs_create_group(&och1970->input->dev.kobj, &och1970_attribute_grout);
    if(err < 0){
         OCH_ERR("%s:input create group fail!\n", __func__);
         goto exit2;
    }

    OCH_INFO("%s successful\n", __func__);
    return 0;

exit2:
    free_irq(och1970->irq_number, NULL);
    och1970_input_deinit(och1970);
    cancel_work_sync(&och1970->irq_work);
exit1:
    mutex_destroy(&och1970->irq_mutex);
    gpio_free(och1970->irq_gpio);
    gpio_free(och1970->rst_gpio);
    kfree(och1970);
exit:
    return err;
}

static int och1970_i2c_remove(struct i2c_client *client)
{
    struct och1970_data *och1970 = i2c_get_clientdata(client);

    sysfs_remove_group(&och1970->input->dev.kobj, &och1970_attribute_grout);
    free_irq(och1970->irq_number, NULL);
    cancel_work_sync(&och1970->irq_work);
    och1970_input_deinit(och1970);
    mutex_destroy(&och1970->irq_mutex);
    gpio_free(och1970->irq_gpio);
    gpio_free(och1970->rst_gpio);
    kfree(och1970);

    return 0;
}

