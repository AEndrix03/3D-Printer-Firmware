This firmware simply perform commands sends by a linked conmputer.
An example of command would be built in this form:

sensor_identifier:action:repeat:print

where:

- sensor_identifier: is an identifier of a sensor, the map is directly written on the code. (Leds, Motors & Types, Heaters, etc...)
- action: depends on sensor, for example a motor can spin but not light as a led.
- repeat: how many times repeat this command (standard 1)

If wanted, on a command failure, the program throw an exception. The exception can be handled by a led, stopping the printing and more else...

There are three command executions type:
- TCP: Host send command -> Printer will execute it -> Printer sends ok or exception -> host send next command...
- UDP: Printer require command -> Host send it -> Printer executes it -> Printer ask next command...

TCP: low execution time, map all errors with specifics
SEMI: good execution time, map error counts of a specific bundle
UPD: fast execution time, map only the error counts of entire build