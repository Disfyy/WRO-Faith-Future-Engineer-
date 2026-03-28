#!/bin/bash
cd /Users/nr_ulan/Desktop/WRO_Project_Pack

# Create required WRO folders
mkdir -p src/esp32 src/openmv schemes models v-photos t-photos video docs

# Move source code
mv eps323.cpp src/esp32/
mv scanerI2C.cpp src/esp32/
mv openmv_main.py src/openmv/

# Move schemes
mv WRO_Wiring_Map.md schemes/

# Move documentation
mv *.md docs/ 2>/dev/null
mv *.csv docs/ 2>/dev/null
mv *.txt docs/ 2>/dev/null
mv *.h docs/ 2>/dev/null

echo "Repository structure reorganized successfully!"
