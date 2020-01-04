/**
* @file SaveFilePostProcess.cpp
*
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include <hiaiengine/log.h>
#include <vector>
#include <unistd.h>
#include <thread>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <stdlib.h>
#include <sys/stat.h>
#include <sstream>
#include <fcntl.h>
#include "SaveFilePostProcess.h"



HIAI_StatusT SaveFilePostProcess::Init(const hiai::AIConfig& config, const  std::vector<hiai::AIModelDescription>& model_desc) {
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[SaveFilePostProcess] Start init!");

    frameIdToName[0] = "2008_000014";
    frameIdToName[1] = "2008_000159";
    frameIdToName[2] = "2010_004236";

    uint32_t graphId = Engine::GetGraphId();
    std::shared_ptr<Graph> graph = Graph::GetInstance(graphId);
    if (graph == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[SaveFilePostProcess] Fail to get the graph id.");
        return HIAI_ERROR;
    }
    std::ostringstream deviceId;
    deviceId << graph->GetDeviceID();
    string deviceDir = RESULT_FOLDER + "/" + deviceId.str();
    storePath = deviceDir + "/" + ENGINE_NAME;
    if (HIAI_OK != CreateFolder(RESULT_FOLDER, PERMISSION)) {
        return HIAI_ERROR;
    }
    if (HIAI_OK != CreateFolder(deviceDir, PERMISSION)) {
        return HIAI_ERROR;
    }
    if (HIAI_OK != CreateFolder(storePath, PERMISSION)) {
        return HIAI_ERROR;
    }

    // according to the index, choose the class from ables.json.
    Json::Reader reader;
    std::ifstream ifs("labels.json");//open file

    if(!reader.parse(ifs, root_)){
       // fail to parse
       cout << "parse labels json file failed. file=" << "./labels.json";
    }
    else{
       // print one label for testing.
       std::cout<<root_["1"].asString()<<endl;
       cout << "parse labels json file successfully.";
    }


    // get the config value you set in the graph.config
    for (int index = 0; index < config.items_size(); ++index) {
        const ::hiai::AIConfigItem& item = config.items(index);
        if (item.name() == "confidence") {
            confidence = atof(item.value().c_str());
        }
    }

    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[SaveFilePostProcess] End init!");
    return HIAI_OK;
}

HIAI_IMPL_ENGINE_PROCESS("SaveFilePostProcess", SaveFilePostProcess, INPUT_SIZE)
{
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[SaveFilePostProcess] Start process!");
    if (arg0 == nullptr) {
        HIAI_ENGINE_LOG(HIAI_IDE_WARNING, "[SaveFilePostProcess] The arg0 is null.");
        return HIAI_ERROR;
    }
    std::shared_ptr<EngineTransT> tran = std::static_pointer_cast<EngineTransT>(arg0);
    //add sentinel image for showing this data in dataset are all sent, this is last step.
    BatchImageParaWithScaleT imageHandle = {tran->b_info, tran->v_img};

    if (isSentinelImage(std::make_shared<BatchImageParaWithScaleT>(imageHandle))) {
        HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[SaveFilePostProcess] Send sentinel image, process over.");
        std::shared_ptr<std::string> result(new std::string);
        HIAI_StatusT hiaiRet = HIAI_OK;
        do {
            hiaiRet = SendData(0, "string", std::static_pointer_cast<void>(result));
            if (HIAI_QUEUE_FULL == hiaiRet) {
                HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[SaveFilePostProcess] The queue is full, sleep 200ms");
                usleep(SEND_DATA_INTERVAL_MS);
            }
        } while (hiaiRet == HIAI_QUEUE_FULL);

        if (HIAI_OK != hiaiRet) {
            HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[SaveFilePostProcess] Failed to send data, error code: %d", hiaiRet);
        }
        return hiaiRet;
    }
    if (!tran->status) {
        HIAI_ENGINE_LOG(HIAI_IDE_ERROR, tran->msg.c_str());
        return HIAI_ERROR;
    }
    std::vector<OutputT> outputDataVec = tran->output_data_vec;
    for (unsigned int ind = 0; ind < tran->b_info.batch_size; ind++) {
        int frameId = (int)tran->b_info.frame_ID[ind];
        if (frameId == -1) {
            HIAI_ENGINE_LOG(HIAI_IDE_WARNING, "[SaveFilePostProcess] There is no image result for index %d.", ind);
            continue;
        }

        std::string prefix = storePath  + "/" + frameIdToName[frameId];
        for (unsigned int i = 0 ; i < outputDataVec.size(); ++i) {
            OutputT out = outputDataVec[i];
            if (out.size <= 0) {
                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[SaveFilePostProcess] The OutPutT size(%d) less than 0.", out.size);
                return HIAI_ERROR;
            }
            std::string name(out.name);

//            cout << "name "  << out.name << " frameID: " << frameId << " :::: " << frameIdToName[frameId] << endl;

            GetOutputName(name);
//            std::string outFileName = prefix + "_" + name + ".bin";
//            int fd = open(outFileName.c_str(), O_CREAT| O_WRONLY, FIlE_PERMISSION);
//            if (fd == -1) {
//                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[SaveFilePostProcess] Failed to open file %s.", outFileName.c_str());
//                return HIAI_ERROR;
//            }
//            int oneResultSize = out.size / tran->b_info.max_batch_size;
//            int ret = write(fd, out.data.get() + oneResultSize * ind, oneResultSize);
//            if(ret == -1){
//                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[SaveFilePostProcess] Failed to write data to file.");
//                ret = close(fd);
//                if(ret == -1){
//                    HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[SaveFilePostProcess] Failed to close file.");
//                }
//                return HIAI_ERROR;
//            }
//
//            ret = close(fd);
//            if(ret == -1){
//                HIAI_ENGINE_LOG(HIAI_IDE_ERROR, "[SaveFilePostProcess] Failed to close file.");
//                return HIAI_ERROR;
//            }

            // according the model's output shape and dtype to get the result of inference.
            // you can view the model by "tools->viewModle"
            // about the model, you can see that the result is [200, 7, 1, 1] and [1,1,1,1]

            // from the result you can see there is two results:
            //    for the first result [200, 7, 1, 1]:
            //      the first column is the index of the input image in the batch;
            //      the second column is the classes index in labels file;
            //      the 3th column is the confidence;
            //      the (3-6) coloumns is the box.
            //    for the second reslut [1,1,1,1]:
            //      it is the boxes number.

            /*
            outputDataVec[0]:
            0  3  0.991211  -0.00744629  0.0736084  0.407227  0.568848
            0  1  0.982422  0.797852  0.0163574  0.998047  0.757812
            0  4  0.775391  0.240479  0.360352  0.813477  0.999023
            0  1  0.740723  0.274658  0.0690308  0.356201  0.21655
            ...

            outputDataVec[1]:
            200
            */

            // get the result according the confidence you set.
            float *res =  reinterpret_cast<float *>(out.data.get());
            cout << "outputDataVec[" << i << "]:"<< endl;

            float confidence_tmp = 0;
            vector<BBoxWithLable> bboxResults;
            BBoxWithLable tmpBBox;

            // get original image;
            string originalName = "./" + frameIdToName[frameId] + ".jpg";
            cout << "originalName:" << originalName << endl;


            Mat mat = imread(originalName, CV_LOAD_IMAGE_COLOR);
            int originalWidth = mat.cols;
            int originalHeight = mat.rows;


            if (i == 0){
                for (int k =0; k< 200; k++){
                  for (int m = 0; m < 7; m++) {
                    float tmp = res[k * 7 + m];
                    if(m == 1){
                        int tmpInt = int(tmp);
                        tmpBBox.labels = root_[to_string(tmpInt)].asString();
                        //tmpBBox.labels = "AAAAAAAAAAAAAA";
                    }
                    if(m == 2){
                        tmpBBox.confidence = tmp;
                    }
                    if(m == 3){
                        tmp = tmp>=0 ? tmp: 0;
                        tmpBBox.leftTop.x = (tmp * originalWidth);


                    }
                    if(m == 4){
                        tmp = tmp>=0 ? tmp: 0;
                        tmpBBox.leftTop.y = tmp* originalHeight;
                    }
                    if(m == 5){
                        tmp = tmp>=0 ? tmp: 0;
                        tmpBBox.rightDown.x = tmp * originalWidth;
                    }
                    if(m == 6){
                        tmp = tmp>=0 ? tmp: 0;
                        tmpBBox.rightDown.y = tmp*originalHeight;
                    }

                  }

                  if (tmpBBox.confidence >= confidence){
                    tmpBBox.labels = tmpBBox.labels + " " + to_string(tmpBBox.confidence);
                    bboxResults.push_back(tmpBBox);
                  }
                  else{
                    break;
                  }
                  cout << endl;
                }

                cout << endl;
            }
            else{
                cout << res[0] << endl;
                continue;
            }


            // draw rectangle and put text

            for (int n=0; n<bboxResults.size(); n++){
                cout << "bbbox:" << bboxResults[n].leftTop.x << " " << bboxResults[n].leftTop.y << " " << bboxResults[n].rightDown.x << " " <<  bboxResults[n].rightDown.y << endl;

                cout << "size:" << originalWidth << " " << originalHeight << endl;


                rectangle(mat, bboxResults[n].leftTop, bboxResults[n].rightDown, Scalar(0, 255, 255), 1, 8, 0);
                Point textPoint(bboxResults[n].leftTop.x+2, bboxResults[n].leftTop.y + 20);
                putText(mat,bboxResults[n].labels, textPoint, cv::FONT_HERSHEY_COMPLEX, 0.5, cv::Scalar(0, 0, 255), 1, 8, 0);

            }



            std::string outFileName = prefix + "_" + name + ".jpg";

            cout << "outFileName:" << outFileName << endl;
            imwrite(outFileName,mat);

            HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[SaveFilePostProcess] the result file: %s", outFileName.c_str());


        }
    }
    HIAI_ENGINE_LOG(HIAI_IDE_INFO, "[SaveFilePostProcess] End process!");
    return HIAI_OK;
}
