#include "analysis_pipeline/unpacker_core/stages/byte_stream_processor_stage.h"
#include "analysis_pipeline/config/config_manager.h"
#include "analysis_pipeline/pipeline/pipeline.h"

class ByteStreamProcessorRepeatingSequenceStage : public ByteStreamProcessorStage {
public:
    ByteStreamProcessorRepeatingSequenceStage();
    ~ByteStreamProcessorRepeatingSequenceStage() override = default;

    void OnInit() override;
    void Process() override;

    std::string Name() const override { return "ByteStreamProcessorRepeatingSequenceStage"; }

private:
    std::unique_ptr<Pipeline> local_pipeline_;
    std::shared_ptr<ConfigManager> local_config_;
    std::string repeat_count_product_name_;
    std::string repeat_count_product_member_;

    ClassDefOverride(ByteStreamProcessorRepeatingSequenceStage, 1);
};
