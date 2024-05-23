# Export sensor logs to the host PC in two file systems

This demonstration is an example of a safe method for exporting sensor logs stored in the Pico on-board flash to a host PC in USB Mass storage class.

Format the on-board flash memory in two separate file systems, one for internal operation and one for export.

- For internal operation: littlefs
- For export: FAT

When a USB connection is detected, the sensor log is copied from littlefs to FAT before the MSC operation starts.
Battery power is required to operate with the USB disconnected, but in this demonstration, the bootcell button is substituted as a 'USB dis-connected state' while it is pressed.

In this example it is one-way for export only, but it is also possible to import files from FAT to littlefs when the USB is disconnected.
