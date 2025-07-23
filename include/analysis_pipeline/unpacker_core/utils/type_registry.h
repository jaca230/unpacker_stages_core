#ifndef UNPACKER_CORE_UTILS_TYPE_REGISTRY_H
#define UNPACKER_CORE_UTILS_TYPE_REGISTRY_H

#include <string>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <memory>
#include <TObject.h>
#include <TDataMember.h>
#include <cstring>
#include <algorithm>
#include <regex>

class TypeRegistry {
public:
    using HandlerFunc = std::function<bool(const uint8_t* buffer,
                                           size_t buffer_size,
                                           size_t offset,
                                           bool little_endian,
                                           TObject* obj,
                                           TDataMember* member)>;

    static TypeRegistry& Instance();

    HandlerFunc GetHandler(const std::string& type_name) const;

    void RegisterHandler(const std::string& type_name, HandlerFunc handler);

private:
    TypeRegistry();
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    bool system_little_endian_;

    std::unordered_map<std::string, HandlerFunc> handlers_;
    std::unordered_map<std::string, size_t> sizes_;

    template<typename T>
    bool ReadValue(const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, T& out_value) const {
        if (offset + sizeof(T) > buffer_size) {
            return false;
        }
        std::memcpy(&out_value, buffer + offset, sizeof(T));

        if (little_endian != system_little_endian_) {
            SwapBytes(&out_value, sizeof(T));
        }
        return true;
    }

    template<typename T>
    bool AssignValue(TObject* obj, TDataMember* member, const T& value) const {
        char* base = reinterpret_cast<char*>(obj);
        char* member_ptr = base + member->GetOffset();
        std::memcpy(member_ptr, &value, sizeof(T));
        return true;
    }

    static void SwapBytes(void* data, size_t size);

    size_t GetTypeSize(const std::string& type_name) const;
};

#endif // UNPACKER_CORE_UTILS_TYPE_REGISTRY_H
