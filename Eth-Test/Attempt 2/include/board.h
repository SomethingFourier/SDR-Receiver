#ifndef __BOARD_H__
#define __BOARD_H__

/* PIO and state machine selections */
#define PICO_RMII_ETHERNET_PIO              pio0
#define PICO_RMII_ETHERNET_SM_RX            1
#define PICO_RMII_ETHERNET_SM_TX            0

/* Ethernet pins */
#define PICO_RMII_ETHERNET_TX_PIN           10
#define PICO_RMII_ETHERNET_RX_PIN           13
#define PICO_RMII_ETHERNET_MDIO_PIN         16
#define PICO_RMII_ETHERNET_MDC_PIN          17
#define PICO_RMII_ETHERNET_RST_PIN          18
#define PICO_RMII_ETHERNET_RETCLK_PIN       23

/* LED GPIO */
#define LED_GPIO                            4

/* Enable features */
#define GENERATE_RMII_CLK                   1
#define GENERATE_MDIO_CLK                   1

#endif
