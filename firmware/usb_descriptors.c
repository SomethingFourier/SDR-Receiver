/*
 * usb_descriptors.c — Lab 6 UAC1 + CDC composite USB device descriptors.
 *
 * VID:PID  cafe:4011
 * USB 1.1 Full Speed
 *
 * Interface layout:
 * ┌───┬───────────────────────────────────────────┐
 * │ 0 │ CDC Control  (class 0x02, ACM, IAD)       │
 * │ 1 │ CDC Data     (class 0x0A)                  │
 * │ 2 │ Audio Control (UAC1, IAD, no EPs)          │
 * │ 3 │ Audio Streaming alt 0 (zero-bandwidth)     │
 * │ 3 │ Audio Streaming alt 1 (active, EP0x81)     │
 * └───┴───────────────────────────────────────────┘
 *
 * Endpoint assignments:
 *   0x81  Audio isochronous IN  (EP1 IN — UAC1 capture)
 *   0x83  CDC notification IN   (EP3 IN — interrupt)
 *   0x04  CDC data OUT          (EP4 OUT — bulk)
 *   0x84  CDC data IN           (EP4 IN  — bulk)
 *
 * Signal chain:
 *   Input Terminal (ID=1, type=0x0603 Line-In)
 *   → Feature Unit (ID=2, mute control)
 *   → Output Terminal (ID=3, type=0x0101 USB Streaming)
 */

#include "tusb.h"

/* ---- String descriptor indices ---------------------------------------- */
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC_CTRL,
    STRID_AUDIO,
};

/* ---- Device descriptor ------------------------------------------------- */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0110,   /* USB 1.1 */

    /* IAD device class — required when the config has multiple function IADs */
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xcafe,
    .idProduct          = 0x4011,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

/* ---- Configuration descriptor ----------------------------------------- */

/* CDC endpoint addresses (shifted to avoid EP1 collision with audio) */
#define EPNUM_CDC_NOTIF   0x83   /* EP3 IN  — interrupt notification */
#define EPNUM_CDC_OUT     0x04   /* EP4 OUT — bulk data from host    */
#define EPNUM_CDC_IN      0x84   /* EP4 IN  — bulk data to host      */

/* Audio isochronous IN endpoint (EP1 IN) */
#define EPNUM_AUDIO_IN    0x81

/* Audio descriptor sizes — computed by TinyUSB master macros */
#define AUDIO_IAD_LEN        8u
#define AUDIO_STD_AC_LEN     9u
#define AUDIO_CS_AC_LEN      9u
#define AUDIO_IT_LEN         12u
#define AUDIO_FU_LEN         13u
#define AUDIO_OT_LEN         9u
#define AUDIO_AS_ALT0_LEN    9u
#define AUDIO_AS_ALT1_LEN    9u
#define AUDIO_CS_AS_LEN      7u
#define AUDIO_FORMAT_LEN     11u
#define AUDIO_ISO_EP_LEN     9u
#define AUDIO_CS_ISO_EP_LEN  7u

#define AUDIO_CS_ENTITY_LEN  (AUDIO_IT_LEN + AUDIO_FU_LEN + AUDIO_OT_LEN) /* 34 */

#define AUDIO_BLOCK_LEN  (AUDIO_IAD_LEN + AUDIO_STD_AC_LEN + AUDIO_CS_AC_LEN \
                        + AUDIO_IT_LEN + AUDIO_FU_LEN + AUDIO_OT_LEN         \
                        + AUDIO_AS_ALT0_LEN + AUDIO_AS_ALT1_LEN               \
                        + AUDIO_CS_AS_LEN + AUDIO_FORMAT_LEN                  \
                        + AUDIO_ISO_EP_LEN + AUDIO_CS_ISO_EP_LEN)             /* 112 */

/* CONFIG_TOTAL_LEN = config header(9) + CDC(66) + audio block(112) = 187 */
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + AUDIO_BLOCK_LEN)

static uint8_t const desc_configuration[] = {
    /* --- Config header: 4 interfaces, bus-powered 100 mA ----------------- */
    TUD_CONFIG_DESCRIPTOR(1, 4, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    /* --- CDC function (interfaces 0 + 1) --------------------------------- */
    /* TUD_CDC_DESCRIPTOR produces the CDC IAD + control + data interfaces. */
    TUD_CDC_DESCRIPTOR(0, STRID_CDC_CTRL,
                       EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    /* --- Audio function (interfaces 2 + 3) ------------------------------- */

    /* 3a: Manual Audio IAD */
    0x08, TUSB_DESC_INTERFACE_ASSOCIATION, 2, 2, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, 0,

    /* 3b: Audio Control standard + class-specific interface headers */
    9, TUSB_DESC_INTERFACE, 2, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, STRID_AUDIO,
    9, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_HEADER, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(43), 1, 3,

    /* 3c: Input Terminal — ID=1, Line-In (0x0603), assoc OT ID=3, 2ch */
    12, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL, 1, U16_TO_U8S_LE(0x0603), 3, 2, U16_TO_U8S_LE(0x0003), 0, 0,

    /* 3d: Feature Unit — ID=2, source IT ID=1, mute on master+ch1+ch2 */
    13, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, 2, 1, 2, U16_TO_U8S_LE(0x0001), U16_TO_U8S_LE(0x0001), U16_TO_U8S_LE(0x0001), 0,

    /* 3e: Output Terminal — ID=3, USB Streaming (0x0101), assoc IT ID=1 */
    9, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL, 3, U16_TO_U8S_LE(0x0101), 1, 2, 0,

    /* 3f: Audio Streaming alt 0 (zero-bandwidth, no endpoints) */
    9, TUSB_DESC_INTERFACE, 3, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, 0,

    /* 3g: Audio Streaming alt 1 (active, one IN endpoint) */
    9, TUSB_DESC_INTERFACE, 3, 1, 1, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, 0,

    /* CS AS general: bTerminalLink=3 (OT ID), bDelay=1, wFormatTag=PCM */
    7, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_AS_GENERAL, 3, 1, U16_TO_U8S_LE(AUDIO_DATA_FORMAT_TYPE_I_PCM),

    /* Type I format: 2ch, 4-byte subframe, 24-bit, 48 kHz */
    11, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_FORMAT_TYPE, AUDIO_FORMAT_TYPE_I, 2, 4, 24, 1, 0x80, 0xBB, 0x00,

    /* Standard isochronous IN endpoint: EP1, async, 392 B max, interval 1 */
    9, TUSB_DESC_ENDPOINT, EPNUM_AUDIO_IN, 0x05, U16_TO_U8S_LE(392), 1, 0, 0,

    /* CS isochronous endpoint: sampling-frequency control, ms lock delay */
    7, TUSB_DESC_CS_ENDPOINT, AUDIO_CS_EP_SUBTYPE_GENERAL, 0x01, 1, U16_TO_U8S_LE(1)
};

/* Compile-time check that the descriptor has the expected size. */
TU_VERIFY_STATIC(sizeof(desc_configuration) == CONFIG_TOTAL_LEN,
                 "Configuration descriptor length mismatch");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

/* ---- String descriptors ------------------------------------------------ */
static char const *const string_desc_arr[] = {
    (const char[]){0x09, 0x04},  /* 0: LANGID — English (0x0409) */
    "SomethingFourier",                   /* 1: Manufacturer */
    "Software-Defined Radio",        /* 2: Product */
    "000002",                    /* 3: Serial number */
    "SDR Control",               /* 4: CDC control interface */
    "SDR Audio",                 /* 5: Audio interface */
};

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= (uint8_t)(sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t)(((uint16_t)TUSB_DESC_STRING << 8) | (2u * chr_count + 2u));
    return _desc_str;
}
