/**
* @file Common.cpp
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include <iostream>
#include<stdio.h>
#include<dirent.h>
#include<regex>
#include <fcntl.h>
#include "Common.h"

template <class Archive>
void serialize(Archive& ar, ScaleInfoT& data) {
    ar(data.scale_width, data.scale_height);
}

template <class Archive>
void serialize(Archive& ar, ResizeInfo& data) {
    ar(data.resize_width, data.resize_height);
}

template <class Archive>
void serialize(Archive& ar, CropInfo& data) {
    ar(data.point_x, data.point_y, data.crop_width, data.crop_height);
}

template <class Archive>
void serialize(Archive& ar, NewImageParaT& data) {
    ar(data.f_info, data.img, data.scale_info,data.resize_info, data.crop_info);
}

template <class Archive>
void serialize(Archive& ar, BatchImageParaWithScaleT& data) {
    ar(data.b_info, data.v_img);
}

template <class Archive>
void serialize(Archive& ar, OutputT& data) {
    ar(data.size);
    ar(data.name);
    if (data.size > 0 && data.data.get() == nullptr) {
        data.data.reset(new u_int8_t[data.size]);
    }

    ar(cereal::binary_data(data.data.get(), data.size * sizeof(u_int8_t)));
}

template <class Archive>
void serialize(Archive& ar, EngineTransT& data) {
   ar(data.status, data.msg, data.b_info, data.size, data.output_data_vec, data.v_img);
}

//The new version of serialize function
void GetEvbImageInfoSearPtr(void *input_ptr, std::string& ctrl_str, uint8_t*& data_ptr, uint32_t& data_len) {
    if (input_ptr == nullptr) {
        return;
    }
    EvbImageInfo* image_info = (EvbImageInfo*)input_ptr;
    ctrl_str = std::string((char*)input_ptr, sizeof(EvbImageInfo));
    data_ptr = (uint8_t*)image_info->pucImageData;
    data_len = image_info->size;
}

//The new version of deserialize function
std::shared_ptr<void> GetEvbImageInfoDearPtr(const char* ctrl_ptr, const uint32_t& ctr_len, const uint8_t* data_ptr, const uint32_t& data_len) {
    if (ctrl_ptr == nullptr) {
        return nullptr;
    }
    EvbImageInfo* image_info = (EvbImageInfo*)ctrl_ptr;
    std::shared_ptr<BatchImageParaWithScaleT> image_handle = std::make_shared<BatchImageParaWithScaleT>();
    image_handle->b_info.frame_ID.push_back(image_info->frame_ID);
    image_handle->b_info.batch_size = image_info->batch_size;
    image_handle->b_info.max_batch_size = image_info->max_batch_size;
    image_handle->b_info.batch_ID = image_info->batch_ID;
    image_handle->b_info.is_first = image_info->is_first;
    image_handle->b_info.is_last = image_info->is_last;
    if (isSentinelImage(image_handle)) {
        return image_handle;
    }
    NewImageParaT imgData;
    imgData.img.format = (IMAGEFORMAT)image_info->format;
    imgData.img.width = image_info->width ;
    imgData.img.height = image_info->height;
    imgData.img.size = image_info->size; // Get image info size;
    imgData.img.data.reset((uint8_t*)data_ptr, hiai::Graph::ReleaseDataBuffer);
    image_handle->v_img.push_back(imgData);
    return std::static_pointer_cast<void>(image_handle);
}

/**
* @brief: check if the image_handle is a sentinel image
* @[in]: image_handle, the image to check
* @[return]: bool, true if the image is a sentinel image
*/
bool isSentinelImage(const std::shared_ptr<BatchImageParaWithScaleT> imageHandle) {
    // 指针不为空并且标志位是-1
    if (imageHandle && (int)imageHandle->b_info.batch_ID == -1) {
        return true;
    }
    return false;
}

/**
* @brief: replace '/' to '_'  in name
* @[in]: name, the name to replace
*/
void GetOutputName(std::string& name){
    while (true) {
        std::string::size_type pos = name.find("/");
        if (pos != std::string::npos) {
            name.replace(pos, 1, "_");
        } else {
            break;
        }
    }
}

/**
* @brief: create folder to store the detection results
* the folder name on the host will be "result_files/enginename"
*/
HIAI_StatusT CreateFolder(std::string folderPath, mode_t mode) {
    int folder_exist = access(folderPath.c_str(), W_OK);
    if (-1 == folder_exist) {
        if (mkdir(folderPath.c_str(), mode) == -1) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "Failed to create folder %s.", folderPath.c_str());
            return HIAI_ERROR;
        }
    }
    return HIAI_OK;
}

HIAI_REGISTER_DATA_TYPE("EngineTransT", EngineTransT);
HIAI_REGISTER_DATA_TYPE("OutputT", OutputT);
HIAI_REGISTER_DATA_TYPE("ScaleInfoT", ScaleInfoT);
HIAI_REGISTER_DATA_TYPE("NewImageParaT", NewImageParaT);
HIAI_REGISTER_DATA_TYPE("BatchImageParaWithScaleT", BatchImageParaWithScaleT);
HIAI_REGISTER_SERIALIZE_FUNC("EvbImageInfo", EvbImageInfo, GetEvbImageInfoSearPtr, GetEvbImageInfoDearPtr);


std::vector<std::string> split(const std::string& s, char delimiter){
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
      tokens.push_back(token);
    }
    return tokens;
}