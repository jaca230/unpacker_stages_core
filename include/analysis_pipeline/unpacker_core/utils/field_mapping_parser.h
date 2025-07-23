#ifndef FIELD_MAPPING_PARSER_H
#define FIELD_MAPPING_PARSER_H

#include <TObject.h>
#include <TClass.h>
#include <TDataMember.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

class FieldMappingParser {
public:
    FieldMappingParser() = default;
    ~FieldMappingParser() = default;

    // Parse buffer according to json mapping, fill fields of obj accordingly
    bool ParseAndFill(const uint8_t* buffer,
                      size_t buffer_size,
                      size_t start_offset,
                      const nlohmann::json& json_field_mapping,
                      TObject* obj);

private:
    bool ExtractAndAssignField(const uint8_t* buffer,
                               size_t buffer_size,
                               size_t start_offset,
                               const std::string& member_name,
                               const nlohmann::json& field_info,
                               TObject* obj);
};

#endif // FIELD_MAPPING_PARSER_H
