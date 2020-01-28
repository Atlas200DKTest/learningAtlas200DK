/**
* @file Custom.cpp
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
#include "Custom.h"
#include "hiaiengine/log.h"
#include "hiaiengine/data_type_reg.h"
#include "hiaiengine/data_type.h"

/**
* @ingroup hiaiengine
* @brief HIAI_DEFINE_PROCESS : implemention of the engine
* @[in]: engine name and the number of input
*/
HIAI_StatusT Custom::Init(const hiai::AIConfig &config, const std::vector<hiai::AIModelDescription> &modelDesc)
{
    //TODO:
    //need to get config items of the camera.
    HIAI_ENGINE_LOG("[CameraDatasets] start init!");
    if (config_ == nullptr) {
    config_ = make_shared<CameraDatasetsConfig>();
    }

    for (int index = 0; index < config.items_size(); ++index) {
    const ::hiai::AIConfigItem& item = config.items(index);
    string name = item.name();
    string value = item.value();

    if (name == "fps") {
      config_->fps = atoi(value.data());
    } else if (name == "image_format") {
      config_->image_format = CommonParseParam(value);
    } else if (name == "data_source") {
      config_->channel_id = CommonParseParam(value);
    } else if (name == "image_size") {
      ParseImageSize(value, config_->resolution_width,
                     config_->resolution_height);
    } else {
      HIAI_ENGINE_LOG("unused config name: %s", name.c_str());
    }
    }

    HIAI_StatusT ret = HIAI_OK;
    bool failed_flag = (config_->image_format == PARSEPARAM_FAIL
      || config_->channel_id == PARSEPARAM_FAIL
      || config_->resolution_width == 0 || config_->resolution_height == 0);

    if (failed_flag) {
    string msg = config_->ToString();
    msg.append(" config data failed");
    HIAI_ENGINE_LOG(msg.data());
    ret = HIAI_ERROR;
    }

    HIAI_ENGINE_LOG("[CameraDatasets] end init!");
    return ret;

}



CameraOperationCode Custom::PreCapProcess() {
  MediaLibInit();

  CameraStatus status = QueryCameraStatus(config_->channel_id);
  if(status != CAMERA_STATUS_CLOSED){
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess.QueryCameraStatus {status:%d} failed.",status);
    return kCameraNotClosed;
  }

  // Open Camera
  int ret = OpenCamera(config_->channel_id);
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess OpenCamera {%d} "
                    "failed.",config_->channel_id);
    return kCameraOpenFailed;
  }

  // set fps
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_FPS,
                          &(config_->fps));
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set fps {fps:%d} "
                    "failed.",config_->fps);
    return kCameraSetPropeptyFailed;
  }

  // set image format
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_IMAGE_FORMAT,
                          &(config_->image_format));
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set image_fromat "
                    "{format:%d} failed.",config_->image_format);
    return kCameraSetPropeptyFailed;
  }

  // set image resolution.
  CameraResolution resolution;
  resolution.width = config_->resolution_width;
  resolution.height = config_->resolution_height;
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_RESOLUTION,
                          &resolution);
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set resolution {width:%d, height:%d } failed.",
                    config_->resolution_width, config_->resolution_height);
    return kCameraSetPropeptyFailed;
  }

  // set work mode
  CameraCapMode mode = CAMERA_CAP_ACTIVE;
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_CAP_MODE, &mode);
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set cap mode {mode:%d}"
                    " failed.",mode);
    return kCameraSetPropeptyFailed;
  }

  return kCameraOk;
}


bool Custom::DoCapProcess() {
  CameraOperationCode ret_code = PreCapProcess();
  if (ret_code == kCameraSetPropeptyFailed) {
    CloseCamera(config_->channel_id);

    HIAI_ENGINE_LOG("[CameraDatasets] DoCapProcess.PreCapProcess failed");
    return false;
  }

  // set procedure is running.
  SetExitFlag(CAMERADATASETS_RUN);

  HIAI_StatusT hiai_ret = HIAI_OK;
  int read_ret = 0;
  int read_size = 0;
  bool read_flag = false;
  int count = 0;
  while (GetExitFlag() == CAMERADATASETS_RUN) {

    hiai::ImageData<u_int8_t> img;
    // channel begin from zero
    img.channel = 0;
    img.format = YUV420SP;
    img.width = config_->resolution_width;
    img.height = config_->resolution_height;
    // YUV size in memory is width*height*3/2
    img.size = config_->resolution_width * config_->resolution_height * 3 / 2;

    shared_ptr <uint8_t> data(new uint8_t[img.size], default_delete<uint8_t[]>());
    img.data = data;

    read_size = (int) img.size;
    uint8_t* pdata = img.data.get();

    // do read frame from camera
    read_ret = ReadFrameFromCamera(config_->channel_id, (void*) pdata,
                                   &read_size);
    // indicates failure when readRet is 1
    read_flag = ((read_ret == 1) && (read_size == (int) img.size));

    if (!read_flag) {
      HIAI_ENGINE_LOG("[CameraDatasets] readFrameFromCamera failed "
                      "{camera:%d, ret:%d, size:%d, expectsize:%d} ",
                      config_->channel_id, read_ret, read_size,
                      (int) img.size);
      break;
    }
    count++;

    HIAI_ENGINE_LOG("got frame, count= %d", count, pdata);
    cout << "got frame, count=" << count  << endl;


  }

  // close camera
  CloseCamera(config_->channel_id);

  if (hiai_ret != HIAI_OK) {
    return false;
  }

  return true;
}

HIAI_IMPL_ENGINE_PROCESS("Custom", Custom, INPUT_SIZE)
{
    //TODO:
	//user code here
	HIAI_ENGINE_LOG("start get data from camera!");
	//begin to capture the photo from camera
	DoCapProcess();

	HIAI_ENGINE_LOG("end get data from camera!");



	//send data
	std::shared_ptr<std::string> resultData(new std::string);
    HIAI_StatusT ret = SendData(0, "string", std::static_pointer_cast<void>(resultData));
    return ret;
}
