# A Simple UART Test

Uses UART 1 to do some basic communication with a Quectel BG96 modem. It has a (very) basic (& flawed) AT command set implementation.

This uses the interrupts system (not really interupts, more like `kevents` but more responsive). We use the `multiwaiter` because we have 2 compartments:
 1. The `Producer` compartment, mainly sleeps but wakes up to send incrementing numbers to the One filling the queue with incrementing numbers.
 2. The `uart` compartment that uses a `multiwaiter` to listen to both the queue and the UART.
 Note: The use of a sealed queue because we are messaging between compartments.

Mobile communications is achieved using a Quectel BG95 modem. We have written a very simple (and not terribly fault tolerant) AT interface to drive the modem.

## Modem Interface

Communication is via the UART at 115200bps. We don't, currently, support any kind of flow control.
The AT command set used is specific to this device but easily modified. If you have one of these, or a compatible, module and wish to use it you will need to alter some settings in `modem.cc`:
1. `FORMAT_URL` needs changing to point at your own server (the address shown is a temporary IP that will be gone by the time you read this). The test code that we've written uses port 3100, but feel free to change that.
```
#define FORMAT_URL "http://18.175.136.129:3100/trk/%s/?%s"
```
2. The APM information will need changing. This can be found in `tasks_process()` in the switch statement under `case TASK_SET_APN: // Set the APN`. Change this to your own APN.

### HTTP Message Format
This is a simple HTTP POST. We pass two queries:
1. `t`: An integer. We called the `t` for `type`, to give a clue to the server what sort of information is in the query.
2. `v`: This is `v` for `value`. In this example we are merely passing the number from the `producer` compartment.

## To Build
From the folder execute:
```
xmake config --sdk=/cheriot-tools --board=sonata-1.1
xmake
xmake run
```

## nodejs server
There is a simple nodeJS server here. Place it on VM connected to the web and run `node trak5.js` and you will have a server to talk to. It will dispaly the messages received on the screen. It has been configured to show the vehicle button names associated with the buttons from example 14.