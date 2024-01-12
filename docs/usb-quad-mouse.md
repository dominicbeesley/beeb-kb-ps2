# Convert a USB mouse to Amiga / Quadrature

These instructions are for a MSO-04 MS/04 mouse from [ebay](https://www.ebay.co.uk/itm/334450794872?hash=item4dded22178:g:UagAAOSw2g5i1rwA)

These instructions will probably work with any mouse that uses a [TP8833 IC](https://www.digchip.com/datasheets/parts/datasheet/485/TP8833-pdf.php)
to do the quadrature to USB encoding

## Open the mouse

The mouse can be opened by undoing the single screw (hidden beneath a QC sticker)

## Remove the usb lead

Either cut off or desolder the USB lead

## Remove the TP8833 chip

Note: You could optionally leave this chip in place and instead solder wires to the pins but this seemed neater to me



|Colour |Amiga  |AMX    |Atari  |TP8833 |Function
|-------|-------|-------|-------|-------|------------------------------
|Black  |   1   |       |       |  16   | Y2
|White  |   6   |       |       |  12   | Left
|Grey   |   2   |       |       |  13   | X1
|Purple |   7   |       |       |  17   | 5V
|Blue   |   3   |       |       |  15   | Y1
|Green  |   8   |       |       |   9   | Ground
|Yellow |   4   |       |       |  14   | X2
|Orange |   9   |       |       |  10   | Right
|Red    |   5   |       |       |  11   | Middle
