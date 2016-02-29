## uCAN
uCAN is a CAN bus testbench running on STM32F407-Discovery and STM32F091-Nucleo
development boards.

uCAN is controlled via command line interface (CLI), CLI runs over either
serial interface (STM32F091-Nucleo) or USB virtual COM port (STM32F407-Discovery).

#### PREREQUISITES
 - ARM Cortex-M GCC cross toolchain for building the firmware
 - OpenOCD v.0.8.0 or higher for firmware uploading

#### BUILDING
STM32F407-Discovery:
```
$ make TARGET=F407
```
STM32F091-Nucleo:
```
$ make TARGET=F091
```

#### UPLOADING FIRMWARE
STM32F407-Discovery:
```
$ make TARGET=F407 load
```
STM32F091-Nucleo:
```
$ make TARGET=F091 load
```

#### COMMANDS

- ``addr [ADDRh]``

   Display or set (if ADDR specified) CAN address.

- ``send <ADDRh> <BYTE-0h> [BYTE-1h [BYTE-2h ... [BYTE-7h]]]``

   Send packet to the ADDR.
   Packet payload is specified via command line. Up to 8 bytes can be sent.

- ``dump <on|off>``

   Enable/disable dumping of received packets.

- ``ping <ADDRh> <NUM> [TRACE]``

   Flood ping ADDR with NUM packets.
   Nonempty third argument enables packets tracing.

- ``stat [reset]``

   Show receive/transmit statistics.
   Optional "reset" argument causes statistics reset.

- ``xstat``

   Show mailboxes status as well as TEC, LEC and REC values.
