# Schemes

Wiring diagrams and system schematics for the WRO Future Engineers vehicle.

## Files

| File | Source / Rendered | Hardware |
|---|---|---|
| `WRO_Wiring_Map.md` | source | **v13 (current)** |
| `WRO_Full_System_Diagram.md` | source (Markdown + embedded Mermaid) | **v13 (current)** |
| `WRO_Full_System_Diagram.mmd` | source (Mermaid only) | **v13 (current)** |
| `WRO_Detailed_Wiring_Diagram.mmd` | source (Mermaid only) | **v13 (current)** |
| `WRO_Full_System_Diagram.png` | rendered | ⚠️ **stale** — regenerate from `.mmd` |
| `WRO_Full_System_Diagram.svg` | rendered | ⚠️ **stale** — regenerate from `.mmd` |

## Regenerating the rendered diagrams

The `.png` / `.svg` were generated from earlier-revision source files and have not been re-rendered for v13. Use one of:

- **Mermaid CLI (mmdc):**
  ```
  npx -y @mermaid-js/mermaid-cli -i WRO_Full_System_Diagram.mmd -o WRO_Full_System_Diagram.svg
  npx -y @mermaid-js/mermaid-cli -i WRO_Full_System_Diagram.mmd -o WRO_Full_System_Diagram.png -w 2400
  ```
- **Mermaid Live Editor:** paste the `.mmd` content into [mermaid.live](https://mermaid.live/) and export.

## Authoritative pin reference

The full v13 pin map with notes lives at [`../docs/WRO_Wiring_Map_v13.md`](../docs/WRO_Wiring_Map_v13.md). The `.md` files in this folder summarize that reference visually.
