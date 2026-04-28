# WRO Quick Race Checklist

## 1) Before Power ON
- Battery charged and voltage checked
- Wheels free, no mechanical jam
- E-Stop button physically works
- Connectors fixed, no loose wire
- One power switch only (WRO 9.10)
- One start button only (WRO 9.11)

## 2) Power ON
- ESP32 boots without errors
- Steering centers correctly
- Motor remains stopped at startup
- Status LED behavior normal

## 3) Sensor Check
- Run scanerI2C.cpp
- TCA9548A found at 0x70
- CH0 IMU found at 0x69
- CH1 AS5600 left found at 0x36
- CH2 AS5600 right found at 0x36
- No unexpected devices on empty channels
- If any mismatch -> NO-GO until fixed

## 4) Safety Check
- Press E-Stop: motor=0, steering center
- Release E-Stop: system resumes correctly
- Camera disconnect test triggers safe stop

## 5) Track Start
- Correct profile/Kp/Kd loaded
- First run at reduced speed
- Observe 1 full lap before race pace

## 6) Final Go/No-Go
- No critical warnings in serial
- Stable steering and lap counting
- Team sign-off complete
- If any safety/rule check fails -> NO-GO

Sign: ____________________  Time: ____________________
