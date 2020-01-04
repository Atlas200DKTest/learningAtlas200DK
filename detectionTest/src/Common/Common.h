/**
* @file Common.h
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef COMMON_H_
#define COMMON_H_

#include <unistd.h>
#include <sys/stat.h>
#include "hiaiengine/data_type.h"
#include "hiaiengine/data_type_reg.h"
#include "hiaiengine/status.h"
#include "hiaiengine/ai_tensor.h"

using hiai::BatchInfo;
using hiai::IMAGEFORMAT;
using hiai::ImageData;

/**
define error code for HIAI_ENGINE_LOG
**/
#define USE_DEFINE_ERROR 0x6001

enum {
    HIAI_IDE_ERROR_CODE,
    HIAI_IDE_INFO_CODE,
    HIAI_IDE_WARNING_CODE
};

HIAI_DEF_ERROR_CODE(USE_DEFINE_ERROR, HIAI_ERROR, HIAI_IDE_ERROR, "");
HIAI_DEF_ERROR_CODE(USE_DEFINE_ERROR, HIAI_INFO, HIAI_IDE_INFO, "");
HIAI_DEF_ERROR_CODE(USE_DEFINE_ERROR, HIAI_WARNING, HIAI_IDE_WARNING, "");

typedef struct ScaleInfo {
    float scale_width = 1;
    float scale_height = 1;
} ScaleInfoT;
template <class Archive>
void serialize(Archive& ar, ScaleInfoT& data);

typedef struct ResizeInfo {
    uint32_t resize_width = 0;
    uint32_t resize_height = 0;
} ResizeInfoT;
template <class Archive>
void serialize(Archive& ar, ResizeInfo& data);

typedef struct CropInfo {
    int point_x = -1;
    int point_y = -1;
    int crop_width = -1;
    int crop_height = -1;
} CropInfoT;
template <class Archive>
void serialize(Archive& ar, CropInfo& data);

typedef struct NewImagePara {
    hiai::FrameInfo f_info;
    hiai::ImageData<u_int8_t> img;
    ScaleInfoT scale_info;
    ResizeInfo resize_info;
    CropInfo crop_info;
} NewImageParaT;

template <class Archive>
void serialize(Archive& ar, NewImageParaT& data);

typedef struct BatchImageParaWithScale {
    hiai::BatchInfo b_info;
    std::vector<NewImageParaT> v_img;
} BatchImageParaWithScaleT;

template <class Archive>
void serialize(Archive& ar, BatchImageParaWithScaleT& data);

typedef enum ImageType {
    IMAGE_TYPE_JPEG = 1,
    IMAGE_TYPE_PNG,
}ImageTypeT;

struct EvbImageInfo {
    bool is_first;
    bool is_last;
    uint32_t batch_size;
    uint32_t batch_index;
    uint32_t max_batch_size;
    uint32_t batch_ID;
    uint32_t frame_ID;
    int format;
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t size = 0;
    u_int8_t* pucImageData;
};

const int SEND_DATA_INTERVAL_MS = 200000;
const static mode_t PERMISSION = 0700;
const static mode_t FIlE_PERMISSION = 0600;

//The new version of serialize function
void GetEvbImageInfoSearPtr(void *input_ptr, std::string& ctrl_str, uint8_t*& data_ptr, uint32_t& data_len);

/**
* @brief: check if the image_handle is a sentinel image
* @[in]: image_handle, the image to check
* @[return]: bool, true if the image is a sentinel image
*/
bool isSentinelImage(const std::shared_ptr<BatchImageParaWithScaleT> image_handle);

//The new version of deserialize function
std::shared_ptr<void> GetEvbImageInfoDearPtr(const char* ctrl_ptr, const uint32_t& ctr_len, const uint8_t* data_ptr, const uint32_t& data_len);

typedef struct Output {
    int32_t size;
    std::string name;
    std::shared_ptr<u_int8_t> data;
}OutputT;
template<class Archive>
void serialize(Archive& ar, OutputT& data);

// 传送给后处理引擎的数据格式
typedef struct EngineTrans {
    // 模型推理的状态
    bool status;
    // 记录错误信息
    std::string msg;
    hiai::BatchInfo b_info;
    uint32_t size;
    std::vector<OutputT> output_data_vec;
    std::vector<NewImageParaT> v_img;
}EngineTransT;
template<class Archive>
void serialize(Archive& ar, EngineTransT& data);

/**
* @brief: create folder to store the detection results
* the folder name on the host will be "result_files/enginename"
*/
HIAI_StatusT CreateFolder(std::string folderPath, mode_t mode);

/**
* @brief: replace '/' to '_'  in name
* @[in]: name, the name to replace
*/
void GetOutputName(std::string& name);


//split the string, result in vector sv, the seperator is flag.
std::vector<std::string> split(const std::string& s, char delimiter);

#endif // COMMON_H_
