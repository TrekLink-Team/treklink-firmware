### ENCLOSURE & PCB LAYOUT DIMENSIONS SPECIFICATION DOCUMENT

*(Updated to CAD Exact Measurements)*

**1. ENCLOSURE DIMENSIONS**
*   **External Dimensions (L x W x H):** 124.6 mm x 79.6 mm x 33.7 mm *(Assembled height)*
*   **Internal Usable Dimensions (L x W x H):** 118.8 mm x 73.8 mm x ~28.5 mm
*   **Enclosure Base Floor to Post Top (Standoff Height):** 4.2 mm

---

**2. INTERNAL STRUCTURAL PILLARS (CORNER SCREWS)**
*   **Layout:** 4 corners
*   **Pillar Outer Diameter ($\phi$):** 9.2 mm
*   **Distance Between Centers:** 108.8 mm (Horizontal) x 63.9 mm (Vertical)

---

**3. PCB MOUNTING POSTS (ENCLOSURE HARDWARE)**
*   **Layout:** 2 Rows x 3 Columns (6 mounting posts total)
*   **Horizontal Spacing (Hole-to-Hole):** 42.5 mm between adjacent holes *(Total span from far-left to far-right is 85.0 mm)*
*   **Vertical Spacing (Hole-to-Hole):** 60.0 mm
*   **Enclosure Post Outer Diameter ($\phi$):** 6.0 mm
*   **Enclosure Hole Inner Diameter ($\phi$):** 2.4 mm *(Ideal for M2.5 or small M3 self-tapping screws)*
*   **Recommended PCB Drill Hole Diameter ($\phi$):** 2.5 mm to 3.2 mm

---

**4. FINAL REVISED PCB FOOTPRINT (UNIFORM 5MM CLEARANCE)**

This half-board design mounts onto the middle and right-side enclosure posts. It applies a strict 5.0 mm concentric boundary around the hole centers to guarantee manufacturing safety, uniform rounded corners, and clear wire routing paths.

**A. Actual PCB Dimensions**
*   **Total Width (X):** **52.5 mm** *(42.5 mm hole span + 5.0 mm left margin + 5.0 mm right margin)*
*   **Total Height (Y):** **70.0 mm** *(60.0 mm hole span + 5.0 mm top margin + 5.0 mm bottom margin)*
*   **PCB Thickness:** 1.6 mm *(Standard FR4)*
*   **Corner Radius:** **5.0 mm** *(Concentric with mounting hole centers)*

**B. Distance from PCB Edges to Hole Centers**
*   **Left Edge to Left-most Holes:** **5.0 mm**
*   **Right Edge to Right-most Holes:** **5.0 mm**
*   **Top Edge to Top-most Holes:** **5.0 mm**
*   **Bottom Edge to Bottom-most Holes:** **5.0 mm**

**C. Cross-Sectional Z-Heights**
*   **Clearance Under PCB:** **4.2 mm** *(Allows for bottom-side SMD components like ceramic capacitors or thin ICs)*
*   **Top of PCB to Enclosure Floor:** **5.8 mm** *(4.2 mm standoff + 1.6 mm PCB thickness)*

**D. Battery Area & Clearances**
*   **Remaining Free Battery Width:** **44.8 mm** *(Measured from the inner wall's corner pillars to the PCB edge).*
*   **Dual 21700 Battery Fit:** A 43.0 mm battery pack leaves exactly 1.8 mm of free wiggle room before touching the PCB edge.
*   **Top / Bottom Enclosure Wall Wire Gaps:** **1.9 mm** of free space to route wires horizontally along the top and bottom edges of the enclosure.
*   **Right Corner Pillar Wire Gap:** **~2.4 mm** of diagonal clearance between the rounded corner of the PCB and the rounded corner pillar of the enclosure.
