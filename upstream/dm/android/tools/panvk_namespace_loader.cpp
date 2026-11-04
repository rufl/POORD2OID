#include <android_linker_ns.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *buffer, size_t capacity, const char *message) {
    if (!buffer || !capacity) return;
    snprintf(buffer, capacity, "%s", message ? message : "unknown linker error");
}

extern "C" void *dm_panvk_namespace_open(const char *directory, const char *library,
                                         char *error, size_t error_capacity) {
    if (error && error_capacity) error[0] = '\0';
    if (!directory || !*directory || !library || !*library) {
        set_error(error, error_capacity, "PanVK namespace loader requires a directory and library name");
        return nullptr;
    }
    if (!linkernsbypass_load_status()) {
        set_error(error, error_capacity, "Android linker namespace bridge is unavailable");
        return nullptr;
    }

    android_namespace_t *driver_namespace = android_create_namespace(
        "d2r-panvk", directory, nullptr, ANDROID_NAMESPACE_TYPE_SHARED, nullptr, nullptr);
    if (!driver_namespace) {
        set_error(error, error_capacity, "could not create PanVK linker namespace");
        return nullptr;
    }
    if (!linkernsbypass_link_namespace_to_default_all_libs(driver_namespace)) {
        set_error(error, error_capacity, "could not link PanVK namespace to Android system libraries");
        return nullptr;
    }

    dlerror();
    void *handle = linkernsbypass_namespace_dlopen(library, RTLD_NOW | RTLD_LOCAL, driver_namespace);
    if (!handle) {
        set_error(error, error_capacity, dlerror());
        return nullptr;
    }
    return handle;
}
