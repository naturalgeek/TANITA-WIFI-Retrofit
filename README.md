# TANITA-WIFI-Retrofit
I'am aiming to build a retrofit PCB to add Wifi to the Tanita 601,603 and 613 Body Scales.

# Current status
In development

# Hot it might work
basically there is a microcontroller in the handset of the scale. This microcontroller communicates to another microcontroller on a separate PCB containting the SD Cardslot via SPI. This is not SPI-SD, the communication happens in a properitary format. This SD-Card PCB can probably be replaced by something like a ESP32 which handles the communication to the mainboard and sends data to some HTTPS endpoint instead of writing it to a memory card.
