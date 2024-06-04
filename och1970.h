#ifndef _MSPCO1_H_
#define _MSPCO1_H_

#include <linux/ioctl.h>

#define OCH1970_I2C_NAME                    "OCH1970"
#define OCH1970_INPUT_NAME                  "magnetic"

#define bit7        (0x01<<7)
#define bit6        (0x01<<6)
#define bit5        (0x01<<5)
#define bit4        (0x01<<4)
#define bit3        (0x01<<3)
#define bit2        (0x01<<2)
#define bit1        (0x01<<1)
#define bit0        (0x01<<0)

#define OCH_DISABLE                     0
#define OCH_ENABLE                      1

//register address of the OCH1970
#define OCH1970_REG_WIA 0x00                     //company id & device id,4bytes
#define OCH1970_REG_STATUS1 0x10                 //status data,2bytes
#define OCH1970_REG_DATAX 0x11                   //X data [15:0]
#define OCH1970_REG_DATAY 0x12                   //Y data [15:0]
#define OCH1970_REG_DATAX_Y 0x13                 //X data [15:0] & Y data [15:0]
#define OCH1970_REG_DATAZ 0x14
#define OCH1970_REG_DATAX_Z 0x15
#define OCH1970_REG_DATAY_Z 0x16
#define OCH1970_REG_DATAX_Y_Z 0x17

#define OCH1970_REG_STATUS2 0x18                  //same with STATUS1
#define OCH1970_REG_DATAXH 0x19                   //X data [15:8] only 8bits
#define OCH1970_REG_DATAYH 0x1A                   //Y data [15:8] only 8bits
#define OCH1970_REG_DATAXH_YH 0x1B                //X data [15:8] & Y data [15:8]
#define OCH1970_REG_DATAZH 0x1C
#define OCH1970_REG_DATAXH_ZH 0x1D
#define OCH1970_REG_DATAYH_ZH 0x1E
#define OCH1970_REG_DATAXH_YH_ZH 0x1F

#define OCH1970_REG_CNTL1 0X20      //CNTL1 config interrupt
#define OCH1970_REG_CNTL2 0x21      //CNTL2 config other things
#define OCH1970_REG_THRE_X1 0x22    //BOP&BRP setting
#define OCH1970_REG_THRE_X2 0x23
#define OCH1970_REG_THRE_Y1 0X24
#define OCH1970_REG_THRE_Y2 0X25
#define OCH1970_REG_THRE_Z1 0X26
#define OCH1970_REG_THRE_Z2 0X27

#define OCH1970_REG_SRST 0x30       //soft reset

//specific constant values
#define OCH1970_SLA 0x0D<<1 //8 bits address,if you use 7 bits address, the address is 0x0D;
#define OCH1970_VALUE_STANDBY_MODE 0x00
#define OCH1970_VALUE_SINGLE_MODE 0x01
#define OCH1970_VALUE_CONTINUOUS_MODE_1_0p5Hz 0x02
#define OCH1970_VALUE_CONTINUOUS_MODE_2_1Hz 0x04
#define OCH1970_VALUE_CONTINUOUS_MODE_3_2Hz 0x06
#define OCH1970_VALUE_CONTINUOUS_MODE_4_20Hz 0x08
#define OCH1970_VALUE_CONTINUOUS_MODE_5_40Hz 0x0A
#define OCH1970_VALUE_CONTINUOUS_MODE_6_100Hz 0x0C
#define OCH1970_VALUE_CONTINUOUS_MODE_7_500Hz 0x0E

#define LOG_TAG                     "[OCH1970-Magnetic] "
#define OCH_INFO(fmt, args...)      printk(KERN_INFO LOG_TAG fmt, ##args)//KERN_INFO      KERN_EMERG
#define OCH_ERR(fmt, args...)       printk(KERN_ERR      LOG_TAG fmt, ##args)//KERN_ERR   KERN_EMERG

struct och1970_data {
     struct i2c_client *client;
     struct input_dev *input;
     struct mutex irq_mutex;
     struct wake_lock time_lock;
     struct delayed_work work;
     struct work_struct irq_work;
     uint16_t bop_x1;
     uint16_t brp_x1;
     uint16_t bop_y1;
     uint16_t brp_y1;
     uint16_t bop_z1;
     uint16_t brp_z1;
     int16_t x_data;
     int16_t y_data;
     int16_t z_data;
     bool wakeup;
     int delay;
     int enable;
     int irq_gpio;
     int rst_gpio;
     int irq_number;
};


#endif /* _OCH1970_H_ */
