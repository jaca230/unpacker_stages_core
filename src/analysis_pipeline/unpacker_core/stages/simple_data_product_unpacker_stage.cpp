#include "analysis_pipeline/unpacker_core/stages/simple_data_product_unpacker_stage.h"

#include <TParameter.h>
#include <TClass.h>
#include <TObject.h>
#include <spdlog/spdlog.h>

ClassImp(SimpleDataProductUnpackerStage)

SimpleDataProductUnpackerStage::SimpleDataProductUnpackerStage() = default;

void SimpleDataProductUnpackerStage::OnInit() {
    ByteStreamProcessorStage::OnInit();

    root_class_name_ = parameters_.value("root_class_name", "");
    if (root_class_name_.empty()) {
        spdlog::error("[{}] Missing required parameter: root_class_name", Name());
        throw std::runtime_error("Missing root_class_name parameter");
    }

    default_endianness_ = parameters_.value("default_endianness", "little");
    data_product_name_ = parameters_.value("data_product_name", "simple_data_product");

    cls_ = TClass::GetClass(root_class_name_.c_str());
    if (!cls_) {
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
    }
}

void SimpleDataProductUnpackerStage::Process() {
    if (!parser_ || !cls_) {
        spdlog::error("[{}] Parser or class pointer not initialized, aborting Process()", Name());
        return;
    }

    auto lock = getInputByteStreamLock();
    if (!lock) {
        spdlog::error("[{}] Failed to acquire ByteStream lock '{}'", Name(), input_byte_stream_product_name_);
        return;
    }

    std::shared_ptr<TObject> base_ptr = lock->getSharedObject();
    if (!base_ptr) {
        spdlog::warn("[{}] ByteStream product has null shared object", Name());
        return;
    }

    auto byte_stream_ptr = std::dynamic_pointer_cast<dataProducts::ByteStream>(base_ptr);
    if (!byte_stream_ptr || !byte_stream_ptr->data || byte_stream_ptr->size == 0) {
        spdlog::warn("[{}] Invalid or empty ByteStream object", Name());
        return;
    }

    const uint8_t* buffer = byte_stream_ptr->data;
    size_t buffer_size = byte_stream_ptr->size;

    int last_index = getLastReadIndex();
    size_t start_offset = (last_index < 0) ? 0 : static_cast<size_t>(last_index);

    TObject* obj = static_cast<TObject*>(cls_->New());
    if (!obj) {
        spdlog::error("[{}] Failed to instantiate ROOT object of class '{}'", Name(), root_class_name_);
        return;
    }

    if (!parser_->Parse(buffer, buffer_size, start_offset, obj)) {
        spdlog::error("[{}] Failed to parse buffer into ROOT object", Name());
        delete obj;
        return;
    }

    auto pdp = std::make_unique<PipelineDataProduct>();
    pdp->setName(data_product_name_);
    pdp->setObject(std::unique_ptr<TObject>(obj));
    pdp->addTag("simple_data_product");
    pdp->addTag("built_by_simple_data_product_unpacker");

    getDataProductManager()->addOrUpdate(data_product_name_, std::move(pdp));
    setLastReadIndex(static_cast<int>(start_offset + parser_->GetTotalParsedSize()));

    spdlog::info("[{}] Produced data product '{}', updated last read index to {}", Name(), data_product_name_, getLastReadIndex());
}
