#pragma once
#include <string>
#include <limits>

// Required ihx firmware files
#define PICO_FIRMWARE "pico.ihx"
#define BASESTATION_FIRMWARE "bs_mm_fw_0103.ihx"
#define PSU_FIRMWARE "psu_mm_fw_0102.ihx"

namespace EigenApi
{
    class Callback
    {
    public:
    	enum DeviceType {
    		PICO,
    		TAU,
    		ALPHA
    	};
    	virtual ~Callback() = default;
    	
        virtual void device(const char* dev, DeviceType dt, int rows, int cols, int ribbons, int pedals) {};
        virtual void disconnect(const char* dev, DeviceType dt) {};

        virtual void key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, unsigned p, int r, int y) {};
        virtual void breath(const char* dev, unsigned long long t, unsigned val) {};
        virtual void strip(const char* dev, unsigned long long t, unsigned strip, unsigned val, bool a) {};
        virtual void pedal(const char* dev, unsigned long long t, unsigned pedal, unsigned val) {};
        virtual void dead(const char* dev, unsigned reason) {};
    };

    // Firmware reader Interface for providing other implementations than the default Posix
    class IFW_Reader
    {
    public:
        virtual ~IFW_Reader() = default;
        virtual bool open(std::string filename, int oFlags, void* *fd) = 0;
        virtual ssize_t read(void* fd, void *data, size_t byteCount) = 0;
        virtual void close(void* fd) = 0;
        virtual bool confirmResources() = 0;
    };

    // Default firmware reader implementation for OSX/Linux
    class FWR_Posix : public EigenApi::IFW_Reader
    {
    public:
        explicit FWR_Posix(std::string path);
        bool open(std::string filename, int oFlags, void* *fd) override;
        ssize_t read(void* fd, void *data, size_t byteCount) override;
        void close(void* fd) override;
        bool confirmResources() override;
        void setPath(std::string path);
        std::string getPath();

    private:
        std::string path = "./";
    };


    class Eigenharp
    {
    public:
        explicit Eigenharp(IFW_Reader &fwReader);
        virtual ~Eigenharp();

        bool start();
        bool process();
        bool stop();

        // note: callback ownership is retained by caller
        void addCallback(Callback* api);
        void removeCallback(Callback* api);
        void clearCallbacks();
        
        void setLED(const char* dev, unsigned course, unsigned int key,unsigned int colour);

        void setPollTime(unsigned pollTime);

    private:
        void *impl;
    };
    
    class Logger
    {
    public:
        static void setLogFunc(void (*pLogFn)(const char*));
        static void logmsg(const char*);
    private:
        static void (*_logmsg)(const char*);
    };
}

