#include "analysis_pipeline/unpacker_core/utils/type_registry.h"
#include <spdlog/spdlog.h>

TypeRegistry& TypeRegistry::Instance() {
    static TypeRegistry instance;
    return instance;
}

TypeRegistry::TypeRegistry() {
    // Initialize system endianness once
    uint16_t test = 0x1;
    system_little_endian_ = (*reinterpret_cast<uint8_t*>(&test) == 0x1);

#define REGISTER_TYPE(TYPE_NAME, CPP_TYPE) \
    handlers_[TYPE_NAME] = [this](const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, TObject* obj, TDataMember* member) -> bool { \
        CPP_TYPE val; \
        if (!ReadValue<CPP_TYPE>(buffer, buffer_size, offset, little_endian, val)) return false; \
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
    if (it != sizes_.end()) return it->second;

    // C-style array: int[5]
    std::regex array_regex(R"(([\w:]+)\[(\d+)\])");
    std::smatch match;
    if (std::regex_match(type_name, match, array_regex)) {
        std::string base_type = match[1];
        size_t count = std::stoul(match[2]);
        size_t base_size = GetTypeSize(base_type);
        return base_size * count;
    }

    // ROOT-style array<T,N>
    std::regex root_array_regex(R"(array<([\w:]+),(\d+)>)");
    if (std::regex_match(type_name, match, root_array_regex)) {
        std::string base_type = match[1];
        size_t count = std::stoul(match[2]);
        size_t base_size = GetTypeSize(base_type);
        return base_size * count;
    }

    return 0;
}

TypeRegistry::HandlerFunc TypeRegistry::GetHandler(const std::string& type_name) const {
    // Direct handler first
    auto it = handlers_.find(type_name);
    if (it != handlers_.end()) return it->second;

    // Handle C-style arrays: type[N]
    std::regex array_regex(R"(([\w:]+)\[(\d+)\])");
    std::smatch match;
    if (std::regex_match(type_name, match, array_regex)) {
        std::string base_type = match[1];
        size_t count = std::stoul(match[2]);

        auto base_handler_it = handlers_.find(base_type);
        if (base_handler_it == handlers_.end()) {
            spdlog::warn("No handler for base type '{}'", base_type);
            return nullptr;
        }

        size_t element_size = GetTypeSize(base_type);
        if (element_size == 0) {
            spdlog::warn("Unknown size for base type '{}'", base_type);
            return nullptr;
        }

        return [base_handler = base_handler_it->second, count, element_size](const uint8_t* buffer, size_t buffer_size,
                                                                             size_t offset, bool little_endian,
                                                                             TObject* obj, TDataMember* member) -> bool {
            for (size_t i = 0; i < count; ++i) {
                size_t elem_offset = offset + i * element_size;
                if (!base_handler(buffer, buffer_size, elem_offset, little_endian, obj, member)) {
                    spdlog::error("Failed to handle array element {} at offset {}", i, elem_offset);
                    return false;
                }
            }
            return true;
        };
    }

    // Handle ROOT style array<T,N>
    std::regex root_array_regex(R"(array<([\w:]+),(\d+)>)");
    if (std::regex_match(type_name, match, root_array_regex)) {
        std::string base_type = match[1];
        size_t count = std::stoul(match[2]);

        auto base_handler_it = handlers_.find(base_type);
        if (base_handler_it == handlers_.end()) {
            spdlog::warn("No handler for base type '{}'", base_type);
            return nullptr;
        }

        size_t element_size = GetTypeSize(base_type);
        if (element_size == 0) {
            spdlog::warn("Unknown size for base type '{}'", base_type);
            return nullptr;
        }

        return [base_handler = base_handler_it->second, count, element_size](const uint8_t* buffer, size_t buffer_size,
                                                                             size_t offset, bool little_endian,
                                                                             TObject* obj, TDataMember* member) -> bool {
            for (size_t i = 0; i < count; ++i) {
                size_t elem_offset = offset + i * element_size;
                if (!base_handler(buffer, buffer_size, elem_offset, little_endian, obj, member)) {
                    spdlog::error("Failed to handle ROOT array element {} at offset {}", i, elem_offset);
                    return false;
                }
            }
            return true;
        };
    }

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
