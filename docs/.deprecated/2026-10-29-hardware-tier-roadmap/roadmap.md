# Version control:

## v1.0 prototype: status done
- product is built using perfboard development prototyping
- consumer modules is used in the build (Ra-02, Neo-6M, SSD1306, MPU6050)
- Board footprint large, but overall functional layout of the device is achieved
- Build characteristics:
    - Enclosure: Plastic project box, manual drilling, non waterproof
    - crude and basic jumper wiring on perfboard
    - UX/UI: achieved basic 3 button navigation and a central SOS button
    - "PCB" development type: either castellated board on perfboard + soldered pin headers to perfboard + through hole components.
    - PCB layout: 2-part perfboard design,motherboard-daughterboard style, connected via hand soldered ribbon wires, stacked. (very thick, almost 30mm total footprint in height, which is thick for pcb)
    - individual parts bought for prototype development, inconsistent quality. a unit has some defects in peripheral qualities (small beeper, no vibration, etc...)
- overall: purely prototype.

## v2.0 development phase: best quality prototype
- product is built using a single custom PCB, with castellated compatible module based soldering, and allow for easy replacement of modules using castellated/ through-hole components direct solder with module-ready standby pins
- consumer modules is used for critical components (RF, GPS, IMU, OLED, etc...), the PCB mainly contains basic circuit design based on the prototype version 
- Smaller PCB footprint but still allow easy replacement of consumer modules
- Important: use the BEST hardware available in the tier range.
- components and modules bought in bulk in wholesale retailers (pro-sumer grade)
- Enclosure must use 3d printing technology, waterproof not needed yet
- overall: basically the prototype version, but the pcb is now actual pcb with pcba and smd components
- Use castellated design for solderable modules/ SMD where possible (must be repairable/replacable). The ESP32 module itself either is DIP soldered on top/ castellated/ bare chip. Each method will cost differently for the PCB design.

## v3.0-4.0: Pre-production consumer grade product

- Build pivots towards maximum function of each unit sold while maintaining quality and functionality and cutting costs efficiently
- Cheaper but functional components that basically do the same, but at a fraction of the cost
- PCB design will be optimized for mass production, using castellated modules buoght on bulk for rapid pre production development, later phases include proprietary experimental proprietary modules built in
- Smaller PCB footprint, simpler but more efficient PCB design, use less components for the same functionality
- enclosure must be 3d printed now, the development phase should start implementing robust and waterproof case
- Optimized components and UX/UI pivoting towards commercial product
- Prosumer or wholesale, Pivots to B2B direct sourcing for components
- Overall: pre-production consumer grade product, ready to mass product tier

## v5.0: mass production consumer grade product
- Build pivots towards maximum profit of each unit sold while maintaining quality and functionality
- Cheapest but functional components that basically do the same, but at a tiny fraction of the cost
- PCB design will be optimized for mass production, using only proprietary modules using castellated structure
- Smallest PCB footprint, most condensed footprint but more efficient PCB design, use lesser or same components for the same functionality
- enclosure must be resin or plastic molded 3 printing or mold injection , case must be waterproof, beautiful commercial grade, and robust in design
- Optimized components and UX/UI representing commercial product
- Direct B2B factory sourcing for components
- Overall: mass-production grade product