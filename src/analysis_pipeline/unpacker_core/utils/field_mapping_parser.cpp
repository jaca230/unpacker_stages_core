#include "analysis_pipeline/unpacker_core/utils/field_mapping_parser.h"
#include <spdlog/spdlog.h>

FieldMappingParser::FieldMappingParser() {
    uint16_t test = 0x1;
    system_little_endian_ = (*reinterpret_cast<uint8_t*>(&test) == 0x1);
}


bool FieldMappingParser::ParseAndFill(const uint8_t* buffer,
                                      size_t buffer_size,
                                      size_t start_offset,
                                      const nlohmann::json& json_field_mapping,
                                      TObject* obj) {
    if (!buffer || !obj) {
        spdlog::error("FieldMappingParser: Null buffer or object pointer");
        return false;
    }

    TClass* cls = obj->IsA();
    if (!cls) {
        spdlog::error("FieldMappingParser: TObject has no TClass");
        return false;
    }

    // Debug print entire JSON mapping, indented for readability
    spdlog::debug("FieldMappingParser: Field mapping JSON:\n{}", json_field_mapping.dump(4));

    for (auto it = json_field_mapping.begin(); it != json_field_mapping.end(); ++it) {
        const std::string& member_name = it.key();
        const nlohmann::json& field_info = it.value();

        spdlog::debug("Parsing field '{}': {}", member_name, field_info.dump());

        if (!ExtractAndAssignField(buffer, buffer_size, start_offset, member_name, field_info, obj)) {
            spdlog::error("FieldMappingParser: Failed to extract/assign field '{}'", member_name);
            return false;
        }
    }

    return true;
}

bool FieldMappingParser::ExtractAndAssignField(const uint8_t* buffer,
                                               size_t buffer_size,
                                               size_t start_offset,
                                               const std::string& member_name,
                                               const nlohmann::json& field_info,
                                               TObject* obj) {
    TClass* cls = obj->IsA();
    if (!cls) return false;

    TDataMember* member = cls->GetDataMember(member_name.c_str());
    if (!member) {
        spdlog::error("FieldMappingParser: Member '{}' not found in class '{}'", member_name, cls->GetName());
        return false;
    }

    // Extract offset, size, and endianness from JSON
    if (!field_info.contains("offset") || !field_info.contains("size") || !field_info.contains("endianness")) {
        spdlog::error("FieldMappingParser: Missing offset/size/endianness for field '{}'", member_name);
        return false;
    }

    int64_t offset = field_info["offset"].get<int64_t>();
    size_t size = field_info["size"].get<size_t>();
    std::string endian_str = field_info["endianness"].get<std::string>();
    bool little_endian = (endian_str == "little");

    // Compute absolute buffer offset
    size_t abs_offset = 0;
    if (offset >= 0) {
        abs_offset = start_offset + static_cast<size_t>(offset);
    } else {
        // Negative offset means relative from buffer end
        if (static_cast<size_t>(-offset) > buffer_size) {
            spdlog::error("FieldMappingParser: Negative offset {} out of range for field '{}'", offset, member_name);
            return false;
        }
        abs_offset = buffer_size + offset; // offset is negative
    }

    // Bounds check
    if (abs_offset + size > buffer_size) {
        spdlog::error("FieldMappingParser: Field '{}' (offset {}, size {}) out of buffer bounds (size {})",
                      member_name, abs_offset, size, buffer_size);
        return false;
    }

    const uint8_t* data_ptr = buffer + abs_offset;

    std::string type_name = member->GetTypeName();

    // Dispatch on known types - expand as needed
    if (type_name == "unsigned char" || type_name == "UChar_t" || type_name == "uint8_t") {
        uint16_t val;
        if (!ReadValue<uint16_t>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<uint16_t>(obj, member, val);
    }
    else if (type_name == "unsigned short" || type_name == "UInt_t" || type_name == "uint16_t") {
        uint16_t val;
        if (!ReadValue<uint16_t>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<uint16_t>(obj, member, val);
    }
    else if (type_name == "unsigned int" || type_name == "UInt32_t" || type_name == "uint32_t") {
        uint32_t val;
        if (!ReadValue<uint32_t>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<uint32_t>(obj, member, val);
    }
    else if (type_name == "unsigned long" || type_name == "UInt64_t" || type_name == "uint64_t") {
        uint64_t val;
        if (!ReadValue<uint64_t>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<uint64_t>(obj, member, val);
    }
    else if (type_name == "short" || type_name == "Int16_t" || type_name == "int16_t") {
        int16_t val;
        if (!ReadValue<int16_t>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<int16_t>(obj, member, val);
    }
    else if (type_name == "int" || type_name == "Int_t" || type_name == "int32_t") {
        int32_t val;
        if (!ReadValue<int32_t>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<int32_t>(obj, member, val);
    }
    else if (type_name == "long" || type_name == "Int64_t" || type_name == "int64_t") {
        int64_t val;
        if (!ReadValue<int64_t>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<int64_t>(obj, member, val);
    }
    else if (type_name == "float" || type_name == "Float_t") {
        float val;
        if (!ReadValue<float>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<float>(obj, member, val);
    }
    else if (type_name == "double" || type_name == "Double_t") {
        double val;
        if (!ReadValue<double>(buffer, buffer_size, abs_offset, little_endian, val)) return false;
        return AssignValueToMember<double>(obj, member, val);
    }
    else {
        spdlog::error("FieldMappingParser: Unsupported member type '{}' for field '{}'", type_name, member_name);
        return false;
    }
}

template<typename T>
bool FieldMappingParser::ReadValue(const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, T& out_value) const {
    if (offset + sizeof(T) > buffer_size) {
        spdlog::error("FieldMappingParser: Buffer overrun while reading {} bytes at offset {}", sizeof(T), offset);
        return false;
    }

    std::memcpy(&out_value, buffer + offset, sizeof(T));

    if (little_endian != system_little_endian_) {
        SwapBytes(&out_value, sizeof(T));
    }

    return true;
}


template<typename T>
bool FieldMappingParser::AssignValueToMember(TObject* obj, TDataMember* member, const T& value) const {
    // Member offset within object
    char* base = reinterpret_cast<char*>(obj);
    char* member_ptr = base + member->GetOffset();

    // Copy value into member storage
    std::memcpy(member_ptr, &value, sizeof(T));
    return true;
}

void FieldMappingParser::SwapBytes(void* data, size_t size) const {
    uint8_t* bytes = static_cast<uint8_t*>(data);
    for (size_t i = 0; i < size/2; ++i) {
        std::swap(bytes[i], bytes[size - 1 - i]);
    }
}
