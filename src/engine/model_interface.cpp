/**
 * @file model_interface.cpp
 * @brief 学习引擎模型接口实现
 */

#include "engine/model_interface.h"
#include "engine/llama_model.h"
#include "common/logging.h"
#include <mutex>

namespace kb {
namespace engine {

// ModelFactory单例实例
class ModelFactoryImpl : public ModelFactory {
public:
    static ModelFactoryImpl& getInstance() {
        static ModelFactoryImpl instance;
        return instance;
    }
    
    ModelInterfacePtr createModel(
        ModelType modelType,
        const std::string& modelPath,
        const ModelParameters& parameters) override {
        
        // 根据模型类型创建不同的模型
        switch (modelType) {
            case ModelType::TEXT_GENERATION:
            case ModelType::TEXT_EMBEDDING:
                // 目前只支持LlamaModel
                LOG_INFO("ModelInterfacePtr 创建LlamaModel");
                return std::make_shared<LlamaModel>();
                
            case ModelType::MULTIMODAL:
                // 目前不支持多模态模型
                LOG_ERROR("不支持多模态模型");
                return nullptr;
                
            default:
                LOG_ERROR("未知的模型类型");
                return nullptr;
        }
    }
    
    std::vector<std::string> getAvailableModels() const override {
        // 委托给LlamaModelFactory
        return LlamaModelFactory::getInstance().getAvailableModels();
    }
    
private:
    ModelFactoryImpl() {
        LOG_INFO("ModelFactory初始化");
    }
    
    ~ModelFactoryImpl() {
        LOG_INFO("ModelFactory销毁");
    }
};

ModelFactory& ModelFactory::getInstance() {
    return ModelFactoryImpl::getInstance();
}

} // namespace engine
} // namespace kb 