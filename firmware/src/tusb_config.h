#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// Device mode on the Pico.
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                 OPT_OS_PICO
#endif

#define CFG_TUD_ENDPOINT0_SIZE      64

#define CFG_TUD_CDC                 1
#define CFG_TUD_CDC_RX_BUFSIZE      256
#define CFG_TUD_CDC_TX_BUFSIZE      256


// --------------------- //
// -*-*-*- Audio -*-*-*- //
// --------------------- //
#define CFG_TUD_AUDIO                          1   // UAC1 capture

// One Audio Streaming interface (Interface 3)
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT          1

// Control request buffer (mute, sampling-freq queries from host)
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ      64

// Enable the IN endpoint (device → host audio data)
#define CFG_TUD_AUDIO_ENABLE_EP_IN             1

// Max isochronous packet: 49 samples × 2 ch × 2 bytes = 196 B worst-case
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX     196

// Software FIFO: double-buffer (384 B = 2 × 192 nominal)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ  384


#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
