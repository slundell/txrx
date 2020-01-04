# txrx
Remote control for auxillary equipment on RC cars

The remote controller for our SCX10II Cherokee has one extra channel. We wanted to add remote controlled led ramp, head lights and a winch. 

This uses two Wemos D1 Mini in ESP-now mode to send commands directly from the sender to the receiver. 
The sender is housed in a custom remote control case. It has snap-in mounting for tactile switches and print-in place buttons. 
The Wemos on the sender has a battery shield mounted, which connects to a small lithium battery.

The receiver uses a motor shield for the winch and the led ramp. The headlights are controlled directly with the I/O pins. 
A 100Ohm resistor is used in series with the leds to limit the current. The headlights are controlled on the low side, and the high side is connected to +5V.

