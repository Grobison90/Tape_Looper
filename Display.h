#include "daisy_seed.h"
#include "daisysp.h" 
#include "util/oled_fonts.h"
#include "dev/oled_ssd130x.h"

class Display{
private:
OledDisplay<SSD130xI2c128x64Driver> oled;
OledDisplay<SSD130xI2c128x64Driver>::Config oled_config;
FontDef text_font = Font_6x8;
uint16_t i2c_address = 60;

public:

void Init(){
    oled_config.driver_config.transport_config.i2c_config.address = i2c_address;
	oled_config.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
	oled_config.driver_config.transport_config.i2c_config.mode = I2CHandle::Config::Mode::I2C_MASTER;
	oled_config.driver_config.transport_config.i2c_config.speed = I2CHandle::Config::Speed::I2C_400KHZ;

	oled.Init(oled_config);
	oled.Height
}




};