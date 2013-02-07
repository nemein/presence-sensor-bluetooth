# Office presence Bluetooth sensor
==================================

Requires Linux. Tested with Ubuntu 12.04.

## Dependencies
* [BlueZ](http://www.bluez.org/)
* [Mosquitto](http://mosquitto.org/)
* [libcurl](http://curl.haxx.se/libcurl/)
* [iniParser](http://ndevilla.free.fr/iniparser/)
* [JsonCpp](http://jsoncpp.sourceforge.net/)

Source codes of **iniParser** and **JsonCpp** are included in subdirectory `sensor_common/external`. The other dependencies can be installed with command

    $ sudo apt-get install libbluetooth-dev libmosquitto0-dev libcurl4-openssl-dev
    
## Building
Building requires **GNU compiler**. If for some reason it isn't installed already, it can be installed with command

    $ sudo apt-get install build-essential
    
To build the sensor, go to project root directory and run command

    $ make

## Running
Start the sensor with

    $ ./BluetoothSensor
    
Use **CTRL-C** to quit. Settings can be altered by modifying file `config.ini`.

## License
This software is available under the LGPL license and has been developed by [Nemein](http://nemein.com) as part of the EU-funded [SmarcoS project](http://smarcos-project.eu/).
    
    
    
