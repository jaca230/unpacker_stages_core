#include "analysis_pipeline/unpacker_core/stages/byte_stream_processor_repeating_sequence_stage.h"
#include <spdlog/spdlog.h>
#include <TParameter.h>
#include <TClass.h>
#include <TObject.h>

ClassImp(ByteStreamProcessorRepeatingSequenceStage)

void ByteStreamProcessorRepeatingSequenceStage::OnInit() {
    ByteStreamProcessorStage::OnInit(); // Sets input stream + index names

    repeat_count_product_name_ = parameters_.value("repeat_count_product_name", "");
    repeat_count_product_member_ = parameters_.value("repeat_count_product_member", "");

    spdlog::debug("[{}] repeat_count_product_name = '{}', repeat_count_product_member = '{}'",
                Name(), repeat_count_product_name_, repeat_count_product_member_);

    local_config_ = std::make_shared<ConfigManager>();
    if (parameters_.contains("pipeline_config")) {
        if (!local_config_->addJsonObject(parameters_["pipeline_config"])) {
            throw std::runtime_error("Failed to load inline pipeline_config");
        }
    } else if (parameters_.contains("pipeline_config_file")) {
        std::string path = parameters_["pipeline_config_file"];
        if (!local_config_->loadFiles({path})) {
            throw std::runtime_error("Failed to load pipeline_config_file: " + path);
        }
    } else {
        throw std::runtime_error("No pipeline config provided");
    }

    if (!local_config_->validate()) {
        throw std::runtime_error("Invalid internal config");
    }

    local_pipeline_ = std::make_unique<Pipeline>(local_config_);
    if (!local_pipeline_->buildFromConfig()) {
        throw std::runtime_error("Internal pipeline build failed");
    }
}


void ByteStreamProcessorRepeatingSequenceStage::Process() {
    //Step 1. Get the byte stream product
    auto byte_stream_lock = getInputByteStreamLock();
    if (!byte_stream_lock) {
        spdlog::error("[{}] Failed to acquire ByteStream lock '{}'", Name(), input_byte_stream_product_name_);
        return;
    }

    std::shared_ptr<TObject> base_obj = byte_stream_lock->getSharedObject();
    if (!base_obj) {
        spdlog::warn("[{}] ByteStream product has null shared object", Name());
        return;
    }

    auto byte_stream_ptr = std::dynamic_pointer_cast<dataProducts::ByteStream>(base_obj);
    if (!byte_stream_ptr || !byte_stream_ptr->data || byte_stream_ptr->size == 0) {
        spdlog::warn("[{}] Invalid or empty ByteStream object", Name());
        return;
    }

    // Step 2. Add the byte stream to the local/internal pipeline
    auto internal_byte_stream_dp = std::make_unique<PipelineDataProduct>();
    internal_byte_stream_dp->setName(input_byte_stream_product_name_);
    internal_byte_stream_dp->setSharedObject(byte_stream_ptr);
    local_pipeline_->getDataProductManager().addOrUpdate(input_byte_stream_product_name_, std::move(internal_byte_stream_dp));

    //Step 3. Get the last read index
    int last_index = getLastReadIndex();

    //Step 4. Add the last read index to the interal/local pipeline
    auto param = std::make_unique<TParameter<int>>(last_index_product_name_.c_str(), last_index);
    auto internal_last_read_index_dp = std::make_unique<PipelineDataProduct>();
    internal_last_read_index_dp->setName(last_index_product_name_);
    internal_last_read_index_dp->setObject(std::move(param));
    internal_last_read_index_dp->addTag("internal");
    internal_last_read_index_dp->addTag("byte_stream_index");
    internal_last_read_index_dp->addTag("built_by_byte_stream_processor_repeating_sequence_stage");
    local_pipeline_->getDataProductManager().addOrUpdate(last_index_product_name_, std::move(internal_last_read_index_dp));

    // Step 5. Figure out how many times to repeat based on config.
    int repeat_count = 0;

    if (!repeat_count_product_name_.empty() && !repeat_count_product_member_.empty()) {
        //Checkout the data product with the member variable we're using for repeat count
        auto repeat_handle = getDataProductManager()->checkoutRead(repeat_count_product_name_);
        if (!repeat_handle.get()) {
            spdlog::error("[{}] Failed to lock repeat count product '{}'", Name(), repeat_count_product_name_);
            return;
        }

        // Get the member pointer and assign it (if it's type is good)
        auto [member_ptr, member_type] = repeat_handle->getMemberPointerAndType(repeat_count_product_member_);
        if (!member_ptr) {
            spdlog::error("[{}] Member '{}' not found in product '{}'", Name(), repeat_count_product_member_, repeat_count_product_name_);
            return;
        }

        //This is pretty heinous, but it's the quickest way to handle the various types we might have
        if (member_type == "int" || member_type == "Int_t") {
            repeat_count = *static_cast<const int*>(member_ptr);
        } else if (member_type == "unsigned int" || member_type == "UInt_t" || member_type == "uint" || member_type == "uint32_t") {
            repeat_count = static_cast<int>(*static_cast<const unsigned int*>(member_ptr));
        } else if (member_type == "short" || member_type == "Short_t") {
            repeat_count = static_cast<int>(*static_cast<const short*>(member_ptr));
        } else if (member_type == "unsigned short" || member_type == "UShort_t" || member_type == "uint16_t") {
            repeat_count = static_cast<int>(*static_cast<const unsigned short*>(member_ptr));
        } else if (member_type == "char" || member_type == "Char_t" || member_type == "int8_t") {
            repeat_count = static_cast<int>(*static_cast<const char*>(member_ptr));
        } else if (member_type == "unsigned char" || member_type == "UChar_t" || member_type == "uint8_t") {
            repeat_count = static_cast<int>(*static_cast<const unsigned char*>(member_ptr));
        } else if (member_type == "long" || member_type == "Long_t" || member_type == "int64_t") {
            repeat_count = static_cast<int>(*static_cast<const long*>(member_ptr));
        } else if (member_type == "unsigned long" || member_type == "ULong_t" || member_type == "uint64_t") {
            repeat_count = static_cast<int>(*static_cast<const unsigned long*>(member_ptr));
        } else if (member_type == "Long64_t") {
            repeat_count = static_cast<int>(*static_cast<const Long64_t*>(member_ptr));
        } else if (member_type == "ULong64_t") {
            repeat_count = static_cast<int>(*static_cast<const ULong64_t*>(member_ptr));
        } else if (member_type == "float" || member_type == "Float_t") {
            repeat_count = static_cast<int>(*static_cast<const float*>(member_ptr));
        } else if (member_type == "double" || member_type == "Double_t") {
            repeat_count = static_cast<int>(*static_cast<const double*>(member_ptr));
        } else if (member_type == "bool" || member_type == "Bool_t") {
            repeat_count = static_cast<int>(*static_cast<const bool*>(member_ptr));
        } else {
            spdlog::error("[{}] Unsupported member type '{}' for repeat count", Name(), member_type);
            return;
        }

        spdlog::debug("[{}] Resolved repeat_count = {} from '{}.{}'", 
                      Name(), repeat_count, repeat_count_product_name_, repeat_count_product_member_);
    } else {
        spdlog::warn("[{}] No repeat count config provided; defaulting to repeat_count = 0", Name());
    }

    if (repeat_count <= 0) {
        spdlog::warn("[{}] repeat_count = {} <= 0, skipping execution loop", Name(), repeat_count);
        return;
    }

    // Step 6. Execute the local pipeline repeat_count times
    std::vector<std::pair<std::string, std::unique_ptr<PipelineDataProduct>>> collected_products;
    for (int i = 0; i < repeat_count; ++i) {
        local_pipeline_->execute();

        auto& dp_manager = local_pipeline_->getDataProductManager();
        
        // Extract all the data products made this iteration (Except the ones we need each iteration)
        for (const auto& name : dp_manager.getAllNames()) {
            if (name == last_index_product_name_) continue; //Skip, we keep the last index
            else if (name == input_byte_stream_product_name_) continue; //Skip, we keep the input byte stream

            auto extracted = dp_manager.extractProduct(name);
            if (!extracted) {
                spdlog::error("[{}] Failed to extract '{}'", Name(), name);
                continue;
            }
            
            // Rename the product to include the iteration index
            extracted->setName(name + "_" + std::to_string(i));
            collected_products.emplace_back(extracted->getName(), std::move(extracted));
        }
    }

    // Step 7. Push collected outputs to parent pipeline
    getDataProductManager()->addOrUpdateMultiple(std::move(collected_products));

    // STep 8. Extract the last read index from the internal pipeline and update the main pipeline
    auto internal_last_read_index_dp_lock = local_pipeline_->getDataProductManager().checkoutRead(last_index_product_name_);
    if (!internal_last_read_index_dp_lock) {
        spdlog::warn("[{}] Failed to lock internal last index product '{}'", Name(), last_index_product_name_);
        return;
    }

    TObject* obj = internal_last_read_index_dp_lock->getObject();
    if (!obj) {
        spdlog::warn("[{}] Last index product '{}' has null object", Name(), last_index_product_name_);
        return;
    }

    auto* index_param = dynamic_cast<TParameter<int>*>(obj);
    if (!index_param) {
        spdlog::warn("[{}] Product '{}' is not a TParameter<int>", Name(), last_index_product_name_);
        return;
    }

    setLastReadIndex(index_param->GetVal());
}
