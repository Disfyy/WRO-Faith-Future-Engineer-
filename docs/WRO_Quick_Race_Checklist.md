# WRO Quick Race Checklist

## 1) Before Power ON
- Battery charged and voltage checked
- Wheels free, no mechanical jam
- E-Stop button physically works
- Connectors fixed, no loose wire

## 2) Power ON
- ESP32 boots without errors
- Steering centers correctly
- Motor remains stopped at startup
- Status LED behavior normal

## 3) Sensor Check
- Run scanerI2C.cpp
- CH0 IMU found
- CH1 AS5600 left found
- CH2 AS5600 right found
- No unexpected devices on empty channels

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

Sign: ____________________  Time: ____________________
