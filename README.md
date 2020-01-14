# x-touch-xctl
Code for interfacing with a Behringer X-Touch usign Xctl over ethernet

This library allows you to communicate with a Behringer X-Touch 
over Ethernet using the Xctl protocol. It gives you full control
over the motorised faders, LEDs, 7-segment displays, wheels
and scribble pads (including RGB backlight control)
   
Note: This library doesn't contain the routines for actually sending
and receiving the UDP packets over Ethernet (it only generates and
interprets the packet contents). For an example of how to implement
this see the main.cpp file supplied alongside 

Included is a sample application to demonstrate how to use the x-touch library.
It makes the x-touch behave in a way similar to a simple 64 channel desk.
*** This is an interface demonstration only - no audio processing is done! ***

The X-Touch must have firmware version 1.15 in order to use Xctl mode
To upgrade using Linux download the firmware from
http://downloads.music-group.com/software/behringer/X-TOUCH/X-TOUCH_Firmware_V1.15.zip
Connect the X-Touch via USB to the PC and run the following command:
amidi -p hw:1,0,0 -s X-TOUCH_sysex_update_1-15_1-03.syx -i 100

To configure the X-Touch for XCtl use the following procedure:
   - Hold select of CH1 down whilst the X-Touch is turned on
   - Set the mode to Xctl
   - Set the Ifc to Network
   - Set the Slv IP to the IP address of your PC
   - Set the DHCP on (or set a static IP on the X-Touch as desired)
   - Press Ch1 select to exit config mode


