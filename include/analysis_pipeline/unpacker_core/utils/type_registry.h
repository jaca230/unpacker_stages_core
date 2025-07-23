#ifndef UNPACKER_CORE_UTILS_TYPE_REGISTRY_H
#define UNPACKER_CORE_UTILS_TYPE_REGISTRY_H

#include <string>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <memory>
#include <TObject.h>
#include <TDataMember.h>
#include <spdlog/spdlog.h>
#include <cstring>
#include <algorithm>

class TypeRegistry {
public:
    using HandlerFunc = std::function<bool(const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, TObject* obj, TDataMember* member)>;

    static TypeRegistry& Instance();

    HandlerFunc GetHandler(const std::string& type_name) const;

    void RegisterHandler(const std::string& type_name, HandlerFunc handler);

private:
    TypeRegistry();
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    std::unordered_map<std::string, HandlerFunc> handlers_;

    // Template utilities defined here (inline in header)
    template<typename T>
    static bool ReadValue(const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, T& out_value) {
        if (offset + sizeof(T) > buffer_size) {
            spdlog::error("TypeRegistry: Buffer overrun while reading {} bytes at offset {}", sizeof(T), offset);
            return false;
        }
        std::memcpy(&out_value, buffer + offset, sizeof(T));

        uint16_t test = 0x1;
        bool system_little_endian = (*reinterpret_cast<uint8_t*>(&test) == 0x1);

        if (little_endian != system_little_endian) {
            SwapBytes(&out_value, sizeof(T));
        }
        return true;
    }

    template<typename T>
    static bool AssignValue(TObject* obj, TDataMember* member, const T& value) {
        char* base = reinterpret_cast<char*>(obj);
        char* member_ptr = base + member->GetOffset();
        std::memcpy(member_ptr, &value, sizeof(T));
        return true;
    }

    static void SwapBytes(void* data, size_t size) {
        uint8_t* bytes = static_cast<uint8_t*>(data);
        for (size_t i = 0; i < size / 2; ++i) {
            std::swap(bytes[i], bytes[size - 1 - i]);
        }
    }
};

#endif // UNPACKER_CORE_UTILS_TYPE_REGISTRY_H
