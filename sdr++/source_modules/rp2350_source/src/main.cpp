#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <utils/optionlist.h>
#include <RtAudio.h>

#include <string>
#include <vector>
#include <glob.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <chrono>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "rp2350_source",
    /* Description:     */ "Custom RP2350 SDR Source",
    /* Author:          */ "Antigravity",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

struct DeviceInfo {
    RtAudio::DeviceInfo info;
    int id;
    bool operator==(const struct DeviceInfo& other) const {
        return other.id == id;
    }
};

class RP2350SourceModule : public ModuleManager::Instance {
public:
    RP2350SourceModule(std::string name) {
        this->name = name;

#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&errorCallback);
#endif

        sampleRate = 48000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();

        std::string device = "";
        config.acquire();
        if (config.conf.contains("device")) {
            device = config.conf["device"];
        }
        config.release();
        select(device);
        
        sigpath::sourceManager.registerSource("RP2350", &handler);
    }

    ~RP2350SourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("RP2350");
    }

    void postInit() {}
    void enable() { enabled = true; }
    void disable() { enabled = false; }
    bool isEnabled() { return enabled; }

    void refresh() {
        devices.clear();

#if RTAUDIO_VERSION_MAJOR >= 6
        for (int i : audio.getDeviceIds()) {
#else
        int count = audio.getDeviceCount();
        for (int i = 0; i < count; i++) {
#endif
            try {
                auto info = audio.getDeviceInfo(i);
#if !defined(RTAUDIO_VERSION_MAJOR) || RTAUDIO_VERSION_MAJOR < 6
                if (!info.probed) { continue; }
#endif
                if (info.inputChannels < 2) { continue; }
                DeviceInfo dinfo = { info, i };
                devices.define(info.name, info.name, dinfo);
                
                // Auto-select SDR Audio if found
                if (std::string(info.name).find("SDR Audio") != std::string::npos) {
                    selectedDevice = info.name;
                }
            }
            catch (const std::exception& e) {
                flog::error("Error getting audio device ({}) info: {}", i, e.what());
            }
        }
    }

    void select(std::string devname) {
        if (devices.empty()) {
            selectedDevice.clear();
            return;
        }

        if (!devices.keyExists(devname)) {
            if (devices.keyExists(selectedDevice)) {
                devname = selectedDevice;
            } else {
                devname = devices.key(0);
            }
        }
        
        devId = devices.keyId(devname);
        selectedDevice = devname;
    }

private:
    static void menuSelected(void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("RP2350SourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;
        flog::info("RP2350SourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;
        if (_this->running) { return; }

        if (_this->selectedDevice.empty()) { return; }
        
        RtAudio::StreamParameters parameters;
        parameters.deviceId = _this->devices[_this->devId].id;
        parameters.nChannels = 2;
        unsigned int bufferFrames = _this->sampleRate / 200;
        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_MINIMIZE_LATENCY;
        opts.streamName = "RP2350 IQ Source";

        try {
            _this->audio.openStream(NULL, &parameters, RTAUDIO_FLOAT32, _this->sampleRate, &bufferFrames, callback, _this, &opts);
            _this->audio.startStream();
            _this->running = true;
        }
        catch (const std::exception& e) {
            flog::error("Error opening audio device: {}", e.what());
            return;
        }
        
        // Open serial port for tuning
        glob_t glob_result;
        memset(&glob_result, 0, sizeof(glob_result));
        int ret = glob("/dev/cu.usbmodem*", GLOB_TILDE, NULL, &glob_result);
        if (ret == 0 && glob_result.gl_pathc > 0) {
            std::string port = glob_result.gl_pathv[0];
            _this->serial_fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
            if (_this->serial_fd >= 0) {

                struct termios tty;
                if (tcgetattr(_this->serial_fd, &tty) == 0) {
                    cfsetospeed(&tty, B115200);
                    cfsetispeed(&tty, B115200);
                    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
                    tty.c_iflag &= ~IGNBRK;
                    tty.c_lflag = 0;
                    tty.c_oflag = 0;
                    tty.c_cc[VMIN]  = 0;
                    tty.c_cc[VTIME] = 0;
                    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
                    tty.c_cflag |= (CLOCAL | CREAD);
                    tty.c_cflag &= ~(PARENB | PARODD);
                    tty.c_cflag &= ~CSTOPB;
                    tty.c_cflag &= ~CRTSCTS;
                    tcsetattr(_this->serial_fd, TCSANOW, &tty);
                    
                    // Sync frequency on start
                    std::string cmd = "FREQ," + std::to_string(static_cast<long long>(_this->currentFreq)) + "\n";
                    write(_this->serial_fd, cmd.c_str(), cmd.length());
                }
            } else {
                flog::warn("RP2350SourceModule: Found {}, but failed to open for tuning", port);
            }
        } else {
            flog::warn("RP2350SourceModule: No /dev/cu.usbmodem* device found for tuning!");
        }
        globfree(&glob_result);

        flog::info("RP2350SourceModule '{}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        _this->audio.stopStream();
        _this->audio.closeStream();

        if (_this->serial_fd >= 0) {
            close(_this->serial_fd);
            _this->serial_fd = -1;
        }

        flog::info("RP2350SourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;
        _this->currentFreq = freq;
        // Frequency is actually synced over serial in the background audio callback
        // to prevent rapid UI tune events from flooding the microcontroller.
    }

    static void menuHandler(void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rp2350_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            std::string dev = _this->devices.key(_this->devId);
            _this->select(dev);
            config.acquire();
            config.conf["device"] = dev;
            config.release(true);
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_rp2350_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedDevice);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        SmGui::SliderFloat(CONCAT("Digital Gain##_rp2350_gain_", _this->name), &_this->digitalGain, 1.0f, 1000.0f);
    }

    static int callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
        RP2350SourceModule* _this = (RP2350SourceModule*)userData;
        
        // Sync frequency in the background (non-blocking)
        if (_this->serial_fd >= 0 && _this->currentFreq != _this->lastSentFreq) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _this->lastSentTime).count() > 50) {
                _this->lastSentTime = now;
                _this->lastSentFreq = _this->currentFreq;
                std::string cmd = "FREQ," + std::to_string(static_cast<long long>(_this->currentFreq)) + "\n";
                write(_this->serial_fd, cmd.c_str(), cmd.length());
            }
        }

        // Optional debug: Print peak amplitude every ~1 second to verify data isn't silent
        static int debug_counter = 0;
        if (debug_counter++ % 200 == 0) {
            float* fbuf = (float*)inputBuffer;
            float max_val = 0.0f;
            for (unsigned int i = 0; i < nBufferFrames * 2; i++) {
                if (std::abs(fbuf[i]) > max_val) max_val = std::abs(fbuf[i]);
            }
            flog::info("RP2350 Audio Peak Amplitude: {:.6f}", max_val);
        }

        memcpy(_this->stream.writeBuf, inputBuffer, nBufferFrames * sizeof(dsp::complex_t));
        
        if (_this->digitalGain != 1.0f) {
            float* wbuf = (float*)_this->stream.writeBuf;
            for (unsigned int i = 0; i < nBufferFrames * 2; i++) {
                wbuf[i] *= _this->digitalGain;
            }
        }

        _this->stream.swap(nBufferFrames);
        return 0;
    }

#if RTAUDIO_VERSION_MAJOR >= 6
    static void errorCallback(RtAudioErrorType type, const std::string& errorText) {
        switch (type) {
        case RtAudioErrorType::RTAUDIO_NO_ERROR:
            return;
        case RtAudioErrorType::RTAUDIO_WARNING:
        case RtAudioErrorType::RTAUDIO_NO_DEVICES_FOUND:
        case RtAudioErrorType::RTAUDIO_DEVICE_DISCONNECT:
            flog::warn("RP2350SourceModule Warning: {} ({})", errorText, (int)type);
            break;
        default:
            flog::error("RP2350SourceModule Error: {}", errorText);
        }
    }
#endif

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    
    OptionList<std::string, DeviceInfo> devices;
    std::string selectedDevice = "";
    int devId = 0;

    RtAudio audio;
    int serial_fd = -1;
    double currentFreq = 100000000.0;
    double lastSentFreq = -1.0;
    std::chrono::steady_clock::time_point lastSentTime;
    float digitalGain = 1.0f;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/rp2350_source_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RP2350SourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (RP2350SourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
