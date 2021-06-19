#include <Adafruit_IS31FL3741.h>

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

/**************************************************************************/
/*!
    @brief Constructor for breakout version
    @param width Desired width of led display
    @param height Desired height of led display
*/
/**************************************************************************/

Adafruit_IS31FL3741::Adafruit_IS31FL3741(uint8_t width, uint8_t height)
    : Adafruit_GFX(width, height) {}

/**************************************************************************/
/*!
    @brief Initialize hardware and clear display
    @param addr The I2C address we expect to find the chip at
    @param theWire The TwoWire I2C bus device to use, defaults to &Wire
    @returns True on success, false if chip isnt found
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::begin(uint8_t addr, TwoWire *theWire) {
  if (_i2c_dev) {
    delete _i2c_dev;
  }
  _i2c_dev = new Adafruit_I2CDevice(addr, theWire);

  if (!_i2c_dev->begin()) {
    return false;
  }

  _i2c_dev->setSpeed(400000);

  Adafruit_BusIO_Register id_reg =
      Adafruit_BusIO_Register(_i2c_dev, IS3741_IDREGISTER);
  if (id_reg.read() != addr * 2) {
    return false;
  }

  if (!reset()) {
    return false;
  }

  return true;
}

/**************************************************************************/
/*!
    @brief Perform a software reset, will update all registers to POR values
    @returns False if I2C command not acknowledged
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::reset(void) {
  selectPage(4);
  Adafruit_BusIO_Register reset_reg =
      Adafruit_BusIO_Register(_i2c_dev, IS3741_FUNCREG_RESET);
  return reset_reg.write(0xAE);
}

/**************************************************************************/
/*!
    @brief Enables or disables via the shutdown register bit
    @param en Set to true to enable
    @returns False if I2C command not acknowledged
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::enable(bool en) {
  selectPage(4);
  Adafruit_BusIO_Register config_reg =
      Adafruit_BusIO_Register(_i2c_dev, IS3741_FUNCREG_CONFIG);
  Adafruit_BusIO_RegisterBits shutdown_bit =
      Adafruit_BusIO_RegisterBits(&config_reg, 1, 0);
  return shutdown_bit.write(en);
}

/**************************************************************************/
/*!
    @brief Set the global current-mirror from 0 (off) to 255 (brightest)
    @param current Scaler from 0 to 255
    @returns False if I2C command not acknowledged
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::setGlobalCurrent(uint8_t current) {
  selectPage(4);
  Adafruit_BusIO_Register gcurr_reg =
      Adafruit_BusIO_Register(_i2c_dev, IS3741_FUNCREG_GCURRENT);
  return gcurr_reg.write(current);
}

/**************************************************************************/
/*!
    @brief Get the global current-mirror from 0 (off) to 255 (brightest)
    @returns Current scaler from 0 to 255
*/
/**************************************************************************/
uint8_t Adafruit_IS31FL3741::getGlobalCurrent(void) {
  selectPage(4);
  Adafruit_BusIO_Register gcurr_reg =
      Adafruit_BusIO_Register(_i2c_dev, IS3741_FUNCREG_GCURRENT);
  return gcurr_reg.read();
}

/**************************************************************************/
/*!
    @brief Allows changing of command register by writing 0xC5 to 0xFE
    @returns True if I2C command succeeded.
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::unlock(void) {
  Adafruit_BusIO_Register lock_reg =
      Adafruit_BusIO_Register(_i2c_dev, IS3741_COMMANDREGISTERLOCK);
  return lock_reg.write(0xC5);
}

/**************************************************************************/
/*!
    @brief Switch to a given bank/page in the chip memory for future reads
    @param page The IS41 page to switch to
    @returns False if I2C command failed to be ack'd or invalid page
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::selectPage(uint8_t page) {
  if (page > 4)
    return false;

  if (_page == page) {
    // nice, we can skip re-setting the page!
    return true;
  }

  _page = page; // cache this value

  unlock();
  Adafruit_BusIO_Register cmd_reg =
      Adafruit_BusIO_Register(_i2c_dev, IS3741_COMMANDREGISTER);
  return cmd_reg.write(page);
}

/**************************************************************************/
/*!
    @brief Set the PWM scaling for a single LED
    @param lednum The individual LED to scale - from 0 to 351
    @param scale Scaler from 0 to 255
    @returns False if I2C command not acknowledged
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::setLEDscaling(uint16_t lednum, uint8_t scale) {
  uint8_t cmd[2] = {(uint8_t)lednum, scale}; // we'll fix the lednum later

  if (lednum < 180) {
    selectPage(2);
    return _i2c_dev->write(cmd, 2);
  } else if (lednum < 350) {
    selectPage(3);
    cmd[0] = lednum - 180; // fix it for higher numbers!
    return _i2c_dev->write(cmd, 2);
  }
  return false; // failed
}

/**************************************************************************/
/*!
    @brief Optimized function to set the PWM scaling for ALL the LEDs
    @param scale Scaler from 0 to 255
    @returns False if I2C command not acknowledged
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::setLEDscaling(uint8_t scale) {
  uint16_t ledaddr = 0;

  // write 30 bytes of scale at a time
  uint8_t buff[31];
  memset(buff, scale, 31);

  selectPage(2);
  while (ledaddr < 180) {
    buff[0] = (uint8_t)ledaddr;
    if (!_i2c_dev->write(buff, 31))
      return false;

    ledaddr += 30;
  }

  selectPage(3);
  while (ledaddr < 351) {
    buff[0] = (uint8_t)(ledaddr - 180);
    if (!_i2c_dev->write(buff, 31))
      return false;
    ledaddr += 30;
  }
  return true;
}

/**************************************************************************/
/*!
    @brief Sets all LEDs PWM to the same value - great for clearing the whole
    display at once!
    @param fillpwm The PWM value to set all LEDs to, defaults to 0
    @returns False if I2C command not acknowledged
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::fill(uint8_t fillpwm) {
  uint16_t ledaddr = 0;

  // write 30 bytes of scale at a time
  uint8_t buff[31];
  memset(buff, fillpwm, 31);

  selectPage(0);
  while (ledaddr < 180) {
    buff[0] = (uint8_t)ledaddr;
    if (!_i2c_dev->write(buff, 31))
      return false;

    ledaddr += 30;
  }

  selectPage(1);
  while (ledaddr < 351) {
    buff[0] = (uint8_t)(ledaddr - 180);
    if (!_i2c_dev->write(buff, 31))
      return false;
    ledaddr += 30;
  }
  return true;
}

/**************************************************************************/
/*!
    @brief Low level accesssor - sets a 8-bit PWM pixel value to the internal
    memory location. Does not handle rotation, x/y or any rearrangements!
    @param lednum The offset from 0 to 350 that corresponds to the LED
    @param pwm brightness, from 0 (off) to 255 (max on)
    @returns False if I2C command not acknowledged
*/
/**************************************************************************/
bool Adafruit_IS31FL3741::setLEDPWM(uint16_t lednum, uint8_t pwm) {
  uint8_t cmd[2] = {lednum, pwm}; // we'll fix the lednum later

  if (lednum < 180) {
    selectPage(0);
    return _i2c_dev->write(cmd, 2);
  } else if (lednum < 350) {
    selectPage(1);
    cmd[0] = lednum - 180; // fix it for higher numbers!
    return _i2c_dev->write(cmd, 2);
  }
  return false; // failed
}

/**************************************************************************/
/*!
    @brief Adafruit GFX low level accesssor - sets a 8-bit PWM pixel value
    handles rotation and pixel arrangement, unlike setLEDPWM
    @param x The x position, starting with 0 for left-most side
    @param y The y position, starting with 0 for top-most side
    @param color Despite being a 16-bit value, takes 0 (off) to 255 (max on)
*/
/**************************************************************************/
void Adafruit_IS31FL3741::drawPixel(int16_t x, int16_t y, uint16_t color) {
  // check rotation, move pixel around if necessary
  switch (getRotation()) {
  case 1:
    _swap_int16_t(x, y);
    x = 16 - x - 1;
    break;
  case 2:
    x = 16 - x - 1;
    y = 9 - y - 1;
    break;
  case 3:
    _swap_int16_t(x, y);
    y = 9 - y - 1;
    break;
  }

  if ((x < 0) || (x >= 16))
    return;
  if ((y < 0) || (y >= 9))
    return;
  if (color > 255)
    color = 255; // PWM 8bit max

  // setLEDPWM(x + y * 16, color, _frame);
  return;
}