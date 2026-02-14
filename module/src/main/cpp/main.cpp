#include <jni.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

// Fungsi untuk mengubah permission memori agar bisa ditulis (Bypass Read-Only)
void patch_mem(uintptr_t address, const char* data, size_t size) {
    uintptr_t page_start = address & ~(getpagesize() - 1);
    mprotect((void*)page_start, getpagesize() * 2, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy((void*)address, data, size);
    mprotect((void*)page_start, getpagesize() * 2, PROT_READ | PROT_EXEC);
}

// Fungsi mencari base address library
uintptr_t get_module_base(const char* lib_name) {
    uintptr_t base = 0;
    char line[512];
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, lib_name)) {
            base = strtoul(line, NULL, 16);
            break;
        }
    }
    fclose(f);
    return base;
}

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, const unsigned char *uuid, unsigned int uuid_len) override {
        this->api = api;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        const char *process = api->getJniEnv()->GetStringUTFChars(args->nice_name, nullptr);
        // !!! GANTI "com.example.game" dengan package name game kamu !!!
        if (process && std::string(process) == "com.example.game") {
            enable_patch = true;
        }
        api->getJniEnv()->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (enable_patch) {
            std::thread([]() {
                uintptr_t il2cpp = 0;
                // Tunggu sampai libil2cpp dimuat
                while ((il2cpp = get_module_base("libil2cpp.so")) == 0) {
                    sleep(1);
                }
                
                // Beri jeda 3 detik agar loading stabil
                sleep(3);

                // Patching RET (C0 03 5F D6)
                const char* ret = "\xD6\x5F\x03\xC0";
                patch_mem(il2cpp + 0x25F72B4, ret, 4); // Injection
                patch_mem(il2cpp + 0x25F9360, ret, 4); // Speed
                patch_mem(il2cpp + 0x25F7124, ret, 4); // Unknown
                patch_mem(il2cpp + 0x25FA15C, ret, 4); // Time
                patch_mem(il2cpp + 0x25F8908, ret, 4); // Obscured
                patch_mem(il2cpp + 0x25F80E0, ret, 4); // Report
                
                // Stealth Mode (MOV W0, #1 & RET)
                patch_mem(il2cpp + 0x25EE428, "\x20\x00\x80\x52", 4);
                patch_mem(il2cpp + 0x25EE42C, ret, 4);

            }).detach();
        }
    }

private:
    Api *api;
    bool enable_patch = false;
};

REGISTER_ZYGISK_MODULE(MyModule)
