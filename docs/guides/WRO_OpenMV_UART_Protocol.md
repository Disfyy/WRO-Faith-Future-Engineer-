# OpenMV → ESP32-S3 UART Protocol v3.1

## Frame Format
```
RedX,RedDist,GreenX,GreenDist,ModeFlag,ExtraTag*XX\n
```

## Fields

| # | Field | Range | Description |
|---|-------|-------|-------------|
| 1 | `RedX` | -80..79 | X-position of red pillar (cx-80; use RedDist=999 for not-seen) |
| 2 | `RedDist` | 0..999 | Distance to red pillar (cm), 999 = not seen |
| 3 | `GreenX` | -80..79 | X-position of green pillar (cx-80; use GreenDist=999 for not-seen) |
| 4 | `GreenDist` | 0..999 | Distance to green pillar (cm), 999 = not seen |
| 5 | `ModeFlag` | 0..7 | Bit field — see below (bit 3 reserved/unused) |
| 6 | `ExtraTag` | -80..79 | X-position of magenta block (cx-80), or 0 |

Distances use the pinhole model `d = FOCAL_PIX * pillar_height / blob_h_px`,
with `pillar_height = 10 cm` per WRO 2026 FE rule 13.19.

## ModeFlag (bitwise)

| Bit | Value | Flag |
|-----|-------|------|
| 0 | `1` | Orange line visible (CW indicator, rule 13.9) |
| 1 | `2` | Blue line visible (CCW indicator, rule 13.9) |
| 2 | `4` | Magenta parking block visible (rule 13.27) |
| 3 | `8` | RESERVED — wall distance is owned by ESP32 ToF sensors in v13. Camera always emits 0. |

## ExtraTag

- If `ModeFlag & 4` (magenta block visible) → X-position of the block (-80..79)
- Otherwise → `0`

## Example
```
-25,42,30,58,1,0*0F
```
Red pillar at X=-25, 42 cm | Green pillar at X=30, 58 cm | orange line visible | XOR checksum = 0F

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
- Frame rate: ~50 Hz (camera caps with `time.sleep_ms(10)`)

## Compatibility
- Wire format is identical to v3.0; the only changes are semantics:
  bit 3 of `ModeFlag` is now reserved and always 0, and `ExtraTag` no
  longer carries a wall distance. The v13 ESP32 firmware does not consume
  bit 3 from the camera (legacy v11 did), so this is a documentation-only
  change for that field.
