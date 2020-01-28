/**
* @file Custom.h
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef CUSTOM_ENGINE_H_
#define CUSTOM_ENGINE_H_

#include <iostream>
#include <string>
#include <dirent.h>
#include <memory>
#include <unistd.h>
#include <vector>
#include <stdint.h>
#include "hiaiengine/engine.h"
#include "hiaiengine/multitype_queue.h"

//#include "common/face_detection_params.h"

extern "C" {
#include "driver/peripheral_api.h"
}


#define INPUT_SIZE 1
#define OUTPUT_SIZE 1


#define CAMERAL_1 (0)
#define CAMERAL_2 (1)

#define CAMERADATASETS_INIT (0)
#define CAMERADATASETS_RUN  (1)
#define CAMERADATASETS_STOP (2)
#define CAMERADATASETS_EXIT (3)


#define PARSEPARAM_FAIL (-1)
#define MAX_VALUESTRING_LENGTH 25


using hiai::Engine;
using namespace std;
using namespace hiai;

// initial value of frameId
const uint32_t kInitFrameId = 0;

const int kMaxBatchSize = 1;

enum CameraOperationCode {
    kCameraOk = 0,
    kCameraNotClosed = -1,
    kCameraOpenFailed = -2,
    kCameraSetPropeptyFailed = -3,
  };

struct CameraDatasetsConfig {
    int fps;
    int channel_id;
    int image_format;
    int resolution_width;
    int resolution_height;
    std::string ToString() const{
        stringstream log_info_stream("");
          log_info_stream << "fps:" << this->fps << ", camera:" << this->channel_id
              << ", image_format:" << this->image_format << ", resolution_width:"
              << this->resolution_width << ", resolution_height:"
              << this->resolution_height;

          return log_info_stream.str();
    }
  };



class Custom : public Engine {
public:

    string IntToString(int value) {
      char msg[MAX_VALUESTRING_LENGTH] = { 0 };
      // MAX_VALUESTRING_LENGTH ensure no error occurred
      sprintf_s(msg, MAX_VALUESTRING_LENGTH, "%d", value);
      string ret = msg;

      return ret;
    }

    void InitConfigParams() {
      params_.insert(pair<string, string>("Channel-1", IntToString(CAMERAL_1)));
      params_.insert(pair<string, string>("Channel-2", IntToString(CAMERAL_2)));
      params_.insert(
          pair<string, string>("YUV420SP", IntToString(CAMERA_IMAGE_YUV420_SP)));
    }


    int CommonParseParam(const string& val) const {
      map<string, string>::const_iterator iter = params_.find(val);
      if (iter != params_.end()) {
        return atoi((iter->second).c_str());
      }

      return PARSEPARAM_FAIL;
    }


    void SplitString(const string& source, vector<string>& tmp,
                                          const string& obj) const {
      string::size_type pos1 = 0;
      string::size_type pos2 = source.find(obj);

      while (string::npos != pos2) {
        tmp.push_back(source.substr(pos1, pos2 - pos1));
        pos1 = pos2 + obj.size();
        pos2 = source.find(obj, pos1);
      }

      if (pos1 != source.length()) {
        tmp.push_back(source.substr(pos1));
      }
    }



    void ParseImageSize(const string& val, int& width,
                                             int& height) const {
      vector < string > tmp;
      SplitString(val, tmp, "x");

      // val is not a format of resolution ratio(*x*),correct should have 2 array
      // in this wrong case,set width and height zero
      if (tmp.size() != 2) {
        width = 0;
        height = 0;
      } else {
        width = atoi(tmp[0].c_str());
        height = atoi(tmp[1].c_str());
      }
    }


    void SetExitFlag(int flag) {
      TLock lock(mutex_);
      exit_flag_ = flag;
    }


    int GetExitFlag() {
      TLock lock(mutex_);
      return exit_flag_;
    }

    Custom() :
        inputQueue(INPUT_SIZE) {

         config_ = nullptr;
         frame_id_ = kInitFrameId;
         exit_flag_ = CAMERADATASETS_INIT;
         InitConfigParams();

        }


    HIAI_StatusT Init(const hiai::AIConfig &config, const std::vector<hiai::AIModelDescription> &modelDesc);


    bool DoCapProcess();

    CameraOperationCode PreCapProcess();


//    shared_ptr<BatchImageParaWithScaleT>  CreateBatchImageParaObj() {
//      shared_ptr < BatchImageParaWithScaleT > pobj = make_shared<
//          BatchImageParaWithScaleT>();
//
//      pobj->b_info.is_first = (frame_id_ == kInitFrameId);
//      pobj->b_info.is_last = false;
//      // handle one batch every time
//      pobj->b_info.batch_size = 1;
//      pobj->b_info.max_batch_size = kMaxBatchSize;
//      pobj->b_info.batch_ID = 0;
//      pobj->b_info.channel_ID = config_->channel_id;
//      pobj->b_info.processor_stream_ID = 0;
//      pobj->b_info.frame_ID.push_back(frame_id_++);
//      pobj->b_info.timestamp.push_back(time(nullptr));
//
//      NewImageParaT img_data;
//      // channel begin from zero
//      img_data.img.channel = 0;
//      img_data.img.format = YUV420SP;
//      img_data.img.width = config_->resolution_width;
//      img_data.img.height = config_->resolution_height;
//      // YUV size in memory is width*height*3/2
//      img_data.img.size = config_->resolution_width * config_->resolution_height * 3 / 2;
//
//      shared_ptr <uint8_t> data(new uint8_t[img_data.img.size],
//                                default_delete<uint8_t[]>());
//      img_data.img.data = data;
//
//      pobj->v_img.push_back(img_data);
//
//      return pobj;
//    }



    /**
    * @ingroup hiaiengine
    * @brief HIAI_DEFINE_PROCESS : reload Engine Process
    * @[in]: define the number of input and output
    */
    HIAI_DEFINE_PROCESS(INPUT_SIZE, OUTPUT_SIZE)
private:

    // Private implementation a member variable, which is used to cache the input queue
    hiai::MultiTypeQueue inputQueue;

    typedef std::unique_lock<std::mutex> TLock;

    std::shared_ptr<CameraDatasetsConfig> config_;

    std::map<std::string, std::string> params_;

    // thread variable to protect exitFlag
    std::mutex mutex_;
    // ret of cameradataset
    int exit_flag_;
    uint32_t frame_id_;

};

#endif // CUSTOM_ENGINE_H_
