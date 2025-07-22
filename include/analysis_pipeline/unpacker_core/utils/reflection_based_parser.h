#ifndef REFLECTION_BASED_PARSER_H
#define REFLECTION_BASED_PARSER_H

#include <string>
#include <nlohmann/json.hpp>
#include <TObject.h>

class FieldMappingParser;

class ReflectionBasedParser {
public:
    ReflectionBasedParser(std::string class_name, std::string default_endianness = "little");
    ~ReflectionBasedParser();

    bool Parse(const uint8_t* buffer,
               size_t buffer_size,
               size_t start_offset,
               TObject* obj) const;

    size_t GetTotalParsedSize() const { return total_parsed_size_; }

private:
    std::string class_name_;
    std::string default_endianness_;
    nlohmann::json json_field_mapping_;
    FieldMappingParser* delegate_;

    size_t total_parsed_size_ = 0;

    void BuildJsonMappingFromReflection();
};

#endif // REFLECTION_BASED_PARSER_H
