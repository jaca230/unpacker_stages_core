#ifndef SIMPLE_DATA_PRODUCT_UNPACKER_STAGE_H
#define SIMPLE_DATA_PRODUCT_UNPACKER_STAGE_H

#include "analysis_pipeline/unpacker_core/stages/byte_stream_processor_stage.h"
#include "analysis_pipeline/unpacker_core/utils/reflection_based_parser.h"

#include <memory>
#include <string>

class SimpleDataProductUnpackerStage : public ByteStreamProcessorStage {
public:
    SimpleDataProductUnpackerStage();
    ~SimpleDataProductUnpackerStage() override = default;

    void OnInit() override;
    void Process() override;

    std::string Name() const override { return "SimpleDataProductUnpackerStage"; }

private:
    std::string root_class_name_;
    std::string default_endianness_;
    std::string data_product_name_;

    std::unique_ptr<ReflectionBasedParser> parser_;
    TClass* cls_ = nullptr;  // cached ROOT class pointer

    ClassDefOverride(SimpleDataProductUnpackerStage, 1);
};

#endif  // SIMPLE_DATA_PRODUCT_UNPACKER_STAGE_H
