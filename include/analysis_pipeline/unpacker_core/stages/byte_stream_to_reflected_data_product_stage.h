#ifndef BYTE_STREAM_TO_REFLECTED_DATA_PRODUCT_STAGE_H
#define BYTE_STREAM_TO_REFLECTED_DATA_PRODUCT_STAGE_H

#include "analysis_pipeline/unpacker_core/stages/byte_stream_processor_stage.h"
#include "analysis_pipeline/unpacker_core/utils/reflection_based_parser.h"

#include <memory>
#include <string>

class ByteStreamToReflectedDataProductStage : public ByteStreamProcessorStage {
public:
    ByteStreamToReflectedDataProductStage();
    ~ByteStreamToReflectedDataProductStage() override = default;

    void OnInit() override;
    void Process() override;

    std::string Name() const override { return "ByteStreamToReflectedDataProductStage"; }

private:
    std::string root_class_name_;
    std::string default_endianness_;
    std::string data_product_name_;

    std::unique_ptr<ReflectionBasedParser> parser_;
    TClass* root_class_ = nullptr;  // Cached ROOT class pointer

    ClassDefOverride(ByteStreamToReflectedDataProductStage, 1);
};

#endif  // BYTE_STREAM_TO_REFLECTED_DATA_PRODUCT_STAGE_H
