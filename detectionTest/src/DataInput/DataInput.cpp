/**
* @file DataInput.cpp
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include <memory>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <math.h>
#include <map>
#include "DataInput.h"
#include "hiaiengine/log.h"
#include "hiaiengine/data_type_reg.h"
#include "hiaiengine/ai_memory.h"

const static std::string RC = "RC";
const int DVPP_BUFFER_ALIGN_SIZE = 128;

/**
* @brief: make datasetInfo_
*/
// 读取本地的图片，并且将图片信息保存到vector中
void DataInput::MakeDatasetInfo() {
    // 将待检测的图片信息保存到类成员中
    datasetInfo_.clear();
    ImageInfo imageInfo1;
    imageInfo1.id = 0;
    imageInfo1.path = path_ + "2008_000014.jpg";
    imageInfo1.width = 500;
    imageInfo1.height = 374;
    imageInfo1.size = 140054;
    imageInfo1.format = (int)IMAGE_TYPE_JPEG;
    datasetInfo_.push_back(imageInfo1);
    ImageInfo imageInfo2;
    imageInfo2.id = 1;
    imageInfo2.path = path_ + "2008_000159.jpg";
    imageInfo2.width = 500;
    imageInfo2.height = 375;
    imageInfo2.size = 91741;
    imageInfo2.format = (int)IMAGE_TYPE_JPEG;
    datasetInfo_.push_back(imageInfo2);
    ImageInfo imageInfo3;
    imageInfo3.id = 2;
    imageInfo3.path = path_ + "2010_004236.jpg";
    imageInfo3.width = 500;
    imageInfo3.height = 375;
    imageInfo3.size = 67517;
    imageInfo3.format = (int)IMAGE_TYPE_JPEG;
    datasetInfo_.push_back(imageInfo3);
}

// 初始化函数
// 读取graph.config,设置图片信息到类中的成员中
HIAI_StatusT DataInput::Init(const hiai::AIConfig& config, const std::vector<hiai::AIModelDescription>& model_desc) {
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] Start init!");

    //read the config of dataset
    for (int index = 0; index < config.items_size(); ++index) 
    {
        const ::hiai::AIConfigItem& item = config.items(index);
        std::string name = item.name();

        if (name == "target") {
            target_ = item.value();
        } else if (name == "path") {
            path_ = item.value();
        }
    }
    //get the dataset image info
    MakeDatasetInfo();

    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] End init!");
    return HIAI_OK;
}

/**
* @brief: get the image buffer
* @[in]: path, the image path;
* @[in]: imageBufferPtr, the point of image buffer;
* @[in]: imageBufferLen, the buffer length;
* @[in]: frameId, the start of file offset
* @[return]: bool, if success return true, else return false
*/
// 1.路径 2.用于存储图片数据的地址(MALLOC申请的内存地址) 3.图片的大小 4.偏移量
bool DataInput::GetImageBuffer(const char* path, uint8_t* imageBufferPtr, uint32_t imageBufferLen, uint32_t frameId){
    bool ret = false;
    // fopen64用于读取大型文件
    FILE * file = fopen64(path, "r");
    if (file == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to  open file %s.", path);
        return ret;
    }
    do {
        unsigned long imageFseek = ((unsigned  long)frameId)*((unsigned  long)imageBufferLen);
        // 操作文件指针的偏移量,SEEK_SET指代文件开头的位置
        // 成功返回0
        if (0 != fseeko64(file, imageFseek, SEEK_SET)) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call fseeko64 for offset(%u)", frameId * imageBufferLen);
            break;
        }

        // 返回读取的大小
        if (imageBufferLen != fread(imageBufferPtr, 1, imageBufferLen, file)) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call fread for length(%u).", imageBufferLen);
            break;
        }
        ret = true;
    } while (0);

    
    fclose(file);
    return ret;
}

/**
* @brief: convert image info to EvbImageInfo
* @[in]: index, index of image in datasetInfo_
* @[return]: shared_ptr<EvbImageInfo>, if null, means error
*/
shared_ptr<EvbImageInfo> DataInput::makeEvbImageInfo(int index){
    if (index < 0 || (unsigned int)index >= datasetInfo_.size()) {
        return nullptr;
    }
    shared_ptr<EvbImageInfo> evbImageInfo = std::make_shared<EvbImageInfo>();
    if (evbImageInfo == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call make_shared for EvbImageInfo.");
        return nullptr;
    }
    evbImageInfo->format = (IMAGEFORMAT)datasetInfo_[index].format;
    evbImageInfo->width = datasetInfo_[index].width ;
    evbImageInfo->height = datasetInfo_[index].height;
    std::string imageFullPath = datasetInfo_[index].path;
    if ((ImageType)datasetInfo_[index].format == IMAGE_TYPE_JPEG) {
        // transfer jepg to imagepreprocess use dvpp jepgd need to add 8 bit for check
        evbImageInfo->size = datasetInfo_[index].size + 8;
    } else {
        evbImageInfo->size = datasetInfo_[index].size;
    }
    uint8_t * imageBufferPtr = nullptr;
    HIAI_StatusT mallocRet = hiai::HIAIMemory::HIAI_DMalloc(evbImageInfo->size, (void*&)imageBufferPtr);
    if (HIAI_OK != mallocRet || nullptr == imageBufferPtr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call HIAI_DMalloc.");
        return nullptr;
    }
    bool ret = GetImageBuffer(imageFullPath.c_str(), imageBufferPtr, datasetInfo_[index].size, 0);
    if (!ret) {
        mallocRet = hiai::HIAIMemory::HIAI_DFree(imageBufferPtr);
        if (HIAI_OK != mallocRet) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call HIAI_DFree, error code: %u.", mallocRet);
        }
        imageBufferPtr = nullptr;
        return nullptr;
    }
    evbImageInfo->pucImageData = imageBufferPtr;
    evbImageInfo->frame_ID = datasetInfo_[index].id;
    return evbImageInfo;
}

/**
* @brief free the buffer malloced by HIAI:MALLOC
*/
static void FreeImageBuffer(uint8_t* ptr){
    if (ptr == nullptr) {
        return;
    }
    HIAI_StatusT ret = HIAI_OK;
    #if defined(IS_RC)
        ret = hiai::HIAIMemory::HIAI_DVPP_DFree(ptr);
    #else
        ret = hiai::HIAIMemory::HIAI_DFree(ptr);
    #endif
    if (HIAI_OK != ret) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call DFree.");
    }
    ptr = nullptr;
}

/**
* @brief: convert image info to NewImageParaT
* @[in]: index, index of image in datasetInfo_
* @[out]: imgData, the point of data image
* @[return]: HIAI_StatusT
*/
HIAI_StatusT DataInput::makeImageInfo(NewImageParaT& imgData, int index) {
    if (index < 0 || (uint32_t)index >= datasetInfo_.size()) {
        return HIAI_ERROR;
    }

    
    imgData.img.format = (IMAGEFORMAT)datasetInfo_[index].format;
    imgData.img.width = datasetInfo_[index].width ;
    imgData.img.height = datasetInfo_[index].height;

    std::string imageFullPath = datasetInfo_[index].path;

    uint8_t * imageBufferPtr = nullptr;
    HIAI_StatusT mallocRet = HIAI_OK;
    #if defined(IS_RC)
        //run on same side with dvpp
        if ((ImageType)datasetInfo_[index].format == IMAGE_TYPE_JPEG) {
        // transfer jepg to imagepreprocess use dvpp jepgd need to add 8 bit for check
            imgData.img.size = datasetInfo_[index].size + 8;
        } else {
            imgData.img.size = datasetInfo_[index].size;
        }
        //run on same side with dvpp need to make the mem align to 128(dvpp need)
        int alignBufferSize = (int)ceil(1.0 * imgData.img.size / DVPP_BUFFER_ALIGN_SIZE) * DVPP_BUFFER_ALIGN_SIZE;
        mallocRet = hiai::HIAIMemory::HIAI_DVPP_DMalloc(alignBufferSize, (void*&)imageBufferPtr);
    #else
        imgData.img.size = datasetInfo_[index].size;
        // 申请内存(一张图片的内存大小)
        mallocRet = hiai::HIAIMemory::HIAI_DMalloc(imgData.img.size, (void*&)imageBufferPtr);
    #endif

    if (HIAI_OK != mallocRet || imageBufferPtr == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call DMalloc.");
        return HIAI_ERROR;
    }

    // 1.路径 2.用于存储图片数据的地址 3.图片的大小 4.偏移量
    // 打开图片将里面的内容读取到HIAI_DMalloc申请的空间中
    if (!GetImageBuffer(imageFullPath.c_str(), imageBufferPtr, datasetInfo_[index].size, 0)) {
        // 读取内容失败，释放HIAI_DMalloc申请的空间
        FreeImageBuffer(imageBufferPtr);
        return HIAI_ERROR;
    }
    // free imageBufferPtr with function FreeImageBuffer()
    // 指定自定义的删除器
    shared_ptr<uint8_t> data(imageBufferPtr, FreeImageBuffer);
    imgData.img.data = data;
    return HIAI_OK;
}

/**
* @brief: send batch for RC
* @[in]: batchId, batchId;
* @[in]: batchNum, the total number of batch;
* @[in]: imageInfoBatch, the send data;
* @[return]: HIAI_StatusT
*/
// 1.下标 2.总共的图片个数 3.当前的图片信息
HIAI_StatusT DataInput::SendBatch(int batchId, int batchNum, std::shared_ptr<BatchImageParaWithScaleT> imageInfoBatch){
    HIAI_StatusT hiaiRet = HIAI_OK;

    // 填充完结构体之后,发送图片给下一个引擎
    imageInfoBatch->b_info.batch_size = imageInfoBatch->v_img.size();
    imageInfoBatch->b_info.max_batch_size = 1;
    imageInfoBatch->b_info.batch_ID = batchId;
    imageInfoBatch->b_info.is_first = (batchId == 0 ? true : false);
    imageInfoBatch->b_info.is_last = (batchId == batchNum - 1 ? true : false);

    do {
        hiaiRet = SendData(DEFAULT_DATA_PORT, "BatchImageParaWithScaleT", std::static_pointer_cast<void>(imageInfoBatch));
        if (HIAI_QUEUE_FULL == hiaiRet) {
            HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] The queue is full, sleep 200ms.");
            usleep(SEND_DATA_INTERVAL_MS);
        }
    } while (hiaiRet == HIAI_QUEUE_FULL);

    if (HIAI_OK != hiaiRet) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to send data for batch %u, error code: %u.", batchId, hiaiRet);
    }
    return hiaiRet;
}

/**
* @brief: send batch for EP
* @[in]: batchId, batchId;
* @[in]: batchNum, the total number of batch;
* @[in]: imageInfo, the send data;
* @[return]: HIAI_StatusT
*/
HIAI_StatusT DataInput::SendEvbBatch(int batchId, int batchNum, std::shared_ptr<EvbImageInfo>& imageInfo){
    HIAI_StatusT hiaiRet = HIAI_OK;
    imageInfo->batch_size = 1;
    imageInfo->batch_index = 0;
    imageInfo->max_batch_size = 1;
    imageInfo->batch_ID = batchId;
    imageInfo->is_first = (batchId == 0 ? true : false);
    imageInfo->is_last = (batchId == batchNum - 1 ? true : false);
    do {
        hiaiRet = SendData(DEFAULT_DATA_PORT, "EvbImageInfo", std::static_pointer_cast<void>(imageInfo));
        if (HIAI_GRAPH_NO_USEFUL_MEMORY == hiaiRet || HIAI_GRAPH_INVALID_VALUE == hiaiRet) {
            HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] There is no useful memory, sleep 200ms.");
            usleep(SEND_DATA_INTERVAL_MS);
        }
    } while (HIAI_GRAPH_NO_USEFUL_MEMORY == hiaiRet  || HIAI_GRAPH_INVALID_VALUE == hiaiRet);

    if (HIAI_OK != hiaiRet) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to send data for frame id %u, error code: %u.", imageInfo->frame_ID, hiaiRet);
        FreeEvbBuffer(imageInfo);
    }
    return hiaiRet;
}

/**
* free evb buffer
**/
void DataInput::FreeEvbBuffer(std::shared_ptr<EvbImageInfo>& imageInfo){
    HIAI_StatusT ret = HIAI_OK;
    if (imageInfo->pucImageData != nullptr) {
        ret = hiai::HIAIMemory::HIAI_DFree(imageInfo->pucImageData);
        if (HIAI_OK != ret) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call HIAI_DFree, error code: %u.", ret);
        }
        imageInfo->pucImageData = nullptr;
    }       
}

/**
* @brief: run images on same side, all engine at same side
* @[return]: HIAI_StatusT
*/
HIAI_StatusT DataInput::RunOnSameSide(){
    HIAI_StatusT ret = HIAI_OK;
    // 读取到的图片信息的个数
    int totalCount = datasetInfo_.size();

    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] Run on %s for %u images", target_.c_str(), totalCount);


    for (int i = 0; i < totalCount; i++) {
        //convert batch image infos to BatchImageParaWithScaleT
        std::shared_ptr<BatchImageParaWithScaleT> imageInfoBatch = std::make_shared<BatchImageParaWithScaleT>();
        if (imageInfoBatch == nullptr) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call make_shared for BatchImageParaWithScaleT.");
            return HIAI_ERROR;
        }

        NewImageParaT imgData;
        // 填充结构体
        ret = makeImageInfo(imgData, i);
        if (HIAI_OK != ret) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to make image info for frame id %u. Stop to send images.",i);
            return ret;
        }

        // 发送图片信息
        imageInfoBatch->v_img.push_back(imgData);
        // 记录的是待测图片得到序号
        imageInfoBatch->b_info.frame_ID.push_back(datasetInfo_[i].id);
    
        //then send data
        // 1.下标 2.总共的图片个数 3.当前的图片信息
        ret = SendBatch(i, totalCount, imageInfoBatch);
        if (HIAI_OK != ret) {
            return ret;
        }
    }
    return  HIAI_OK;
}

/**
* @brief: run images on different side, part of engines on host side, other engines on device side
* @[return]: HIAI_StatusT
*/
HIAI_StatusT DataInput::RunOnDifferentSide(){
    HIAI_StatusT ret = HIAI_OK;
    int totalCount = datasetInfo_.size();
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] Run on %s for %u images.", target_.c_str(), totalCount);
    for (int i = 0; i < totalCount; i++) {
        //convert batch image infos to std::shared_ptr<EvbImageInfo>
        shared_ptr<EvbImageInfo> evbImageInfo = makeEvbImageInfo(i);
        if (evbImageInfo == nullptr) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to make evb image info for frame id %u. Stop to send images.",i);
            FreeEvbBuffer(evbImageInfo);
            return HIAI_ERROR;
        }
        //then send data
        ret = SendEvbBatch(i, totalCount, evbImageInfo);
        if (HIAI_OK != ret) {
            return ret;
        }
    }
    return  HIAI_OK;
}

/**
* @brief: check whether run images on same side
* @[return]: HIAI_StatusT
*/
bool DataInput::isOnSameSide(){
    if (target_ == RC) {
        return true;
    }
    return false;
}

/**
* @brief: Send Sentinel Image
*/
HIAI_StatusT DataInput::SendSentinelImage()
{
    HIAI_StatusT ret = HIAI_OK;
    //all data send ok, then send a Sentinel info to other engine for end
    if (isOnSameSide()) {
        //if all engine on one side, use BatchImageParaWithScaleT to send data
        shared_ptr<BatchImageParaWithScaleT> image_handle = std::make_shared<BatchImageParaWithScaleT>();
        if (image_handle == nullptr) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] Failed to call make_shared for BatchImageParaWithScaleT.");
            return HIAI_ERROR;
        }
        // 下一个引擎在处理
        ret = SendBatch(-1, 1, image_handle);
    } else {
        //if part of engines on host side, other engines on device side, use EvbImageInfo to send data
        shared_ptr<EvbImageInfo> imageInfo = std::make_shared<EvbImageInfo>();     
        if (imageInfo == nullptr) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[DataInput] make shared for EvbImageInfo error!");
            return HIAI_ERROR;
        }
        ret = SendEvbBatch(-1, 1, imageInfo);
    }
    return ret;
}

/**
* @ingroup hiaiengine
* @brief HIAI_DEFINE_PROCESS : Overloading Engine Process processing logic
* @[in]: Define an input port, an output port
*/
HIAI_IMPL_ENGINE_PROCESS("DataInput", DataInput, INPUT_SIZE)
{
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] Start process!");
    std::static_pointer_cast<string>(arg0);

    HIAI_StatusT ret = HIAI_OK;
    if (isOnSameSide()) {
        //all engine on one side
        ret = RunOnSameSide();
    } else {
        //part of engines on host side, other engines on device side
        ret = RunOnDifferentSide();
    }
    //send sentinel image
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] Send sentinel image.");
    ret = SendSentinelImage();
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[DataInput] End process!");
    return ret;
}
