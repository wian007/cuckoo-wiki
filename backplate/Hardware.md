# Hardware 

The backplate has its own processor, a  🔴 ST Microelectronics [STM32L151VB](http://www.st.com/web/catalog/mmc/FM141/SC1169/SS1295/LN962/PF248825) 32 MHz ARM Cortex-M3 MCU, colored red in the below photo. 

This CPU has its own 128kB of flash memory and runs its own [firmware](Firmware.md).

![](Pasted%20image%2020251107085145.png)

## Spring Terminals 
It has 10 wiring connectors. These connectors are switched by FETs. They also have "wire present" sensors in them. The feature of Nest where it detects whether a wire is inserted is physically controlled by a switch in the spring terminal, rather than detecting whether 

For example, in the Star terminal, the 2 pins toward the center of the connector are wired together, and get connected to the Rh wire when the thermostat calls for power on star. 

The 2 other pins are a switch that detects if the terminal is physically pressed down, like it would be if a wire were inserted.

![](Pasted%20image%2020251107091042.png)

The available wires are Rc, Rh, W1, W2/AUX, Y1, Y2, G, O/B, Common “C”, Star (misc)
[The names of the wires traditionally match the colors of the wires. ](https://www.ifixit.com/News/30317/what-all-those-letters-mean-on-your-thermostats-wiring)
### Power Wires
 - **Rh / Rc** - "Red, heating or cooling" - these provide the incoming 28V AC source
 - **Common “C”** - provides a path back to the HVAC from Rh/Rc without that power having to route through one of the Command wires. Nest doesn't require a C wire because it can "power sip", but if a C wire is present, it will charge more quickly, and without a C wire there could be problems with erratic calling for heat if the furnace is sensitive
 
### Command Wires
 - **W1** - White / Heat
 - **W2/AUX** - White / Heat / Aux
 - **Y1** - Yellow / Air Conditioning 1
 - **Y2** - Yellow - Air Conditioning 2
 - **G** - Green - Fan
 - **O/B** - Orange (Heat Pump)
 - **Star** - Miscellaneous - for example, powering a whole-home humidifier or dehumidifier.