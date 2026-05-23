# Schemes

Wiring diagrams and system schematics for the WRO Future Engineers vehicle.
All files in this folder describe the **v13 hardware** (ESP32-S3 + 2× AS5600
on dual native I2C + 2× VL53L1X with XSHUT-based runtime address remap, no
TCA9548A mux).

## Files

| File | Format | Notes |
|---|---|---|
| `WRO_Wiring_Map.md` | Markdown | Summary pin table; full reference at [`../docs/guides/WRO_Wiring_Map_v13.md`](../docs/guides/WRO_Wiring_Map_v13.md) |
| `WRO_Full_System_Diagram.md` | Markdown + embedded Mermaid | System block diagram with prose |
| `WRO_Full_System_Diagram.mmd` | Mermaid source | Same diagram, source only |
| `WRO_Full_System_Diagram.svg` / `.png` | Rendered | Generated from the `.mmd` above |
| `WRO_Detailed_Wiring_Diagram.mmd` | Mermaid source | Pin-by-pin connectivity view |
| `WRO_Detailed_Wiring_Diagram.svg` / `.png` | Rendered | Generated from the `.mmd` above |

## Regenerating the rendered diagrams

If you edit a `.mmd` source, re-render with one of:

- **Mermaid CLI (mmdc):**
  ```
  npx -y @mermaid-js/mermaid-cli -i WRO_Full_System_Diagram.mmd -o WRO_Full_System_Diagram.svg
  npx -y @mermaid-js/mermaid-cli -i WRO_Full_System_Diagram.mmd -o WRO_Full_System_Diagram.png -w 2400
  npx -y @mermaid-js/mermaid-cli -i WRO_Detailed_Wiring_Diagram.mmd -o WRO_Detailed_Wiring_Diagram.svg
  npx -y @mermaid-js/mermaid-cli -i WRO_Detailed_Wiring_Diagram.mmd -o WRO_Detailed_Wiring_Diagram.png -w 2400
  ```
- **Mermaid Live Editor:** paste the `.mmd` content into [mermaid.live](https://mermaid.live/) and export.

## Authoritative pin reference

The full v13 pin map with notes lives at [`../docs/guides/WRO_Wiring_Map_v13.md`](../docs/guides/WRO_Wiring_Map_v13.md). The `.md` files in this folder summarize that reference visually.
