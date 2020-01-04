/**
* @file MindInferenceEngine.h
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef MIND_INFERENCE_ENGINE_ENGINE_H_
#define MIND_INFERENCE_ENGINE_ENGINE_H_

#include "hiaiengine/api.h"
#include "hiaiengine/ai_model_manager.h"
#include "hiaiengine/ai_types.h"
#include "hiaiengine/data_type.h"
#include "hiaiengine/engine.h"
#include "hiaiengine/multitype_queue.h"
#include "hiaiengine/data_type_reg.h"
#include "hiaiengine/ai_tensor.h"
#include "Common.h"

#define INPUT_SIZE 1
#define OUTPUT_SIZE 1
using hiai::Engine;

class MindInferenceEngine : public Engine {
public:
    MindInferenceEngine() :
        batchSize_(0),inputQueue_(INPUT_SIZE),imageHandle_(nullptr),aiModelManager_(nullptr), tranData_(nullptr),
        modelName_("ClassifyModel"){}
    ~MindInferenceEngine(){}
    HIAI_StatusT Init(const hiai::AIConfig& config, const  std::vector<hiai::AIModelDescription>& model_desc);
    /**
    * @brief HIAI_DEFINE_PROCESS : override Engine Process logic
    * @[in]: define a input port, a output port
    */
   HIAI_DEFINE_PROCESS(INPUT_SIZE, OUTPUT_SIZE);
private:

    uint32_t batchSize_;
    // Private implementation a member variable, which is used to cache the input queue
    hiai::MultiTypeQueue inputQueue_;
    // 接受预处理引擎发送给过来的数据
    std::shared_ptr<BatchImageParaWithScaleT> imageHandle_;


    // 推理结果的输入Tensor
    std::vector<std::shared_ptr<hiai::IAITensor>> inputDataVec_;
    // 推理结果的输出Tensor
    std::vector<std::shared_ptr<hiai::IAITensor>> outputDataVec_;
    std::shared_ptr<hiai::AIModelManager> aiModelManager_;
    // 发送给后处理引擎的数据
    std::shared_ptr<EngineTransT> tranData_;
    // 线程锁
    std::mutex memoryRecursiveMutex_;
    
    std::vector<uint8_t*> outData_;

    // 记录模型的名字
    std::string modelName_;


    std::vector<std::map<uint8_t *, int>> predictInputData_; // map for storing data from argX, first:buffer, second:buffer's size
    // 获取模型的输入Tensor信息
    std::vector<hiai::TensorDimension> inputTensorVec_;
    // 获取模型的输出Tensor信息
    std::vector<hiai::TensorDimension> outputTensorVec_;
    
    /**
    * @brief: prepare the data buffer for image data
    * @in: buffer: buffer pointer
    * @in: imageCount: total number of received images
    * @in: start: the index of the first image of each batch
    * @in: imageSize: size of each image
    * @return: HIAI_StatusT
    */
    HIAI_StatusT PrepareForInputBuffer0(uint8_t* buffer, const int imageCount, const int start, const int imageSize);
    
    /**
    * @brief: prepare the data buffer for image information
    * @in: buffer: buffer pointer
    * @in: imageCount: total number of received images
    * @in: start: the index of the first image of each batch
    * @in: multiInput1: the second input received from the previous engine
    * @return: HIAI_StatusT
    */
    HIAI_StatusT PrepareForInputBuffer1(uint8_t* buffer, const int imageCount, const int start, std::shared_ptr<hiai::BatchRawDataBuffer> multiInput1);
    
    /**
    * @brief: set the frame ID as -1 to indicate this batch failed
    * @in: start: index of the begin of this batch
    * @in: imageCount: the image count
    */
    void HandleBatchFailure(const int start, const int imageCount);
    
    /**
    * @brief: handle the exceptions when the dataset batch failed
    * @in: errorMsg: the error message
    * @return: HIAI_StatusT
    */
    HIAI_StatusT HandleExceptions(std::string errorMsg);

    /**
    * @brief: clear buffer in outData vector
    */
    void ClearOutData();

    /**
    * @brief: call ai model manager to do the prediction
    * @return: HIAI_StatusT
    */
    HIAI_StatusT Predict();

    /**
    * @brief: set the tranData_ with the result of this model batch
    * @in: start: index of the begin of this batch
    * @return: HIAI_StatusT
    */
    HIAI_StatusT SetOutputStruct(const int start);

    /**
    * @brief: send the result for one batch
    * @return: HIAI_StatusT
    */
    HIAI_StatusT SendResult();
    };

#define MULTI_INPUT_ARG_PUSH(index) \
    std::shared_ptr<hiai::BatchRawDataBuffer> multiInput##index; \
    inputQueue_.PushData(index, arg##index) \

#define MULTI_INPUT_ARG_POP_2 \
    MULTI_INPUT_ARG_PUSH(1); \
do{ \
    if (!inputQueue_.PopAllData(imageHandle_, multiInput1)) \
    { \
        HIAI_ENGINE_LOG("[MindInferenceEngine] fail to PopAllData"); \
        return HIAI_ERROR; \
    } \
}while(0)

#define DEFINE_MULTI_INPUT_ARGS_POP(index) MULTI_INPUT_ARG_POP_##index

#define MULTI_INPUT_ARG(index) \
do{ \
    std::shared_ptr<hiai::AINeuralNetworkBuffer> neuralBuffer##index = \
        std::make_shared<hiai::AINeuralNetworkBuffer>(); \
    neuralBuffer##index->SetBuffer((void*)(inputBuffer##index), \
        (uint32_t)(inputBuffer##index##Size), false); \
    std::shared_ptr<hiai::IAITensor> inputData##index = \
        std::static_pointer_cast<hiai::IAITensor>(neuralBuffer##index); \
    inputDataVec_.push_back(inputData##index); \
}while(0)

#define MULTI_INPUT_ARGS_1 \
    MULTI_INPUT_ARG(0);

#define MULTI_INPUT_ARGS_2 \
    MULTI_INPUT_ARG(0); \
    MULTI_INPUT_ARG(1);

#define DEFINE_MULTI_INPUT_ARGS(index) MULTI_INPUT_ARGS_##index

#endif // MIND_INFERENCE_ENGINE_ENGINE_H_
