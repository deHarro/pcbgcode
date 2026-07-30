Please see docs/pcbgcode.pdf.

This special version, V3.7.2-alpha, is based on John T. Johnsons official version V3.6.6.
* I merged some corrections from me and others, explicetely cutting "never climbing" for all traces, "rounded corners" and "helical milling" of holes with varying diameters, all with the same cutter.
* New is the ability to generate milling code for the outline of the board including radius correction. See [here](https://harald-sattler.de/assets/images/autogen/Paradigma_VE-Modul_Umriss_resize.jpg). This works for PCBs of any shape, provided the milling bit used is thin enough (e.g., for narrow cuts). <br>Holes in the PCB require a workaround: Set milling bit diameter to zero, causing the ULP to generate a DXF file for the milling layer that can be further processed using a suitable tool (e.g., Estlcam).
* I implemented a "clear pads" function. The ULP now generates code to remove the protective coating over the pads. To do this, the milling cutter or engraving bit traverses the entire pad area in a meandering pattern. See [here](https://harald-sattler.de/html/pcb-gcode.htm#RemoveCoating).
* I fixed the error, that only the first of several holes in the file is drilled with the correct depth, whereas all following holes are only spot drilled. Now alle holes are drilled correctly with the given depth.
* When generating g-code for only one layer (TOP or BOTTOM), the files for TEXT and MILL are generated for the opposite layer as well.<br>
  Fixed. Only the selected layer files will be generated.
