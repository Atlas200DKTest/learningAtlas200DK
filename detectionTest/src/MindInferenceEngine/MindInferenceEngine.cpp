/**
* @file MindInferenceEngine.cpp
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include <hiaiengine/log.h>
#include <stdio.h>
#include <hiaiengine/ai_types.h>
#include <vector>
#include <unistd.h>
#include <thread>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include "MindInferenceEngine.h"
#include "hiaiengine/ai_memory.h"

const static int IMAGE_INFO_DATA_NUM = 3;
const static uint32_t INPUT_INDEX_0 = 0;

/**
* @brief: clear buffer in vector
*/
void MindInferenceEngine::ClearOutData() {
    inputDataVec_.clear();
    // release outData_ pre allocate memory
    // 遍历vector中保存的buffer地址
    // c++11新的语法特性
    for (auto buffer : outData_) {
        if (buffer != nullptr) {
            hiai::HIAIMemory::HIAI_DFree(buffer);
            buffer = nullptr;
        }
    }
    outData_.clear();
}

/**
* @brief: init, inherited from hiaiengine lib
*/
HIAI_StatusT MindInferenceEngine::Init(const hiai::AIConfig& config,
   const std::vector<hiai::AIModelDescription>& model_desc) {
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] Start init!");

    aiModelManager_ = std::make_shared<hiai::AIModelManager>();
    if (aiModelManager_ == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call make_shared for AIModelManager.");
        return HIAI_ERROR;
    }

    std::vector<hiai::AIModelDescription> modelDescVec;
    hiai::AIModelDescription modelDesc;
    for (int index = 0; index < config.items_size(); ++index) {
        const ::hiai::AIConfigItem& item = config.items(index);
        if (item.name() == "model_path") {
            std::string modelPath = item.value();
            if (modelPath.empty()) {
                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] The model_path does not exist in graph.config.");
                return HIAI_ERROR;
            }
            modelDesc.set_path(modelPath);
            std::size_t modelNameStartPos = modelPath.find_last_of("/\\");
            std::size_t modelNameEndPos = modelPath.find_last_of(".");
            if (std::string::npos != modelNameStartPos && std::string::npos != modelNameEndPos
                && modelNameEndPos > modelNameStartPos) {
                modelName_ = modelPath.substr(modelNameStartPos + 1, modelNameEndPos - modelNameStartPos - 1);
            }
        } 
        // 模型设置的密码
        else if (item.name() == "passcode") {
            std::string passcode = item.value();
            modelDesc.set_key(passcode);
        }
    }

    modelDesc.set_name(modelName_);
    modelDescVec.push_back(modelDesc);
    hiai::AIStatus ret = aiModelManager_->Init(config, modelDescVec);
    if (hiai::SUCCESS != ret) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call Init for AIModelManager.");
        return HIAI_ERROR;
    }

    // 获取模型的输入输出Tensor信息
    ret = aiModelManager_->GetModelIOTensorDim(modelName_, inputTensorVec_, outputTensorVec_);
    if (hiai::SUCCESS != ret) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call GetModelIOTensorDim for AIModelManager.");
        return HIAI_ERROR;
    }
    // batchSize_ is the n of input [n w h c] or [n c h w]
    // 记录模型一次处理的图片个数
    batchSize_ = inputTensorVec_[INPUT_INDEX_0].n;
    // printf("batchSize_ = %d\n",batchSize_);
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] End init!");
    return HIAI_OK;
}

/**
* @brief: handle the exceptions when the dataset batch failed
* @in: error_msg: the error message
* @return: HIAI_StatusT
*/
HIAI_StatusT MindInferenceEngine::HandleExceptions(std::string errorMsg) {
    HIAI_ENGINE_LOG(HIAI_IDE_ERROR, errorMsg.c_str());
    tranData_->status = false;
    tranData_->msg = errorMsg;
    return SendResult();
};

/**
* @brief: call ai model manager to do the prediction
* @return: HIAI_StatusT
*/
HIAI_StatusT MindInferenceEngine::Predict() {
    //pre malloc OutData
    HIAI_StatusT hiaiRet = HIAI_OK;

    // 循环两次，因为模型推理一次的输出vector是两路
    for (uint32_t index = 0; index < outputTensorVec_.size(); index++) {
        // 获取AINNNodeDescription对象
        hiai::AITensorDescription outputTensorDesc = hiai::AINeuralNetworkBuffer::GetDescription();
        // buf为保存模型输出结果而申请的一段内存空间
        uint8_t* buf = nullptr;
        hiaiRet = hiai::HIAIMemory::HIAI_DMalloc(outputTensorVec_[index].size, (void *&)buf, 1000);
        if (hiaiRet != HIAI_OK || buf == nullptr) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call HIAI_DMalloc.");
            // 疑问一：分配内存失败就要清理输出的空间。
            ClearOutData();
            return HIAI_ERROR;
        }
        // outData_记录模型推理一次，保存推理结果所要申请的内存空间的大小
        outData_.push_back(buf);

        std::shared_ptr<hiai::IAITensor> outputTensor = hiai::AITensorFactory::GetInstance()->CreateTensor(outputTensorDesc, buf, outputTensorVec_[index].size);
        shared_ptr<hiai::AINeuralNetworkBuffer> nnTensor = static_pointer_cast<hiai::AINeuralNetworkBuffer>(outputTensor);

        nnTensor->SetName(outputTensorVec_[index].name);
        // 推理结果的输出Tensor
        outputDataVec_.push_back(outputTensor);
    }

    // put buffer to FrameWork directly, InputSize has only one
    hiai::AITensorDescription inputTensorDesc = hiai::AINeuralNetworkBuffer::GetDescription();
    // 一个map中保存一张图片的信息
    for (unsigned int i = 0; i < predictInputData_.size(); i++) {
        std::map<uint8_t *, int> tmp = predictInputData_[i];

        // 循环一次
        for (std::map<uint8_t *, int>::iterator it = tmp.begin();it != tmp.end(); ++it) {
            shared_ptr<hiai::IAITensor> inputTensor =
                hiai::AITensorFactory::GetInstance()->CreateTensor(inputTensorDesc, (void *)(it->first), it->second);
            inputDataVec_.push_back(inputTensor); // AIModelManager push input data
        }
    }

    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] Start to call Process for AIModelManager.");

    hiai::AIContext aiContext;
    hiaiRet = aiModelManager_->Process(aiContext, inputDataVec_, outputDataVec_, 0);
    if (hiai::SUCCESS != hiaiRet) {
        // 推理失败清空HIAI_DMalloc申请的内存空间
        ClearOutData();
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call Process for AIModelManager, error code: %u", hiaiRet);
        return HIAI_ERROR;
    }
    return HIAI_OK;
}

/**
* @brief: prepare the data buffer for image data
* @in: buffer: buffer pointer
* @in: imageCount: total number of received images
* @in: start: the index of the first image of each batch
* @in: imageSize: size of each image
* @return: HIAI_StatusT
*/
HIAI_StatusT MindInferenceEngine::PrepareForInputBuffer0(uint8_t* buffer, const int imageCount, const int start, const int imageSize) {
    //1.prepare input buffer for each batch
    //the loop for each image

    for (int j = 0; j < batchSize_; j++) {
        if (start + j < imageCount) {
            // 只循环执行一次
            if (memcpy_s(buffer + j * imageSize, imageSize, imageHandle_->v_img[start + j].img.data.get(), imageSize)) {
                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call memcpy_s for inputBuffer0 to copy image buffer.");
                return HIAI_ERROR;
            }
        } else {
            if (memset_s(buffer + j * imageSize, imageSize, static_cast<char>(0), imageSize)) {
                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call memcpy_s for inputBuffer0 to fill zero.");
                return HIAI_ERROR;
            }
        }
    }
    return HIAI_OK;
}

/**
* @brief: prepare the data buffer for image information
* @in: buffer: buffer pointer
* @in: imageCount: total number of received images
* @in: start: the index of the first image of each batch
* @in: multiInput1: the second input received from the previous engine
* @return: HIAI_StatusT
*/
HIAI_StatusT MindInferenceEngine::PrepareForInputBuffer1(uint8_t* buffer, const int imageCount, const int start, std::shared_ptr<hiai::BatchRawDataBuffer> multiInput1) {
    //the loop for each info
    for (int j = 0; j < batchSize_; j++) {
        if (start + j < imageCount) {
            hiai::RawDataBuffer rawDataBuffer = multiInput1->v_info[start + j];
            int size = rawDataBuffer.len_of_byte;
            if (memcpy_s(buffer + j * size, size, rawDataBuffer.data.get(), size)) {
                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call memcpy_s for inputBuffer1 to copy image info.");
                return HIAI_ERROR;
            }
        } else {
            float info[3] = { 0.0, 0.0, 0.0 };
            int size = sizeof(info);
            if (memcpy_s(buffer + j * size, size, info, size)) {
                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call memcpy_s for inputBuffer1 to fill zero.");
                return HIAI_ERROR;
            }
        }
    }
    return HIAI_OK;
}

/**
* @brief: set the frame ID as -1 to indicate this batch failed
* @in: start: index of the begin of this batch
* @in: imageCount: the image count
*/
void MindInferenceEngine::HandleBatchFailure(const int start, const int imageCount) {
    for (int i = 0; i < batchSize_; i++) {
        if(start + i < imageCount){
            tranData_->b_info.frame_ID[i + start] = -1;
        }
    }
}

/**
* @brief: set the tranData_ with the result of this batch
* @in: start: index of the begin of this batch
* @return: HIAI_StatusT
*/
HIAI_StatusT MindInferenceEngine::SetOutputStruct(const int start) {
    for (unsigned int i = 0; i < outputDataVec_.size(); ++i) {
        std::shared_ptr<hiai::AINeuralNetworkBuffer> resultTensor = std::static_pointer_cast<hiai::AINeuralNetworkBuffer>(outputDataVec_[i]);
        auto tensorSize = resultTensor->GetSize();
        if (memcpy_s(tranData_->output_data_vec[i].data.get() + start / batchSize_ * tensorSize, tensorSize, resultTensor->GetBuffer(), tensorSize)) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call memcpy_s for tranData_->output_data_vec[%d].data.", i);
            return HIAI_ERROR;
        }
    }
    return HIAI_OK;
}

/**
* @brief: send the predicted result for one batch
* @return: HIAI_StatusT
*/
HIAI_StatusT MindInferenceEngine::SendResult() {
    HIAI_StatusT hiaiRet = HIAI_OK;
    do {
        hiaiRet = SendData(0, "EngineTransT", std::static_pointer_cast<void>(tranData_));
        if (HIAI_QUEUE_FULL == hiaiRet) {
            HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] The queue is full, sleep 200ms");
            usleep(SEND_DATA_INTERVAL_MS);
        }
    } while (hiaiRet == HIAI_QUEUE_FULL);

    if (HIAI_OK != hiaiRet) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to send data , error code: %d", hiaiRet);
    }
    return hiaiRet;
}

/**
* @ingroup hiaiengine
* @brief HIAI_DEFINE_PROCESS : Realize the port input/output processing
* @[in]: Define an input port, an output port,
*        And the Engine is registered, its called "HIAIMultiEngineExample"
*/

HIAI_IMPL_ENGINE_PROCESS("MindInferenceEngine", MindInferenceEngine, INPUT_SIZE) {
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] Start process!");
    HIAI_StatusT hiaiRet = HIAI_OK;
    std::lock_guard<std::mutex> lk(memoryRecursiveMutex_);
    // 类的构造函数中已经为其赋值为了nullptr
    if (tranData_ == nullptr) {
        tranData_ = std::make_shared<EngineTransT>();
    }

    // 1.PreProcess:Framework input data
    imageHandle_ = nullptr;
    // 将上一个引擎发送的数据送入队列中
    inputQueue_.PushData(0, arg0);
#if (INPUT_SIZE == 1)
    // 取出所有队列中头部的数据,因为INPUT_SIZE == 1所以只需传递一个参数接收
    // 如果获取失败,写入错误信息并发送给下一个引擎
    if (!inputQueue_.PopAllData(imageHandle_)) {
        return HandleExceptions("[MindInferenceEngine] Failed to call PopAllData.");
    }
#endif

#if (INPUT_SIZE == 2)
    DEFINE_MULTI_INPUT_ARGS_POP(2);
#endif
    // 接收到的数据为空,写入错误信息并发送给下一个引擎
    if (imageHandle_ == nullptr) {
        return HandleExceptions("[MindInferenceEngine] The imageHandle_ is null.");
    }
    // add sentinel image for showing this data in dataset are all sent, this is last step.
    // 接受到的是记录结束标志的数据
    if (isSentinelImage(imageHandle_)) {
        tranData_->status = true;
        tranData_->msg = "sentinel Image";
        tranData_->b_info = imageHandle_->b_info;
        // 发送数据给后处理引擎
        return SendResult();
    }
    // 记录发送过来的图片的个数
    int imageCount = imageHandle_->v_img.size();

#if (INPUT_SIZE == 2)
    if (multiInput1 == nullptr) {
        return HandleExceptions("[MindInferenceEngine] The multiInput1 is null.");
    }

    if (multiInput1->v_info.size() != imageCount) {
        return HandleExceptions("[MindInferenceEngine] The number of image data and information data doesn't match.");
    }
    int inputBuffer1Size = sizeof(float) * IMAGE_INFO_DATA_NUM * batchSize_;
    uint8_t * inputBuffer1 = nullptr;
    hiaiRet = hiai::HIAIMemory::HIAI_DMalloc(inputBuffer1Size, (void *&)inputBuffer1, 1000);
    if (hiaiRet != HIAI_OK || inputBuffer1 == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call HIAI_DMalloc for inputBuffer1.");
        return HIAI_ERROR;
    }
#endif
    int imageSize = imageHandle_->v_img[0].img.size * sizeof(uint8_t);
    int inputBuffer0Size  = imageSize * batchSize_;
    if (inputBuffer0Size <= 0) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] inputBuffer0Size <= 0");
        return HIAI_ERROR;
    }
    uint8_t *inputBuffer0 = nullptr;
    // 根据图片大小申请内存空间
    hiaiRet = hiai::HIAIMemory::HIAI_DMalloc(inputBuffer0Size, (void *&)inputBuffer0, 1000);
    if (hiaiRet != HIAI_OK || inputBuffer0 == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] Failed to call HIAI_DMalloc for inputBuffer0.");
#if (INPUT_SIZE == 2)
        hiai::HIAIMemory::HIAI_DFree(inputBuffer1);
        inputBuffer1 = nullptr;
#endif
        return HIAI_ERROR;
    }
    int batchCount = imageHandle_->b_info.batch_size / batchSize_;
    if (imageHandle_->b_info.batch_size % batchSize_ != 0) {
        batchCount++;
    }
    tranData_->b_info = imageHandle_->b_info;
    // 预处理后的图片信息
    tranData_->v_img = imageHandle_->v_img;
    tranData_->status = true;
    // 1
    tranData_->b_info.max_batch_size = batchCount * batchSize_;

    // 只循环了一次
    for (int i = 0; i < imageCount; i+= batchSize_) {
        predictInputData_.clear();
        // 
        if (HIAI_OK != PrepareForInputBuffer0(inputBuffer0, imageCount, i, imageSize)) {
            // 拷贝图片数据失败之后frame_ID赋值为-1
            HandleBatchFailure(i, imageCount);
            continue;
        }
        std::map<uint8_t *, int> input0;
        input0.insert(std::make_pair(inputBuffer0, inputBuffer0Size));
        // 模型输入Tensor
        predictInputData_.push_back(input0);
#if (INPUT_SIZE == 1)
        DEFINE_MULTI_INPUT_ARGS(1);
#endif
#if (INPUT_SIZE == 2)
        if (HIAI_OK != PrepareForInputBuffer1(inputBuffer1, imageCount, i, multiInput1)) {
            HandleBatchFailure(i, imageCount);
            continue;
        }
        std::map<uint8_t *, int> input1;
        input1.insert(std::make_pair(inputBuffer1, inputBuffer1Size));
        predictInputData_.push_back(input1);
        DEFINE_MULTI_INPUT_ARGS(2);
#endif

        // 2.Call Process, Predict
        inputDataVec_.clear();
        if (HIAI_OK != Predict()) {
            // 将vector中的智能指针分配的空间释放，在释放此空间之前已经先将HIAI_DMalloc申请的空间释放了
            outputDataVec_.clear();
            HandleBatchFailure(i, imageCount);
            continue;
        }
        //init the output buffer for one dataset batch(might be multiple model batches)
        if (tranData_->output_data_vec.empty()) {
            tranData_->size = outputDataVec_.size();
            HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] Alloc memory for dataset batch, number of outputs of the network: %d", outputDataVec_.size());
            for (unsigned int i = 0; i < outputDataVec_.size(); i++) {
                OutputT out;
                std::shared_ptr<hiai::AINeuralNetworkBuffer> resultTensor = std::static_pointer_cast<hiai::AINeuralNetworkBuffer>(outputDataVec_[i]);
                int bufferSize = resultTensor->GetSize();
                out.name = resultTensor->GetName();
                out.size = bufferSize * batchCount;
                if(out.size <= 0){
                    HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[MindInferenceEngine] out.size <= 0");
                    hiai::HIAIMemory::HIAI_DFree(inputBuffer0);
                    inputBuffer0 = nullptr;
#if (INPUT_SIZE == 2)
                    hiai::HIAIMemory::HIAI_DFree(inputBuffer1);
                    inputBuffer1 = nullptr;
#endif
                    ClearOutData();
                    return HIAI_ERROR;
                }
                u_int8_t *ptr = nullptr;
                try {
                    ptr = new u_int8_t[out.size];
                } catch (const std::bad_alloc& e) {
                    hiai::HIAIMemory::HIAI_DFree(inputBuffer0);
                    inputBuffer0 = nullptr;
#if (INPUT_SIZE == 2)
                    hiai::HIAIMemory::HIAI_DFree(inputBuffer1);
                    inputBuffer1 = nullptr;
#endif
                    ClearOutData();
                    return HIAI_ERROR;
                }
                out.data.reset(ptr);
                tranData_->output_data_vec.push_back(out);
                HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] The image count is %d.", imageCount);
            }
        }

        //3.set the tranData_ with the result of this batch
        if (HIAI_OK != SetOutputStruct(i)) {
            ClearOutData();
            outputDataVec_.clear();
            HandleBatchFailure(i, imageCount);
            continue;
        }
        outputDataVec_.clear();
    }


    hiaiRet = SendResult();
    //6. release sources
    hiai::HIAIMemory::HIAI_DFree(inputBuffer0);
    inputBuffer0 = nullptr;
#if (INPUT_SIZE == 2)
    hiai::HIAIMemory::HIAI_DFree(inputBuffer1);
    inputBuffer1 = nullptr;
#endif
    ClearOutData();
    tranData_ = nullptr;
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[MindInferenceEngine] End process!");
    return hiaiRet;
}