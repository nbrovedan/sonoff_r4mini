Soldar os fios assim:
- TX > RXD
- RX > TXD
- GND > GND
- 3.3v > 3.3v
<img width="600" height="483" alt="image" src="https://github.com/user-attachments/assets/19ba027b-4f41-4186-b3c8-a3953297c205" />


Ajustas o bound para 76800 no monitor e 115200 no upload.
Colocar uma fonte extra de 3.3v porque o TTL não dá conta e dá erro na com.

PINOUT:
GPIO00	BUTTON
GPIO01	likely TX (not tested)
GPIO03	likely RX (not tested)
GPIO19	blue LED
GPIO26	Relay output
GPIO27	S2 (external switch input)
GND	S1 (external switch input)
