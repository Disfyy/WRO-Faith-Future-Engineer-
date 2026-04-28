# WRO Wiring Map

## ESP32 Pin Mapping
- GPIO21 -> I2C SDA
- GPIO22 -> I2C SCL
- GPIO27 -> Steering Servo PWM
- GPIO19 -> BTS7960 R_EN
- GPIO23 -> BTS7960 L_EN
- GPIO5  -> BTS7960 R_PWM
- GPIO14 -> BTS7960 L_PWM
- GPIO16 -> UART RX (to OpenMV TX)
- GPIO17 -> UART TX (to OpenMV RX)
- GPIO32 -> E-Stop input (pull-up)
- GPIO2  -> Status LED

## I2C / TCA9548A
- TCA address: 0x70
- CH0 -> ICM-20948 (0x69 expected)
- CH1 -> AS5600 left (0x36)
- CH2 -> AS5600 right (0x36)
- CH3 -> VL53L1X left (0x29)
- CH4 -> VL53L1X right (0x29)
- CH5..CH7 -> empty by default

## Power and Ground
- Common GND across ESP32, BTS7960, sensors, camera
- Keep logic/signal wiring short and secured
- Check polarity before every power-up
