#include "analysis_pipeline/unpacker_core/stages/byte_stream_to_reflected_data_product_stage.h"

#include <TParameter.h>
#include <TClass.h>
#include <TObject.h>
#include <spdlog/spdlog.h>

ClassImp(ByteStreamToReflectedDataProductStage)

ByteStreamToReflectedDataProductStage::ByteStreamToReflectedDataProductStage() = default;

void ByteStreamToReflectedDataProductStage::OnInit() {
    ByteStreamProcessorStage::OnInit();

    root_class_name_ = parameters_.value("root_class_name", "");
    if (root_class_name_.empty()) {
        spdlog::error("[{}] Missing required parameter: root_class_name", Name());
        throw std::runtime_error("Missing root_class_name parameter");
    }

    default_endianness_ = parameters_.value("default_endianness", "little");
    data_product_name_ = parameters_.value("data_product_name", "reflected_data_product");

    root_class_ = TClass::GetClass(root_class_name_.c_str());
    if (!root_class_) {
        spdlog::error("[{}] ROOT class '{}' not found during OnInit", Name(), root_class_name_);
        throw std::runtime_error("ROOT class not found: " + root_class_name_);
    }

    try {
        parser_ = std::make_unique<ReflectionBasedParser>(root_class_name_, default_endianness_);
        spdlog::info("[{}] Initialized ReflectionBasedParser for class '{}', endian '{}'",
                     Name(), root_class_name_, default_endianness_);
    } catch (const std::exception& e) {
        spdlog::error("[{}] Failed to initialize ReflectionBasedParser: {}", Name(), e.what());
        parser_.reset();
        throw;
    }
}

void ByteStreamToReflectedDataProductStage::Process() {
    if (!parser_ || !root_class_) {
        spdlog::error("[{}] Parser or ROOT class pointer not initialized, aborting Process()", Name());
        return;
    }

    auto input_lock = getInputByteStreamLock();
    if (!input_lock) {
        spdlog::error("[{}] Failed to acquire ByteStream lock '{}'", Name(), input_byte_stream_product_name_);
        return;
    }

    std::shared_ptr<TObject> base_obj = input_lock->getSharedObject();
    if (!base_obj) {
        spdlog::warn("[{}] ByteStream product has null shared object", Name());
        return;
    }

    auto byte_stream_ptr = std::dynamic_pointer_cast<dataProducts::ByteStream>(base_obj);
    if (!byte_stream_ptr || !byte_stream_ptr->data || byte_stream_ptr->size == 0) {
        spdlog::warn("[{}] Invalid or empty ByteStream object", Name());
        return;
    }

    const uint8_t* buffer = byte_stream_ptr->data;
    size_t buffer_size = byte_stream_ptr->size;

    int last_read_index = getLastReadIndex();
    size_t parse_offset = (last_read_index < 0) ? 0 : static_cast<size_t>(last_read_index);

    TObject* product_obj = static_cast<TObject*>(root_class_->New());
    if (!product_obj) {
        spdlog::error("[{}] Failed to instantiate ROOT object of class '{}'", Name(), root_class_name_);
        return;
    }

    bool parse_success = parser_->Parse(buffer, buffer_size, parse_offset, product_obj);
    if (!parse_success) {
        spdlog::error("[{}] Failed to parse buffer into ROOT object", Name());
        delete product_obj;
        return;
    }

    auto data_product = std::make_unique<PipelineDataProduct>();
    data_product->setName(data_product_name_);
    data_product->setObject(std::unique_ptr<TObject>(product_obj));
    data_product->addTag("reflected_data_product");
    data_product->addTag("built_by_byte_stream_to_reflected_data_product_stage");

    getDataProductManager()->addOrUpdate(data_product_name_, std::move(data_product));
    setLastReadIndex(static_cast<int>(parse_offset + parser_->GetTotalParsedSize()));

    spdlog::debug("[{}] Produced data product '{}', updated last read index to {}", Name(), data_product_name_, getLastReadIndex());
}
