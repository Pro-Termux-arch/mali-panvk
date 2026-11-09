// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pro-Termux-arch
// Uses only libadrenotools' custom-driver loader. Never calls KGSL/turbo APIs.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <adrenotools/driver.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <jni.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>

static PFN_vkGetInstanceProcAddr loader_gipa;
static bool configured;

extern "C" __attribute__((visibility("default"))) PFN_vkVoidFunction
vk_icdGetInstanceProcAddr(VkInstance instance, const char *name)
{
    return loader_gipa ? loader_gipa(instance, name) : nullptr;
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_protermuxarch_exynosprobe_Native_configure(JNIEnv *env, jclass, jstring directory, jboolean custom)
{
    if (configured) return env->NewStringUTF("FAIL restart process before driver selection");
    configured = true;
    const char *chars = env->GetStringUTFChars(directory, nullptr);
    if (!chars) return nullptr;
    std::string dir = std::string(chars) + "/";
    env->ReleaseStringUTFChars(directory, chars);
    void *loader = custom ? adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL,
        ADRENOTOOLS_DRIVER_CUSTOM, nullptr, dir.c_str(), dir.c_str(),
        "libvulkan_panfrost.so", nullptr, nullptr) : dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!loader) return env->NewStringUTF("FAIL isolated loader creation (no silent stock fallback)");
    loader_gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(loader, "vkGetInstanceProcAddr"));
    if (!loader_gipa) return env->NewStringUTF("FAIL missing loader entry point");
    auto create = reinterpret_cast<PFN_vkCreateInstance>(loader_gipa(nullptr, "vkCreateInstance"));
    if (!create) return env->NewStringUTF("FAIL missing create instance");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    VkInstance instance;
    VkResult r = create(&ci, nullptr, &instance);
    if (r != VK_SUCCESS) return env->NewStringUTF("FAIL loader vkCreateInstance");
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(loader_gipa(instance, "vkEnumeratePhysicalDevices"));
    auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(loader_gipa(instance, "vkGetPhysicalDeviceProperties2"));
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(loader_gipa(instance, "vkDestroyInstance"));
    if (!enumerate || !properties || !destroy) return env->NewStringUTF("FAIL loader dispatch");
    uint32_t count = 8;
    VkPhysicalDevice devices[8];
    r = enumerate(instance, &count, devices);
    std::string result = "FAIL requested G78 driver not enumerated (stock fallback rejected)";
    if (r == VK_SUCCESS) for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        props.pNext = &driver;
        properties(devices[i], &props);
        if (props.properties.vendorID != 0x13b5 || !strstr(props.properties.deviceName, "G78") ||
            props.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) continue;
        if (driver.driverID != (custom ? VK_DRIVER_ID_MESA_PANVK : VK_DRIVER_ID_ARM_PROPRIETARY)) continue;
        char text[1024];
        snprintf(text, sizeof(text), "PASS selected=%s device=%s physical_api=%u.%u.%u driver=%s info=%s",
            custom ? "panvk" : "stock", props.properties.deviceName,
            VK_API_VERSION_MAJOR(props.properties.apiVersion), VK_API_VERSION_MINOR(props.properties.apiVersion),
            VK_API_VERSION_PATCH(props.properties.apiVersion), driver.driverName, driver.driverInfo);
        result = text;
        break;
    }
    destroy(instance, nullptr);
    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_protermuxarch_exynosprobe_Native_preflight(JNIEnv *env, jclass, jstring directory, jstring output)
{
    if (!loader_gipa) return env->NewStringUTF("preflight unavailable: loader not configured");
    const char *d = env->GetStringUTFChars(directory, nullptr);
    if (!d) return nullptr;
    std::string dir(d);
    env->ReleaseStringUTFChars(directory, d);
    const char *o = env->GetStringUTFChars(output, nullptr);
    if (!o) return nullptr;
    std::string out(o);
    env->ReleaseStringUTFChars(output, o);
    std::string result;
    for (const char *name : {"dxvk_reqs", "vkd3d_reqs"}) {
        std::string path = dir + "/lib" + name + ".so";
        void *module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        auto entry = module ? reinterpret_cast<int (*)(int, char **)>(dlsym(module, "panprobe_main")) : nullptr;
        int code = -1;
        if (entry) {
            // One worker thread, before rendering starts. Restore stdout even
            // on query failure; never touch Android's log buffers or policies.
            fflush(stdout);
            int saved = dup(STDOUT_FILENO);
            int file = open((out + "/" + name + ".log").c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
            if (saved >= 0 && file >= 0 && dup2(file, STDOUT_FILENO) >= 0) {
                std::string bridge = dir + "/libexynospresent.so";
                char *args[] = {const_cast<char *>(name), const_cast<char *>(bridge.c_str()), nullptr};
                code = entry(2, args);
                fflush(stdout);
            }
            if (saved >= 0) { dup2(saved, STDOUT_FILENO); close(saved); }
            if (file >= 0) close(file);
        }
        if (module) dlclose(module);
        if (!result.empty()) result += "; ";
        result += std::string(name) + " query_exit=" + std::to_string(code);
    }
    return env->NewStringUTF(result.c_str());
}
