/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "drv_lcd.h"

#define ST7365_LCD_ID 0x007365

static LCDC_InitTypeDef lcdc_int_cfg =
{
    .lcd_itf = LCDC_INTF_DBI_8BIT_B,
    .freq = 12000000,
    .color_mode = LCDC_PIXEL_FORMAT_RGB565,
    .cfg = {
        .dbi = {
            .syn_mode = HAL_LCDC_SYNC_DISABLE,
            .vsyn_polarity = 0,
            .vsyn_delay_us = 0,
            .hsyn_num = 0,
        },
    },
};

static void LCD_Init(LCDC_HandleTypeDef *hlcdc)
{
    memcpy(&hlcdc->Init, &lcdc_int_cfg, sizeof(LCDC_InitTypeDef));
    HAL_LCDC_Init(hlcdc);
    HAL_LCDC_SetROIArea(hlcdc, 0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
}

static uint32_t LCD_ReadID(LCDC_HandleTypeDef *hlcdc)
{
    (void)hlcdc;
    return ST7365_LCD_ID;
}

static void LCD_DisplayOn(LCDC_HandleTypeDef *hlcdc)
{
    (void)hlcdc;
}

static void LCD_DisplayOff(LCDC_HandleTypeDef *hlcdc)
{
    (void)hlcdc;
}

static void LCD_SetRegion(LCDC_HandleTypeDef *hlcdc, uint16_t x0, uint16_t y0,
                          uint16_t x1, uint16_t y1)
{
    HAL_LCDC_SetROIArea(hlcdc, x0, y0, x1, y1);
}

static void LCD_WriteMultiplePixels(LCDC_HandleTypeDef *hlcdc, const uint8_t *pixels,
                                    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    (void)pixels;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;

    if (hlcdc->XferCpltCallback)
    {
        hlcdc->XferCpltCallback(hlcdc);
    }
}

static const LCD_DrvOpsDef st7365_drv =
{
    .Init = LCD_Init,
    .ReadID = LCD_ReadID,
    .DisplayOn = LCD_DisplayOn,
    .DisplayOff = LCD_DisplayOff,
    .SetRegion = LCD_SetRegion,
    .WriteMultiplePixels = LCD_WriteMultiplePixels,
};

LCD_DRIVER_EXPORT2(st7365, ST7365_LCD_ID, &lcdc_int_cfg, &st7365_drv, 2);
