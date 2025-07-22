#ifndef FIELD_MAPPING_PARSER_H
#define FIELD_MAPPING_PARSER_H

#include <TObject.h>
#include <TClass.h>
#include <TDataMember.h>
#include <TRealData.h>
#include <TList.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstring>
#include <string>

class FieldMappingParser {
public:
    FieldMappingParser();
    ~FieldMappingParser() = default;

    bool ParseAndFill(const uint8_t* buffer,
                      size_t buffer_size,
                      size_t start_offset,
                      const nlohmann::json& json_field_mapping,
                      TObject* obj);

private:
    bool system_little_endian_;

    template<typename T>
    bool ReadValue(const uint8_t* buffer, size_t buffer_size, size_t offset, bool little_endian, T& out_value) const;

    template<typename T>
    bool AssignValueToMember(TObject* obj, TDataMember* member, const T& value) const;

    bool ExtractAndAssignField(const uint8_t* buffer,
                               size_t buffer_size,
                               size_t start_offset,
                               const std::string& member_name,
                               const nlohmann::json& field_info,
                               TObject* obj);

    void SwapBytes(void* data, size_t size) const;
};


#endif // FIELD_MAPPING_PARSER_H
