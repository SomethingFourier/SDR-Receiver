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
#ifdef _WIN32
#include <windows.h>
#else
#include <glob.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#include <string.h>
#include <chrono>
#include <thread>

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

        sampleRate = 192000.0;

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
        if (config.conf.contains("comPort")) {
            selectedComPort = config.conf["comPort"];
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

        comPorts.clear();
#ifdef _WIN32
        for (int i = 1; i <= 256; i++) {
            std::string port = "COM" + std::to_string(i);
            std::string path = "\\\\.\\" + port;
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                comPorts.define(port, port, port);
                CloseHandle(h);
                if (selectedComPort.empty()) selectedComPort = port;
            } else {
                if (GetLastError() == ERROR_ACCESS_DENIED) {
                    comPorts.define(port, port + " (In Use)", port);
                }
            }
        }
#else
        glob_t glob_result;
        memset(&glob_result, 0, sizeof(glob_result));
        int ret = glob("/dev/cu.usbmodem*", GLOB_TILDE, NULL, &glob_result);
        if (ret == 0) {
            for(size_t i = 0; i < glob_result.gl_pathc; ++i) {
                std::string port = glob_result.gl_pathv[i];
                comPorts.define(port, port, port);
                if (selectedComPort.empty()) selectedComPort = port;
            }
        }
        globfree(&glob_result);
#endif
        if (!comPorts.empty() && comPorts.keyExists(selectedComPort)) {
            comId = comPorts.keyId(selectedComPort);
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
        auto info = devices.value(devId).info;

        sampleRates.clear();
        for (const auto& sr : info.sampleRates) {
            std::string name = std::to_string(sr) + " Hz";
            if (sr >= 1000) name = std::to_string(sr / 1000) + " kHz";
            sampleRates.define(sr, name, sr);
            if (sr == info.preferredSampleRate) {
                srId = sampleRates.valueId(sr);
            }
        }

        config.acquire();
        if (config.conf["devices"][selectedDevice].contains("sampleRate")) {
            sampleRate = config.conf["devices"][selectedDevice]["sampleRate"];
            if (sampleRates.keyExists(sampleRate)) {
                srId = sampleRates.keyId(sampleRate);
            }
        }
        config.release();

        if (sampleRates.empty()) {
            sampleRate = 192000.0;
        } else {
            sampleRate = sampleRates[srId];
        }
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

        core::setInputSampleRate(_this->sampleRate);
        
        RtAudio::StreamParameters parameters;
        parameters.deviceId = _this->devices[_this->devId].id;
        parameters.nChannels = 2;
        unsigned int bufferFrames = _this->sampleRate / 200;
        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_MINIMIZE_LATENCY;
        opts.streamName = "RP2350 IQ Source";

        try {
            _this->audio.openStream(NULL, &parameters, RTAUDIO_SINT16, _this->sampleRate, &bufferFrames, callback, _this, &opts);
            _this->audio.startStream();
            _this->running = true;
        }
        catch (const std::exception& e) {
            flog::error("Error opening audio device: {}", e.what());
            return;
        }
        
        // Open serial port for tuning
#ifdef _WIN32
        if (!_this->selectedComPort.empty()) {
            std::string port = "\\\\.\\" + _this->selectedComPort;
            _this->serial_fd = CreateFileA(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (_this->serial_fd != INVALID_HANDLE_VALUE) {
                DCB dcbSerialParams = {0};
                dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
                if (GetCommState(_this->serial_fd, &dcbSerialParams)) {
                    dcbSerialParams.BaudRate = CBR_115200;
                    dcbSerialParams.ByteSize = 8;
                    dcbSerialParams.StopBits = ONESTOPBIT;
                    dcbSerialParams.Parity = NOPARITY;
                    SetCommState(_this->serial_fd, &dcbSerialParams);
                    COMMTIMEOUTS timeouts = {0};
                    timeouts.ReadIntervalTimeout = 50;
                    timeouts.ReadTotalTimeoutConstant = 50;
                    timeouts.ReadTotalTimeoutMultiplier = 10;
                    timeouts.WriteTotalTimeoutConstant = 50;
                    timeouts.WriteTotalTimeoutMultiplier = 10;
                    SetCommTimeouts(_this->serial_fd, &timeouts);

                    // Sync frequency on start
                    std::string cmd = "FREQ," + std::to_string(static_cast<long long>(_this->currentFreq)) + "\n";
                    DWORD bytesWritten;
                    WriteFile(_this->serial_fd, cmd.c_str(), cmd.length(), &bytesWritten, NULL);
                    flog::info("RP2350SourceModule: Opened {} for tuning", port);
                } else {
                    CloseHandle(_this->serial_fd);
                    _this->serial_fd = INVALID_HANDLE_VALUE;
                    flog::error("RP2350SourceModule: Failed to set COM state on {}", port);
                }
            } else {
                flog::error("RP2350SourceModule: Failed to open COM port {}", port);
            }
        } else {
            flog::warn("RP2350SourceModule: No COM port selected for tuning!");
        }
#else
        if (!_this->selectedComPort.empty()) {
            std::string port = _this->selectedComPort;
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
                flog::error("RP2350SourceModule: Failed to open port {}", port);
            }
        } else {
            flog::warn("RP2350SourceModule: No COM port selected for tuning!");
        }
#endif

        _this->tuneThreadStop = false;
        _this->tuneThread = new std::thread(tuneWorker, _this);

        flog::info("RP2350SourceModule '{}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        if (_this->tuneThread) {
            _this->tuneThreadStop = true;
            _this->tuneThread->join();
            delete _this->tuneThread;
            _this->tuneThread = nullptr;
        }

        _this->audio.stopStream();
        _this->audio.closeStream();

#ifdef _WIN32
        if (_this->serial_fd != INVALID_HANDLE_VALUE) {
            CloseHandle(_this->serial_fd);
            _this->serial_fd = INVALID_HANDLE_VALUE;
        }
#else
        if (_this->serial_fd >= 0) {
            close(_this->serial_fd);
            _this->serial_fd = -1;
        }
#endif

        flog::info("RP2350SourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;
        _this->currentFreq = freq;
    }

    static void tuneWorker(RP2350SourceModule* _this) {
        while (!_this->tuneThreadStop) {
#ifdef _WIN32
            if (_this->serial_fd != INVALID_HANDLE_VALUE && _this->currentFreq != _this->lastSentFreq) {
                _this->lastSentFreq = _this->currentFreq;
                std::string cmd = "FREQ," + std::to_string(static_cast<long long>(_this->currentFreq)) + "\n";
                DWORD bytesWritten;
                WriteFile(_this->serial_fd, cmd.c_str(), cmd.length(), &bytesWritten, NULL);
            }
#else
            if (_this->serial_fd >= 0 && _this->currentFreq != _this->lastSentFreq) {
                _this->lastSentFreq = _this->currentFreq;
                std::string cmd = "FREQ," + std::to_string(static_cast<long long>(_this->currentFreq)) + "\n";
                write(_this->serial_fd, cmd.c_str(), cmd.length());
            }
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    static void menuHandler(void* ctx) {
        RP2350SourceModule* _this = (RP2350SourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rp2350_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            std::string dev = _this->devices.key(_this->devId);
            _this->select(dev);
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["device"] = dev;
            config.release(true);
        }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rp2350_sr_sel_", _this->name), &_this->srId, _this->sampleRates.txt)) {
            _this->sampleRate = _this->sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedDevice.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedDevice]["sampleRate"] = _this->sampleRate;
                config.release(true);
            }
        }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rp2350_com_sel_", _this->name), &_this->comId, _this->comPorts.txt)) {
            _this->selectedComPort = _this->comPorts.key(_this->comId);
            config.acquire();
            config.conf["comPort"] = _this->selectedComPort;
            config.release(true);
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_rp2350_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedDevice);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        SmGui::SliderFloat(CONCAT("Digital Gain##_rp2350_gain_", _this->name), &_this->digitalGain, 1.0f, 1000.0f);
    }

    static int callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
        RP2350SourceModule* _this = (RP2350SourceModule*)userData;
        
        int16_t* ibuf = (int16_t*)inputBuffer;
        float* wbuf = (float*)_this->stream.writeBuf;
        for (unsigned int i = 0; i < nBufferFrames * 2; i++) {
            wbuf[i] = ((float)ibuf[i]) / 32768.0f;
        }
        
        if (_this->digitalGain != 1.0f) {
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
    OptionList<double, double> sampleRates;
    OptionList<std::string, std::string> comPorts;
    std::string selectedDevice = "";
    std::string selectedComPort = "";
    int devId = 0;
    int srId = 0;
    int comId = 0;

    RtAudio audio;
#ifdef _WIN32
    HANDLE serial_fd = INVALID_HANDLE_VALUE;
#else
    int serial_fd = -1;
#endif
    double currentFreq = 100000000.0;
    double lastSentFreq = -1.0;
    std::chrono::steady_clock::time_point lastSentTime;
    float digitalGain = 1.0f;

    std::thread* tuneThread = nullptr;
    bool tuneThreadStop = false;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    def["comPort"] = "";
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
