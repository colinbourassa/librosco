**librosco** is a software library, written in C, that is capable of communicate with the diagnostic port on the the Rover Modular Engine Management System (MEMS) version 1.6, which was fitted to Rover cars (including the Mini) in the early- to mid-1990s.

The library requires that a [compatible hardware interface](HardwareInterface.md) be built to connect to the ECU's diagnostic port.

**At the moment, only MEMS 1.6 is supported.** The communication protocol changed for newer versions of the system, and my research has so far been limited to the 1.6 system in a Mini SPi.

This library has no user interface of its own (apart from a simple command line utility), but it's used by the [MEMSGauge](https://www.github.com/colinbourassa/memsgauge) graphical front-end.
