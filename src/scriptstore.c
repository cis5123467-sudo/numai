#include "scriptstore.h"

#include <string.h>

static unsigned read_size(const uint8_t *p) {
    return p[0] | (unsigned)p[1] << 8;
}

bool script_record_next(ScriptStore store, size_t *position, ScriptRecord *record) {
    if (!store.records || *position > store.capacity || store.capacity - *position < 2) return false;
    const uint8_t *start = store.records + *position;
    unsigned length = read_size(start);
    if (length < 4 || length > store.capacity - *position - 2) return false;
    const char *name = (const char *)start + 2;
    const char *name_end = memchr(name, 0, length - 2);
    if (!name_end || name == name_end) return false;
    size_t size = length - (size_t)(name_end + 1 - (const char *)start);
    *position += length;
    *record = (ScriptRecord){name, size >= 2 ? name_end + 2 : NULL, size >= 2 ? size - 1 : 0};
    return true;
}

static bool valid_store(ScriptStore store) {
    size_t position = 0;
    ScriptRecord record;
    while (script_record_next(store, &position, &record)) {}
    return store.records && store.capacity - position >= 2 && !read_size(store.records + position);
}

bool script_record_save(ScriptStore store, const char *name, const char *owner,
                        const char *source, size_t size) {
    if (!valid_store(store) || size < 2 || source[size - 1] != '\0') return false;
    size_t position = 0;
    ScriptRecord record;
    while (script_record_next(store, &position, &record)) {
        if (strcmp(record.name, name)) continue;
        size_t owner_length = strlen(owner);
        if (record.size != size || size <= owner_length || strncmp(record.source, owner, owner_length))
            return false;
        // Same-size replacement leaves firmware record pointers and names intact.
        memcpy((char *)record.source, source, size);
        return true;
    }
    size_t name_size = strlen(name) + 1, needed = 3 + name_size + size;
    if (needed > UINT16_MAX || needed + 2 > store.capacity - position) return false;
    uint8_t *start = store.records + position;
    memcpy(start + 2, name, name_size);
    start[2 + name_size] = 0; // Do not auto-import this script in the Python shell.
    memcpy(start + 3 + name_size, source, size);
    start[needed] = start[needed + 1] = 0;
    // Publish the new record after its payload and end marker are in place.
    __asm__ volatile("" ::: "memory");
    start[1] = (uint8_t)(needed >> 8);
    start[0] = (uint8_t)needed;
    return true;
}

ScriptStore calculator_script_store(void) {
#if PLATFORM_DEVICE
    // Epsilon publishes this metadata in the userland image header. Use only
    // the official N0120 layout tested by the application, and fail closed.
    typedef struct {
        uint32_t magic;
        char version[8];
        uint32_t storage, capacity, apps_start, apps_end, apps_ram_start, apps_ram_end;
        uint32_t device_start, device_end, footer;
    } Userland;
    static const uintptr_t headers[] = {0x90020000u, 0x90420000u};
    const uint32_t signature = 0xdec0edfeu, store_signature = 0xee0bddbau;
    uintptr_t code = (uintptr_t)&calculator_script_store;
    ScriptStore result = {0};
    for (unsigned i = 0; i < sizeof(headers) / sizeof(*headers); i++) {
        const Userland *header = (const Userland *)headers[i];
        if (header->magic != signature || header->footer != signature ||
            memcmp(header->version, "24.8.0", sizeof("24.8.0")) ||
            code < header->apps_start || code >= header->apps_end) continue;
        uintptr_t addr = header->storage;
        uint32_t size = header->capacity;
        if (addr < 0x24000000u || addr >= 0x24040000u || (addr & 3) ||
            size < 1024 || size > 0x10000u || size + 8 > 0x24040000u - addr)
            continue;
        if (*(const uint32_t *)addr != store_signature ||
            memcmp((const void *)(addr + 4 + size), &store_signature, sizeof(store_signature))) continue;
        if (result.records) return (ScriptStore){0};
        result = (ScriptStore){(uint8_t *)(addr + 4), size};
    }
    if (valid_store(result)) return result;
#endif
    return (ScriptStore){0};
}
