Please see docs/pcbgcode.pdf.

This special version, V3.7.2-alpha, is based on John T. Johnsons official version V3.6.6.
* I merged some corrections from me and others, explicetely cutting "never climb" for all traces, "rounded corners" and "helical milling" of holes with varying diameters, all with the same cutter.
* New is the ability to generate milling code for the outline of the board including radius correction. This works for PCBs of any shape, provided the milling bit used is thin enough (e.g., for narrow cuts). Holes in the PCB require a workaround: the milling bit diameter is set to zero, causing the ULP to generate a DXF file for the milling layer that can be further processed using a suitable tool (e.g., Estlcam).
* The last addon I implemented is a "clear pads" function. The ULP now generates code to remove the protective coating over the pads. To do this, the milling cutter or engraving bit traverses the entire pad area in a meandering pattern. 
