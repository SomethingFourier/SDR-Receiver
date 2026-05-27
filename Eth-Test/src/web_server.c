#include "web_server.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "lan8720.h"

// Fallback diagnostic "web" interface over USB serial.
// Read simple commands from stdin and print status / an HTML snippet.

static const char http_response[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!doctype html>"
    "<html><head><title>RP2350</title></head>"
    "<body><h1>Hello world from RP2350 + LAN8720</h1>"
    "<p>Status: Link diagnostic available over serial</p>"
    "</body></html>";

static const lan8720_t *g_phy = NULL;

void web_server_set_phy(const void *phy) {
    g_phy = (const lan8720_t *)phy;
}

static bool web_timer_cb(struct repeating_timer *t) {
    (void)t;

    // Non-blocking read of stdin
    int ch;
    static char linebuf[128];
    static int idx = 0;

    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (ch <= 0) continue;
        // Echo back printable characters
        if (ch >= 32 && ch < 127) {
            putchar(ch);
            fflush(stdout);
        }
        if (ch == '\n') continue;  // skip \n
        if (ch == '\r' || idx >= (int)sizeof(linebuf) - 1) {
            if (ch == '\r') {
                putchar('\n');
                fflush(stdout);
            }
            linebuf[idx] = '\0';
            if (idx > 0) {
                // handle command
                if (strncmp(linebuf, "GET ", 4) == 0) {
                    // simple GET request simulation
                    printf("%s\n", http_response);
                } else if (strcmp(linebuf, "status") == 0) {
                    if (g_phy) {
                        uint16_t id1 = lan8720_read_register(g_phy, 2);
                        uint16_t id2 = lan8720_read_register(g_phy, 3);
                        uint16_t bmcr = lan8720_read_register(g_phy, 0);
                        uint16_t bmsr = lan8720_read_register(g_phy, 1);
                        bool link = (bmsr & (1u<<2)) != 0;
                        bool autoneg = (bmcr & (1u<<12)) != 0;
                        printf("PHY addr=%u id1=0x%04x id2=0x%04x BMCR=0x%04x BMSR=0x%04x link=%d autoneg=%d\n",
                               g_phy->phy_addr, id1, id2, bmcr, bmsr, link, autoneg);
                    } else {
                        printf("PHY not set\n");
                    }
                } else if (strcmp(linebuf, "autoneg") == 0) {
                    if (g_phy) {
                        uint16_t anar = 0x01E1u; // selector + 10/100 half/full
                        lan8720_write_register(g_phy, 4, anar);
                        // BMCR: enable autoneg (bit12) and restart autoneg (bit9)
                        lan8720_write_register(g_phy, 0, (1u<<12) | (1u<<9));
                        printf("Sent autoneg advertise and restart.\n");
                    } else {
                        printf("PHY not set\n");
                    }
                } else if (strcmp(linebuf, "force100") == 0) {
                    if (g_phy) {
                        // Force 100Mbps full duplex: BMCR bits 13=100Mbps, 8=full duplex
                        lan8720_write_register(g_phy, 0, 0x2100u);
                        sleep_ms(100);
                        uint16_t bmcr = lan8720_read_register(g_phy, 0);
                        uint16_t bmsr = lan8720_read_register(g_phy, 1);
                        printf("Wrote BMCR=0x2100; BMCR=0x%04x BMSR=0x%04x link=%d\n",
                               bmcr, bmsr, (bmsr & (1u<<2)) != 0);
                    } else {
                        printf("PHY not set\n");
                    }
                } else if (strcmp(linebuf, "force10") == 0) {
                    if (g_phy) {
                        // Force 10Mbps full duplex: BMCR bit 8=full duplex, speed-select bits cleared
                        lan8720_write_register(g_phy, 0, 0x0100u);
                        sleep_ms(100);
                        uint16_t bmcr = lan8720_read_register(g_phy, 0);
                        uint16_t bmsr = lan8720_read_register(g_phy, 1);
                        printf("Wrote BMCR=0x0100; BMCR=0x%04x BMSR=0x%04x link=%d\n",
                               bmcr, bmsr, (bmsr & (1u<<2)) != 0);
                    } else {
                        printf("PHY not set\n");
                    }
                } else if (strcmp(linebuf, "reg31") == 0) {
                    if (g_phy) {
                        uint16_t reg31 = lan8720_read_register(g_phy, 31);
                        printf("Register 31 (Status): 0x%04x\n", reg31);
                        printf("  bit 2 (link): %d\n", (reg31 >> 2) & 1);
                        printf("  bit 4 (duplex): %d (0=half, 1=full)\n", (reg31 >> 4) & 1);
                        printf("  bits 5-6 (speed): %d (01=10M, 10=100M)\n", (reg31 >> 5) & 3);
                    } else {
                        printf("PHY not set\n");
                    }
                } else if (strcmp(linebuf, "loopback") == 0) {
                    if (g_phy) {
                        uint16_t bmcr = lan8720_read_register(g_phy, 0);
                        bmcr |= (1u << 14); /* set loopback bit */
                        lan8720_write_register(g_phy, 0, bmcr);
                        uint16_t bmcr2 = lan8720_read_register(g_phy, 0);
                        printf("Enabled internal loopback: BMCR=0x%04x\n", bmcr2);
                    } else {
                        printf("PHY not set\n");
                    }
                    fflush(stdout);
                } else if (strcmp(linebuf, "noloopback") == 0) {
                    if (g_phy) {
                        uint16_t bmcr = lan8720_read_register(g_phy, 0);
                        bmcr &= ~(1u << 14); /* clear loopback bit */
                        lan8720_write_register(g_phy, 0, bmcr);
                        uint16_t bmcr2 = lan8720_read_register(g_phy, 0);
                        printf("Disabled internal loopback: BMCR=0x%04x\n", bmcr2);
                    } else {
                        printf("PHY not set\n");
                    }
                    fflush(stdout);
                } else if (strcmp(linebuf, "help") == 0) {
                    printf("Commands: status, autoneg, force100, force10, reg31, loopback, noloopback, GET /, help\n");
                    fflush(stdout);
                } else {
                    printf("Unknown command. Type 'help' for list.\n");
                    fflush(stdout);
                }
            }
            idx = 0;
        } else {
            linebuf[idx++] = (char)ch;
        }
    }
    return true; // keep timer
}

void web_server_start(void) {
    // Provide simple serial-based diagnostic interface via USB serial.
    static struct repeating_timer timer;
    // Poll stdin periodically (100 ms) for commands
    add_repeating_timer_ms(100, web_timer_cb, NULL, &timer);
    printf("Ready. Type 'help' for commands.\n");
}