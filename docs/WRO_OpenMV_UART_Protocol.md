# OpenMV UART Protocol for ESP32

## Frame Format (3 fields)
Single line, ASCII:
errorX,distance,objectType\n
Example:
-12,145,1

## Field Ranges (expected by firmware)
- errorX: -160 .. 160
- distance: 0 .. 10000
- objectType:
	- 0: нет объекта / фон
	- 1: оранжевая линия (курс +)
	- 2: синяя линия (курс -)
	- 3: красный столбик (объезд вправо)
	- 4: зелёный столбик (объезд влево)

## Requirements
- Baud: 115200
- 8N1
- End each frame with \n (\r optional before \n)
- Exactly two commas per frame
- Avoid empty frames

## Validation Rules in ESP32
- Frame must contain exactly two commas
- All three parts must be non-empty
- Values must pass range limits above
- On timeout, camera marked offline and safeStop triggered
