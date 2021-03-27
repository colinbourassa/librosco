_This is a mirror of the information in [my personal webspace](https://colinbourassa.github.io/car_stuff/mems_interface/#building-a-cable)._

## Building a cable ##
To build a cable that will connect to the MEMS 1.6 ECU's diagnostic port, you will need three things:

* A 5V (TTL level) USB to serial converter. I strongly recommend the FTDI TTL-232R-5V. Note that it's important to have a 5V converter -- **a normal RS-232 port will not work, and neither will a converter that uses regular RS-232 voltage levels.** The FTDI cable (or an equivalent, such as the GearMo 5V cable) is available from different retailers. If you're in the UK/Europe, you may want to check your local Amazon site for one of these. Remember that it is important to get one that uses 0V/5V voltage levels.
  * [FTDI TTL-232R-5V-WE from Mouser (US)](http://www.mouser.com/ProductDetail/FTDI/TTL-232R-5V-WE/?qs=OMDV80DKjRpCUAS6UR9QpQ==)
  * [GearMo TTL-232R-5V equivalent from Amazon (US)](http://www.amazon.com/dp/B004LC28G2/)

* A TE Connectivity type 172201 connector, which will mate to the connector on the car's diagnostic port. The shell for this connector is available from different retailers, although stock quantities may vary.
  * [Mouser (US)](http://www.mouser.com/ProductDetail/TE-Connectivity-AMP/172201-1/?qs=mfBlxA6VMP46pCPSoAJQ0g==)
  * [Tencell (UK)](https://www.tencell.com/products/172201-1.html)

* Three pins for the connector shell. The pins are also manufactured by TE Connectivity, part number 170280 for strips of 50. I've been told that they're also available singly, part number 170293.
  * [Allied Electronics (US)](http://www.alliedelec.com/search/productdetail.aspx?SKU=70284521)
  * [RS (UK)](http://uk.rs-online.com/web/p/products/7121729/)

Once you have the components listed above, solder leads onto the pins, insert them into the connector shell, and solder the FTDI cable wires to the pin leads according to the following table. For the pin numbering on the 172201 connector, look at the face of the connector with the key (notch) on the bottom. The first pin (C549-1) will be on the top, and the numbering continues clockwise. (If you're looking at the mating connector in the car, the numbering goes counter-clockwise.)

When looking for the diagnostic connector on the vehicle, note that cars with a security/immobilizer module (such as the Lucas 5AS) will often have a second connector of the same type. On the Mini SPi, the engine ECU connector is beige, while the security module connector is green. Make sure you're connecting to the right one.

| **Pin number** | **FTDI wire color** | **Pin assignment** | **Wire color on mating connector in car** |
|:---------------|:--------------------|:-------------------|:------------------------------------------|
|C549-1|Black|Signal ground|Pink w/ black|
|C549-2|Yellow|Rx (car ECU to PC)|White w/ yellow|
|C549-3|Orange|Tx (PC to car ECU)|Black w/ green|

![https://colinbourassa.github.io/car_stuff/images/te_connectivity_172201.jpg](https://colinbourassa.github.io/car_stuff/images/te_connectivity_172201.jpg)
