# Wokwi Simulation

## Virtual Components Included:
- ESP32 DevKit V1
- SSD1306 OLED (128x64, I2C @ 0x3C)
- 4x Pushbuttons (MENU, SOS, UP, DOWN)
- 1x Slide Switch (POWER)
- 1x LED (SOS indicator)

## Pin Mappings (matches hardware):
- **I2C**: SDA=D21, SCL=D22
- **Buttons**: MENU=D25, SOS=D26, UP=D32, DOWN=D33
- **Switch**: POWER=D4
- **LED**: D2 (built-in on ESP32)

## How to Use:
1. Build project: `pio run`
2. Open Wokwi: https://wokwi.com/vscode
3. Start simulation in VS Code
4. Use virtual buttons to test UI logic

## Note:
LoRa, GPS, and MPU6050 are not simulated (no Wokwi support).
Test these modules with physical hardware or mocks.
