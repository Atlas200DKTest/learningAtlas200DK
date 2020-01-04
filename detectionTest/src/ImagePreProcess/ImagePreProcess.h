/**
* @file ImagePreProcess.h
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef IMAGE_PRE_PROCESS_H_
#define IMAGE_PRE_PROCESS_H_

#include "hiaiengine/engine.h"
#include "hiaiengine/data_type.h"
#include "hiaiengine/data_type_reg.h"
#include "hiaiengine/multitype_queue.h"
#include "dvpp/idvppapi.h"
#include "dvpp/Vpc.h"
#include "Common.h"

#define INPUT_SIZE 1
#define OUTPUT_SIZE 1

using hiai::Engine;
using hiai::AIConfig;
using hiai::AIModelDescription;
using hiai::ImageData;
using hiai::BatchInfo;
using hiai::BatchImagePara;
using hiai::BatchDetectedObjectPara;
using hiai::Rectangle;
using hiai::Point2D;


typedef struct DvppConfig {
    uint32_t resize_width;
    uint32_t resize_height;

    DvppConfig() {
        resize_width = 0;
        resize_height = 0;
    }
}DvppConfig;

typedef struct DvppPreprocessInfo {
    uint32_t resize_width;
    uint32_t resize_height;
    uint32_t preprocess_width;
    uint32_t preprocess_height;
    uint32_t frameID;
    uint32_t orderInFrame;
    uint32_t reserverd[2];
}DvppPreprocessInfo;

typedef enum {
    YUV420_SEMI_PLANNAR = 0,  //0
    YUV422_SEMI_PLANNAR,
    YUV444_SEMI_PLANNAR,
    YUV422_PACKED,
    YUV444_PACKED,
    RGB888_PACKED,  //5
    XRGB8888_PACKED,
    YUV400_SEMI_PLANNAR,
}Imge_Type;

typedef enum {
    RANK_TYPE_NV12 = 0,
    RANK_TYPE_NV21,
    RANK_TYPE_YUYV,
    RANK_TYPE_YVYU,
    RANK_TYPE_UYVY,
    RANK_TYPE_YUV,

    RANK_TYPE_RGB, //6
    RANK_TYPE_BGR,
    RANK_TYPE_RGBA,
    RANK_TYPE_BGRA,
    RANK_TYPE_ARGB,

    RANK_TYPE_ABGR, //11
}IMAGE_RANK_TYPE;

//self_defined, reason as follows by dvpp, png format pic only supports RGB & RGBA
enum pngd_color_space {
    DVPP_PNG_DECODE_OUT_RGB = 2,
    DVPP_PNG_DECODE_OUT_RGBA = 6
};

typedef enum {
    FILE_TYPE_PIC_JPEG = 0x1,
    FILE_TYPE_PIC_PNG,
    FILE_TYPE_YUV,
    FILE_TYPE_MAX
} FILE_TYPE;

// Define ImagePreProcess
class ImagePreProcess : public Engine {
private:
    const int SEND_BUFFER_SIZE = 1024*1024; //transfer max limit each once:1024KB

public:
    ImagePreProcess():
        dvppConfig_(nullptr),dvppOut_(nullptr),dvppIn_(nullptr),pidvppapi_(nullptr),imageFrameID_(0),inputQue_(INPUT_SIZE){}
    ~ImagePreProcess();
    HIAI_StatusT Init(const AIConfig& config, const std::vector<AIModelDescription>& model_desc);
    HIAI_DEFINE_PROCESS(INPUT_SIZE, OUTPUT_SIZE);

private:
    void ClearData();
    int HandleJpeg(const ImageData<u_int8_t> &img);
    int HandleVpcWithParam(const unsigned char* buffer, const int &width, const int &height, const long &bufferSize,
        const ImageData<u_int8_t> &img, const FILE_TYPE &type, const int &format);
    void ProcessCrop(VpcUserCropConfigure &area, const int &width, const int &height, const int &realWidth, const int &realHeight);
    bool SendPreProcessData();
    int HandleDvpp();

private:
    // 保存图片预处理的数据
    std::shared_ptr<DvppConfig> dvppConfig_;
    std::shared_ptr<BatchImageParaWithScaleT> dvppOut_;
    // 接受从上一个引擎发送过来的数据
    std::shared_ptr<BatchImageParaWithScaleT> dvppIn_;
    // 获取DVPP执行器句柄
    IDVPPAPI *pidvppapi_;
    // 记录待测图片的顺序
    uint32_t imageFrameID_;
    hiai::MultiTypeQueue inputQue_;
};

#endif // IMAGE_PRE_PROCESS_H_
