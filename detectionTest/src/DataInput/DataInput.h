/**
* @file DataInput.h
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef DATA_INPUT_ENGINE_H_
#define DATA_INPUT_ENGINE_H_

#include <iostream>
#include <string>
#include <dirent.h>
#include <memory>
#include <unistd.h>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include "hiaiengine/engine.h"
#include "hiaiengine/data_type.h"
#include "hiaiengine/multitype_queue.h"
#include "Common.h"

#define INPUT_SIZE 1
#define OUTPUT_SIZE 1
#define DEFAULT_DATA_PORT 0

using hiai::Engine;
using namespace std;
using namespace hiai;

//the image info of dataset
struct ImageInfo {
    std::string path; //the path of image
    int format;       //the format of image
    int height;
    int width;
    int size;
    int id;
};

class DataInput : public Engine {
public:
    DataInput() :
        path_(""), target_(""){}
    ~DataInput(){}
    HIAI_StatusT Init(const hiai::AIConfig& config, const  std::vector<hiai::AIModelDescription>& model_desc) override;
    
    /**
    * @ingroup hiaiengine
    * @brief HIAI_DEFINE_PROCESS : Overloading Engine Process processing logic
    * @[in]: Define an input port, an output port
    */
    HIAI_DEFINE_PROCESS(INPUT_SIZE, OUTPUT_SIZE)
private:
    /**
    * @brief: Send Sentinel Image
    */
    HIAI_StatusT SendSentinelImage();

    /**
    * @brief: get the image buffer
    * @[in]: path, the image path;
    * @[in]: imageBufferPtr, the point of image buffer;
    * @[in]: imageBufferLen, the buffer length;
    * @[in]: frameId, the start of file offset
    * @[return]: bool, if success return true, else return false
    */
    bool GetImageBuffer(const char* path, uint8_t* imageBufferPtr, uint32_t imageBufferLen, uint32_t frameId);

    /**
    * @brief: send batch for Emulator and OI
    * @[in]: batchId, batchId;
    * @[in]: batchNum, the total number of batch;
    * @[in]: imageInfoBatch, the send data;
    * @[return]: HIAI_StatusT
    */
    HIAI_StatusT SendBatch(int batchId, int batchNum, std::shared_ptr<BatchImageParaWithScaleT> imageInfoBatch);

    /**
    * @brief: send batch for EVB
    * @[in]: batchId, batchId;
    * @[in]: batchNum, the total number of batch;
    * @[in]: imageInfo, the send data;
    * @[return]: HIAI_StatusT
    */
    HIAI_StatusT SendEvbBatch(int batchId, int batchNum, std::shared_ptr<EvbImageInfo>& imageInfo);
    
    /**
    * @brief: convert image info to NewImageParaT
    * @[in]: index, index of image in datasetInfo_
    * @[out]: imgData, the point of data image
    * @[return]: HIAI_StatusT
    */
    HIAI_StatusT makeImageInfo(NewImageParaT& imgData, int index);
    
    /**
    * @brief: convert image info to EvbImageInfo
    * @[in]: index, index of image in datasetInfo_
    * @[return]: shared_ptr<EvbImageInfo>, if null, means error
    */
    shared_ptr<EvbImageInfo> makeEvbImageInfo(int index);

    void FreeEvbBuffer(std::shared_ptr<EvbImageInfo>& imageInfo);
    /**
    * @brief: run images on same side, all engine at same side
    * @[return]: HIAI_StatusT
    */
    HIAI_StatusT RunOnSameSide();

    /**
    * @brief: run images on different side, part of engines on host side, other engines on device side
    * @[return]: HIAI_StatusT
    */
    HIAI_StatusT RunOnDifferentSide();

    /**
    * @brief: check whether run images on same side
    * @[return]: HIAI_StatusT
    */
    bool isOnSameSide();

    /**
    * @brief: make datasetInfo_
    */
    void MakeDatasetInfo();

    //the dataset image infos
    // 保存图片数据信息
    // 在Init中设置数值
    std::vector<ImageInfo> datasetInfo_;
    // 读取graph.config
    std::string target_;
    // 读取graph.config
    std::string path_;
};

#endif // DATA_INPUT_ENGINE_H_
