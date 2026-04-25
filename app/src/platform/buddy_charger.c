#include "buddy_charger.h"

#include <stdint.h>

#include "rtdevice.h"
#include "rtthread.h"

#define DBG_TAG "buddy_charger"
#define DBG_LVL DBG_INFO
#include "rtdbg.h"

#define BUDDY_CHARGER_I2C_BUS_NAME "i2c2"
#define BUDDY_AW32001_ADDRESS 0x49
#define BUDDY_AW32001_CHARGE_CURRENT_REG 0x02
#define BUDDY_AW32001_CHARGE_CURRENT 0x1F
#define BUDDY_AW32001_CHARGE_CURRENT_MASK 0x3F
#define BUDDY_AW32001_CHARGE_CURRENT_KEEP_MASK 0xC0

int buddy_charger_init(void)
{
    struct rt_i2c_bus_device *bus;
    struct rt_i2c_configuration configuration;
    uint8_t reg;
    rt_size_t size;
    rt_err_t err;

    bus = rt_i2c_bus_device_find(BUDDY_CHARGER_I2C_BUS_NAME);
    if (bus == RT_NULL)
    {
        LOG_W("I2C bus %s not found", BUDDY_CHARGER_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    err = rt_device_open((rt_device_t)bus, RT_DEVICE_FLAG_RDWR);
    if (err != RT_EOK)
    {
        LOG_W("open I2C bus %s failed=%d", BUDDY_CHARGER_I2C_BUS_NAME, err);
        return err;
    }

    configuration.mode = 0;
    configuration.addr = 0;
    configuration.timeout = 500;
    configuration.max_hz = 400000;
    err = rt_i2c_configure(bus, &configuration);
    if (err != RT_EOK)
    {
        LOG_W("configure I2C bus %s failed=%d", BUDDY_CHARGER_I2C_BUS_NAME, err);
        return err;
    }

    size = rt_i2c_mem_read(bus, BUDDY_AW32001_ADDRESS, BUDDY_AW32001_CHARGE_CURRENT_REG, 8, &reg, 1);
    if (size != 1)
    {
        LOG_W("read AW32001 charge current failed");
        return -RT_ERROR;
    }

    reg = (reg & BUDDY_AW32001_CHARGE_CURRENT_KEEP_MASK) |
          (BUDDY_AW32001_CHARGE_CURRENT & BUDDY_AW32001_CHARGE_CURRENT_MASK);

    size = rt_i2c_mem_write(bus, BUDDY_AW32001_ADDRESS, BUDDY_AW32001_CHARGE_CURRENT_REG, 8, &reg, 1);
    if (size != 1)
    {
        LOG_W("write AW32001 charge current failed");
        return -RT_ERROR;
    }

    LOG_I("AW32001 charge current set to 0x%02X", reg);
    return RT_EOK;
}
