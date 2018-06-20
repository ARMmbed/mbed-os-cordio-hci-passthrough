Passthrough firmware that proxify a Bluetooth controller connected to an mbed board.
  
## Prerequisite 

* The board must be attached to a Bluetooth controller.
* The target must implement the CordioHCITransport interface and expose it.  
 
## Usage 
 
This application exposes the controller to external hosts via the default serial port. 
Therefore HCI packets can be send and receive with from a PC connected to the 
mbed board. Linux users can takes advantage of this to use their boards with the 
tools provided by bluez or develop the reset sequence of their HCI driver on PC.


### Log peripheral interractions

First log accesses to the peripheral with the help of btmon:

```
sudo btmon
```

btmon can also save traces in btsnoop format which is exploitable by wireshark:

```
sudo btmon -w <log file>
```

### Connect peripheral

This can be achieved with the help of the command `btattach`.

```
sudo btattach -B <peripheral serial> -S <peripheral baudrate> -P h4
```

On ubuntu, daplink serial will usually endup in `/dev/ttyACM*` and the default 
baudrate used by the firmware is `115200`.


### Interract with the peripheral 

Run the command: 

```
sudo btmgmt --index <index of the peripheral>
```

The index of the peripheral is returned to the user when `btattach` is run. 

Once `btmgmt` runs, it is possible to scan for devices or advertise. All 
operations generating trafic will be logged by `btmon`. 


 