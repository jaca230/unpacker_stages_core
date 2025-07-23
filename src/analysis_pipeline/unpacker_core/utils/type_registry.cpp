#include "analysis_pipeline/unpacker_core/utils/type_registry.h"
#include <spdlog/spdlog.h>

TypeRegistry& TypeRegistry::Instance() {
    static TypeRegistry instance;
    return instance;
}

TypeRegistry::TypeRegistry() {
    uint16_t test = 0x1;
    bool system_little_endian = (*reinterpret_cast<uint8_t*>(&test) == 0x1);

#define REGISTER_TYPE(TYPE_NAME, CPP_TYPE) \
    handlers_[TYPE_NAME] = [system_little_endian](const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, TObject* obj, TDataMember* member) -> bool { \
        CPP_TYPE val; \
        if (!ReadValue<CPP_TYPE>(buffer, buffer_size, offset, little_endian, val)) return false; \
        return AssignValue<CPP_TYPE>(obj, member, val); \
    };

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

TypeRegistry::HandlerFunc TypeRegistry::GetHandler(const std::string& type_name) const {
    auto it = handlers_.find(type_name);
    if (it == handlers_.end()) return nullptr;
    return it->second;
}

void TypeRegistry::RegisterHandler(const std::string& type_name, HandlerFunc handler) {
    handlers_[type_name] = std::move(handler);
}
