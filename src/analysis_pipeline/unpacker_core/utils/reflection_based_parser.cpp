#include "analysis_pipeline/unpacker_core/utils/reflection_based_parser.h"
#include "analysis_pipeline/unpacker_core/utils/field_mapping_parser.h"

#include <TClass.h>
#include <TRealData.h>
#include <TDataMember.h>
#include <TList.h>
#include <spdlog/spdlog.h>

#include <unordered_set>
#include <stdexcept>
#include <algorithm>

ReflectionBasedParser::ReflectionBasedParser(std::string class_name, std::string default_endianness)
    : class_name_(std::move(class_name)), default_endianness_(std::move(default_endianness)) {
    delegate_ = new FieldMappingParser();
    BuildJsonMappingFromReflection();
}

ReflectionBasedParser::~ReflectionBasedParser() {
    delete delegate_;
}

bool ReflectionBasedParser::Parse(const uint8_t* buffer,
                                  size_t buffer_size,
                                  size_t start_offset,
                                  TObject* obj) const {
    return delegate_->ParseAndFill(buffer, buffer_size, start_offset, json_field_mapping_, obj);
}

void ReflectionBasedParser::BuildJsonMappingFromReflection() {
    TClass* cls = TClass::GetClass(class_name_.c_str());
    if (!cls) {
        throw std::runtime_error("ReflectionBasedParser: Could not resolve class " + class_name_);
    }

    TList* real_data = cls->GetListOfRealData();
    if (!real_data) {
        throw std::runtime_error("ReflectionBasedParser: Class " + class_name_ + " has no real data");
    }

    const std::unordered_set<std::string> skip_fields = {"fBits", "fUniqueID"};
    json_field_mapping_.clear();

    size_t total_parsed_size = 0;

    for (TIter it(real_data); TRealData* rd = static_cast<TRealData*>(it()); ) {
        const std::string name = rd->GetName();
        if (skip_fields.count(name) > 0) continue;

        TDataMember* dm = rd->GetDataMember();
        if (!dm) continue;

        const std::string type = dm->GetTypeName();
        if (type.find("std::") != std::string::npos || type.find("vector") != std::string::npos) {
            spdlog::debug("Skipping STL or vector field '{}'", name);
            continue;
        }

        const ptrdiff_t offset = rd->GetThisOffset();
        const size_t size = dm->GetUnitSize();

        nlohmann::json field_json;
        field_json["offset"] = static_cast<int64_t>(offset);
        field_json["size"] = size;
        field_json["endianness"] = default_endianness_;

        json_field_mapping_[name] = std::move(field_json);
        total_parsed_size += size;
    }

    if (json_field_mapping_.empty()) {
        throw std::runtime_error("ReflectionBasedParser: No fields found for class " + class_name_);
    }

    spdlog::info("ReflectionBasedParser: Constructed mapping for {} fields from class '{}', total parsed size {} bytes",
                 json_field_mapping_.size(), class_name_, total_parsed_size);

    total_parsed_size_ = total_parsed_size;  // store total size in member variable
}

