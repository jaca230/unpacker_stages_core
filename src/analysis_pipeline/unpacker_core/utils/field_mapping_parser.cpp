#include "analysis_pipeline/unpacker_core/utils/field_mapping_parser.h"
#include "analysis_pipeline/unpacker_core/utils/type_registry.h"
#include <spdlog/spdlog.h>

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

    if (!field_info.contains("offset") || !field_info.contains("size") || !field_info.contains("endianness")) {
        spdlog::error("FieldMappingParser: Missing offset/size/endianness for field '{}'", member_name);
        return false;
    }

    int64_t offset = field_info["offset"].get<int64_t>();
    size_t size = field_info["size"].get<size_t>();
    std::string endian_str = field_info["endianness"].get<std::string>();
    bool little_endian = (endian_str == "little");

    // Compute absolute offset
    size_t abs_offset;
    if (offset >= 0) {
        abs_offset = start_offset + static_cast<size_t>(offset);
    } else {
        if (static_cast<size_t>(-offset) > buffer_size) {
            spdlog::error("FieldMappingParser: Negative offset {} out of range for field '{}'", offset, member_name);
            return false;
        }
        abs_offset = buffer_size + offset; // offset negative
    }

    if (abs_offset + size > buffer_size) {
        spdlog::error("FieldMappingParser: Field '{}' (offset {}, size {}) out of buffer bounds (size {})",
                      member_name, abs_offset, size, buffer_size);
        return false;
    }

    const std::string& type_name = member->GetTypeName();

    // Get handler from TypeRegistry
    auto handler = TypeRegistry::Instance().GetHandler(type_name);
    if (!handler) {
        spdlog::error("FieldMappingParser: No handler registered for type '{}' (field '{}')", type_name, member_name);
        return false;
    }

    // Invoke handler
    if (!handler(buffer, buffer_size, abs_offset, little_endian, obj, member)) {
        spdlog::error("FieldMappingParser: Handler failed for field '{}' of type '{}'", member_name, type_name);
        return false;
    }

    return true;
}
