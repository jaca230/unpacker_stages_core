#include "analysis_pipeline/unpacker_core/utils/type_registry.h"
#include <spdlog/spdlog.h>
#include <regex>
#include <cstring>

TypeRegistry& TypeRegistry::Instance() {
    static TypeRegistry instance;
    return instance;
}

TypeRegistry::TypeRegistry() {
    uint16_t test = 0x1;
    system_little_endian_ = (*reinterpret_cast<uint8_t*>(&test) == 0x1);

#define REGISTER_TYPE(TYPE_NAME, CPP_TYPE) \
    handlers_[TYPE_NAME] = [this](const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, TObject* obj, TDataMember* member) -> bool { \
        CPP_TYPE val; \
        if (!ReadValue<CPP_TYPE>(buffer, buffer_size, offset, little_endian, val)) { \
            spdlog::error("Failed to read value of type {} at offset {}", TYPE_NAME, offset); \
            return false; \
        } \
        return AssignValue<CPP_TYPE>(obj, member, val); \
    }; \
    sizes_[TYPE_NAME] = sizeof(CPP_TYPE);

    REGISTER_TYPE("char", int8_t)
    REGISTER_TYPE("unsigned char", uint8_t)
    REGISTER_TYPE("short", int16_t)
    REGISTER_TYPE("unsigned short", uint16_t)
    REGISTER_TYPE("int", int32_t)
    REGISTER_TYPE("unsigned int", uint32_t)
    REGISTER_TYPE("long", int64_t)
    REGISTER_TYPE("unsigned long", uint64_t)
    REGISTER_TYPE("Long64_t", int64_t)
    REGISTER_TYPE("ULong64_t", uint64_t)
    REGISTER_TYPE("float", float)
    REGISTER_TYPE("double", double)
    REGISTER_TYPE("bool", bool)

#undef REGISTER_TYPE
}

size_t TypeRegistry::GetTypeSize(const std::string& type_name) const {
    auto it = sizes_.find(type_name);
    if (it != sizes_.end()) {
        return it->second;
    }

    std::smatch match;
    std::regex root_array_regex(R"(array<([^,>]+),(\d+)>)");

    if (std::regex_match(type_name, match, root_array_regex)) {
        std::string base_type = match[1];
        size_t count = std::stoul(match[2]);

        base_type.erase(0, base_type.find_first_not_of(" \t"));
        base_type.erase(base_type.find_last_not_of(" \t") + 1);

        size_t base_size = GetTypeSize(base_type);
        return base_size * count;
    }

    spdlog::warn("Unknown type size for '{}'", type_name);
    return 0;
}

TypeRegistry::HandlerFunc TypeRegistry::GetHandler(const std::string& type_name) const {
    auto it = handlers_.find(type_name);
    if (it != handlers_.end()) {
        return it->second;
    }

    std::smatch match;
    std::regex root_array_regex(R"(array<([^,>]+),(\d+)>)");

    if (std::regex_match(type_name, match, root_array_regex)) {
        std::string base_type = match[1];
        size_t count = std::stoul(match[2]);

        base_type.erase(0, base_type.find_first_not_of(" \t"));
        base_type.erase(base_type.find_last_not_of(" \t") + 1);

        size_t element_size = GetTypeSize(base_type);
        if (element_size == 0) {
            spdlog::warn("Unknown size for base type '{}'", base_type);
            return nullptr;
        }

        return [count, element_size, base_type, this](
            const uint8_t* buffer, size_t buffer_size,
            size_t offset, bool little_endian,
            TObject* obj, TDataMember* member) -> bool {

            size_t total_size = count * element_size;
            if (offset + total_size > buffer_size) {
                spdlog::error("Buffer too small for array '{}', needed {} bytes but buffer size is {}", 
                              member->GetName(), total_size, buffer_size - offset);
                return false;
            }

            char* base = reinterpret_cast<char*>(obj);
            char* member_ptr = base + member->GetOffset();

            if (little_endian == system_little_endian_) {
                std::memcpy(member_ptr, buffer + offset, total_size);
            } else {
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* src = buffer + offset + i * element_size;
                    char* dst = member_ptr + i * element_size;
                    std::memcpy(dst, src, element_size);
                    SwapBytes(dst, element_size);
                }
            }

            return true;
        };
    }

    spdlog::warn("No handler found for type '{}'", type_name);
    return nullptr;
}

void TypeRegistry::RegisterHandler(const std::string& type_name, HandlerFunc handler) {
    handlers_[type_name] = std::move(handler);
}

void TypeRegistry::SwapBytes(void* data, size_t size) {
    uint8_t* bytes = static_cast<uint8_t*>(data);
    for (size_t i = 0; i < size / 2; ++i) {
        std::swap(bytes[i], bytes[size - 1 - i]);
    }
}
