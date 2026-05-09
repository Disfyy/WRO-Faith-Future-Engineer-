# OpenMV → ESP32-S3 UART Protocol v3.0

## Frame Format
```
RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
```

## Fields

| # | Field | Range | Description |
|---|-------|-------|-------------|
| 1 | `RedX` | -160..160 | X-position of red pillar (0 if not seen) |
| 2 | `RedDist` | 0..999 | Distance to red pillar (cm), 999 = not seen |
| 3 | `GreenX` | -160..160 | X-position of green pillar (0 if not seen) |
| 4 | `GreenDist` | 0..999 | Distance to green pillar (cm), 999 = not seen |
| 5 | `ModeFlag` | 0..15 | Bit field — see below |
| 6 | `ExtraTag` | -160..999 | Extra data — see below |

## ModeFlag (bitwise)

| Bit | Value | Flag |
|-----|-------|------|
| 0 | `1` | Orange line visible (CW) |
| 1 | `2` | Blue line visible (CCW) |
| 2 | `4` | Magenta parking block visible |
| 3 | `8` | Black wall close (<40 cm) |

## ExtraTag

- If `ModeFlag & 4` (magenta block) → X-position of the block (-160..160)
- Else if `ModeFlag & 8` (wall close) → distance to wall in cm
- Otherwise → `0`

## Example
```
-25,42,30,58,1,0*7F
```
Red pillar at X=-25, 42 cm | Green pillar at X=30, 58 cm | orange line visible | XOR checksum = 7F

## XOR Checksum
```
cs = XOR of every byte in the data string (everything before '*')
Format: 2-char uppercase hex after '*'
```

## Requirements
- Baud: `115200`, `8N1`
- Every frame ends with `\n`
- The `*XX` checksum is mandatory
- Exactly 5 commas = 6 fields
- ESP32-S3 timeout: `500 ms` → camera marked offline → fallback to gyro-only navigation
