#ifndef BYTE_STREAM_TO_DATA_PRODUCT_STAGE_H
#define BYTE_STREAM_TO_DATA_PRODUCT_STAGE_H

#include "analysis_pipeline/unpacker_core/stages/byte_stream_processor_stage.h"
#include "analysis_pipeline/unpacker_core/utils/field_mapping_parser.h"

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

class ByteStreamToDataProductStage : public ByteStreamProcessorStage {
public:
    ByteStreamToDataProductStage();
    ~ByteStreamToDataProductStage() override = default;

    void OnInit() override;
    void Process() override;

    std::string Name() const override { return "ByteStreamToDataProductStage"; }

private:
    std::string root_class_name_;
    std::string data_product_name_;

    nlohmann::json field_mappings_json_;

    FieldMappingParser parser_;
    TClass* root_class_ = nullptr;
    size_t total_parsed_size_ = 0;

    ClassDefOverride(ByteStreamToDataProductStage, 1);
};

#endif  // BYTE_STREAM_TO_DATA_PRODUCT_STAGE_H
