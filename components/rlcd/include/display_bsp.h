#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>


#define AlgorithmOptimization  3     //1:原始算法 2:采用移位算法 3:查表法   来优化CPU

enum ColorSelection {
    ColorBlack = 0,    
    ColorWhite = 0xff
};

class DisplayPort {
  private:
    esp_lcd_panel_io_handle_t io_handle = NULL;
    uint32_t            i2c_data_pdMS_TICKS = 0;
    uint32_t            i2c_done_pdMS_TICKS = 0;
    const char         *TAG                 = "Display";
    int                 mosi_;
    int                 scl_;
    int                 dc_;
    int                 cs_;
    int                 rst_;
    int                 width_;
    int                 height_;
    uint8_t            *DispBuffer = NULL;
    int                 DisplayLen;
    // What the panel is currently showing, so a redraw can send only the part
    // that actually changed. Null until the first full write succeeds.
    uint8_t            *ShadowBuffer = NULL;
    // Gathers a sub-rectangle into one contiguous run, because the rows of a
    // window are 75 bytes apart in DispBuffer and SPI wants a single transfer.
    uint8_t            *WindowBuffer = NULL;
    bool                LowPowerIdle = false;
#if (AlgorithmOptimization == 3)
	uint16_t (*PixelIndexLUT)[300];
	uint8_t  (*PixelBitLUT  )[300];
	void InitPortraitLUT();
	void InitLandscapeLUT();
#endif

    void Set_ResetIOLevel(uint8_t level);
    void RLCD_SendCommand(uint8_t Reg);
    void RLCD_SendData(uint8_t Data);
    void RLCD_Sendbuffera(uint8_t *Data, int len);
    void RLCD_Reset(void);

  public:
    DisplayPort(int mosi, int scl, int dc, int cs, int rst, int width, int height, spi_host_device_t spihost = SPI3_HOST);
    ~DisplayPort();
    void RLCD_Init();
    void RLCD_ColorClear(uint8_t color);
    void RLCD_Display();
    // Sends only the bytes that differ from what the panel already shows, and
    // nothing at all when the frame is unchanged. Returns the bytes written.
    int  RLCD_DisplayChanged();
    // ST7305 runs its own refresh; the low power mode drops it to about 1 Hz,
    // which suits a screen that changes once a minute.
    void RLCD_SetLowPower(bool enabled);
	#if (AlgorithmOptimization != 3)
    void RLCD_SetPortraitPixel(uint16_t x, uint16_t y, uint8_t color);      //竖屏显示
    void RLCD_SetLandscapePixel(uint16_t x, uint16_t y, uint8_t color);     //横屏显示
	#endif
	#if (AlgorithmOptimization == 3)
	void RLCD_SetPixel(uint16_t x, uint16_t y, uint8_t color);
	#endif
};
