#ifndef ANALYSIS_PIPELINE_UNPACKER_CORE_STAGES_BYTE_STREAM_PROCESSOR_STAGE_H
#define ANALYSIS_PIPELINE_UNPACKER_CORE_STAGES_BYTE_STREAM_PROCESSOR_STAGE_H

#include "analysis_pipeline/core/stages/base_stage.h"
#include "analysis_pipeline/unpacker_core/data_products/ByteStream.h"
#include "analysis_pipeline/core/data/pipeline_data_product_read_lock.h"
#include "analysis_pipeline/unpacker_core/utils/field_mapping_parser.h" 

#include <memory>
#include <string>

class ByteStreamProcessorStage : public BaseStage {
public:
    ByteStreamProcessorStage();
    ~ByteStreamProcessorStage() override = default;

    void OnInit() override;
    void Process() override;

    std::string Name() const override { return "ByteStreamProcessorStage"; }

protected:
    int getLastReadIndex() const;
    void setLastReadIndex(int index);

    /// Returns a lock object holding a read lock on the ByteStream product.
    /// If product does not exist or lock fails, returns invalid PipelineDataProductLock.
    PipelineDataProductReadLock getInputByteStreamLock() const;

    std::unique_ptr<TObject> parseObjectFromBytes(
        TObject* obj,
        const uint8_t* data,
        size_t data_size,
        const nlohmann::json& field_mapping_json,
        size_t start_offset);

    std::string last_index_product_name_;
    std::string input_byte_stream_product_name_;

private:
    void createLastReadIndexProduct(int index);

    FieldMappingParser field_mapping_parser_;  // internal state

    ClassDefOverride(ByteStreamProcessorStage, 1);
};

#endif // ANALYSIS_PIPELINE_UNPACKER_CORE_STAGES_BYTE_STREAM_PROCESSOR_STAGE_H
