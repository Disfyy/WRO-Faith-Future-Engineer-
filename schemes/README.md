# Schemes

Wiring diagrams and system schematics for the WRO Future Engineers vehicle.

## Files

| File | Source / Rendered | Hardware |
|---|---|---|
| `WRO_Wiring_Map.md` | source | **v12 (current)** |
| `WRO_Full_System_Diagram.md` | source (Markdown + embedded Mermaid) | **v12 (current)** |
| `WRO_Full_System_Diagram.mmd` | source (Mermaid only) | **v12 (current)** |
| `WRO_Detailed_Wiring_Diagram.mmd` | source (Mermaid only) | **v12 (current)** |
| `WRO_Full_System_Diagram.png` | rendered | ⚠️ **stale v11** — regenerate from `.mmd` |
| `WRO_Full_System_Diagram.svg` | rendered | ⚠️ **stale v11** — regenerate from `.mmd` |

## Regenerating the rendered diagrams

The `.png` / `.svg` were generated from the v11 source files and have not been re-rendered since the v12 migration. Use one of:

- **Mermaid CLI (mmdc):**
  ```
  npx -y @mermaid-js/mermaid-cli -i WRO_Full_System_Diagram.mmd -o WRO_Full_System_Diagram.svg
  npx -y @mermaid-js/mermaid-cli -i WRO_Full_System_Diagram.mmd -o WRO_Full_System_Diagram.png -w 2400
  ```
- **Mermaid Live Editor:** paste the `.mmd` content into [mermaid.live](https://mermaid.live/) and export.

## Authoritative pin reference

The full v12 pin map with notes lives at [`../docs/WRO_Wiring_Map_v12.md`](../docs/WRO_Wiring_Map_v12.md). The `.md` files in this folder summarize that reference visually.
