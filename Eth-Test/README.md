# Ethernet Hardware and Testing

## Ethernet GPIO Mapping
| Pin | Function |
|---|---|
| 10 | Ethernet TX Enable |
| 11 | Ethernet TXD0 |
| 12 | Ethernet TXD1 |
| 13 | Ethernet CSDV |
| 14 | Ethernet RXD0 |
| 15 | Ethernet RXD1 |
| 16 | LAN8720 MDIO  |
| 17 | LAN8720 MDC   |
| 18 | LAN8720 Reset |

## Referenced Repositories and Projects

* https://github.com/rscott2049/pico-rmii-ethernet_nce
* https://github.com/sandeepmistry/pico-rmii-ethernet
* https://mongoose.ws/documentation/tutorials/rp2040/pico-rmii/

## Testing Status
* An error was found with the LAN8720 strapping configuration. The LED2/nINTSEL pin is used to determine whether the LAN8720 will generate a 50MHz clock, or it will receive a 50MHz clock. For this project, the LAN8720 is wired to receive a 50MHz clock from the rp2350, however the strapping pin is incorrectly pulled. To fix this, the 2nd Ethernet LED needs to be oriented inversely, where the anode is connected through a current limitting resistor to +3.3V, and the cathode is connected to the LED2/nINTSEL pin. To get Ethernet to work (and consequently lose this LED's functionality), R31 and R32 will need to be removed, and a 10k resistor from the Eth LED 2 net to +3.3V will need to be added.
