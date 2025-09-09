#include "analysis_pipeline/unpacker_core/stages/byte_stream_processor_stage.h"
#include "analysis_pipeline/core/data/pipeline_data_product_manager.h"
#include "analysis_pipeline/core/data/pipeline_data_product.h"
#include "analysis_pipeline/unpacker_core/data_products/ByteStream.h"
#include "analysis_pipeline/unpacker_core/utils/field_mapping_parser.h"

#include <TParameter.h>
#include <TClass.h>
#include <TObject.h>
#include <spdlog/spdlog.h>

ClassImp(ByteStreamProcessorStage)

ByteStreamProcessorStage::ByteStreamProcessorStage() = default;

void ByteStreamProcessorStage::OnInit() {
    last_index_product_name_ = parameters_.value("last_index_key", "last_processed_packet_index");
    input_byte_stream_product_name_ = parameters_.value("input_byte_stream_product_name", "bytestream_bank_DATA");

    spdlog::debug("[{}] Using last_index_key='{}', input_byte_stream_product_name='{}'",
                  Name(), last_index_product_name_, input_byte_stream_product_name_);
}

void ByteStreamProcessorStage::Process() {
    spdlog::warn("[{}] Base class Process() called. Override this method in your derived stage.", Name());
}

int ByteStreamProcessorStage::getLastReadIndex() const {
    if (!getDataProductManager()->hasProduct(last_index_product_name_)) {
        return 0;
    }

    auto lock = getDataProductManager()->checkoutRead(last_index_product_name_);
    auto* param = dynamic_cast<TParameter<int>*>(lock.get()->getObject());
    if (param) {
        return param->GetVal();
    }

    spdlog::warn("[{}] Product '{}' exists but is not a TParameter<int>", Name(), last_index_product_name_);
    return 0;
}


void ByteStreamProcessorStage::setLastReadIndex(int index) {

    if (!getDataProductManager()->hasProduct(last_index_product_name_)) {
        createLastReadIndexProduct(index);
        return;
    }

    auto lock = getDataProductManager()->checkoutWrite(last_index_product_name_);
    auto* param = dynamic_cast<TParameter<int>*>(lock.get()->getObject());
    if (param) {
        param->SetVal(index);
    } else {
        spdlog::warn("[{}] Product '{}' exists but is not a TParameter<int>", Name(), last_index_product_name_);
    }
}


void ByteStreamProcessorStage::createLastReadIndexProduct(int index) {
    auto param = std::make_unique<TParameter<int>>(last_index_product_name_.c_str(), index);
    auto product = std::make_unique<PipelineDataProduct>();
    product->setName(last_index_product_name_);
    product->setObject(std::move(param));
    product->addTag("internal");
    product->addTag("byte_stream_index");
    product->addTag("built_by_bytestream_processor_stage");
    getDataProductManager()->addOrUpdate(last_index_product_name_, std::move(product));
}

PipelineDataProductReadLock ByteStreamProcessorStage::getInputByteStreamLock() const {
    if (!getDataProductManager()->hasProduct(input_byte_stream_product_name_)) {
        spdlog::debug("[{}] Input ByteStream product '{}' not found", Name(), input_byte_stream_product_name_);
        return PipelineDataProductLock();
    }

    try {
        return getDataProductManager()->checkoutRead(input_byte_stream_product_name_);
    } catch (const std::exception& e) {
        spdlog::error("[{}] Failed to checkout ByteStream product '{}': {}", Name(), input_byte_stream_product_name_, e.what());
        return PipelineDataProductLock();
    }
}

std::unique_ptr<TObject> ByteStreamProcessorStage::parseObjectFromBytes(
    TObject* obj,
    const uint8_t* data,
    size_t data_size,
    const nlohmann::json& field_mapping_json,
    size_t start_offset)
{
    if (!data) {
        spdlog::error("[{}] Null data pointer passed to parseObjectFromBytes", Name());
        return nullptr;
    }

    if (!obj) {
        spdlog::error("[{}] Null TObject pointer passed to parseObjectFromBytes", Name());
        return nullptr;
    }

    if (start_offset >= data_size) {
        spdlog::error("[{}] Start offset {} >= data size {}", Name(), start_offset, data_size);
        return nullptr;
    }

    bool success = field_mapping_parser_.ParseAndFill(data, data_size, start_offset, field_mapping_json, obj);
    if (!success) {
        spdlog::error("[{}] FieldMappingParser failed to fill object '{}'", Name(), obj->ClassName());
        delete obj;
        return nullptr;
    }

    return std::unique_ptr<TObject>(obj);
}


