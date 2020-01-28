本案例适用于MindStudio 1.31以上版本；

本案例从Atlas200DK配套使用的树莓派摄像头读入数据。

可以通过如下步骤复现该案例：
1. 在MindStudio创建一个新的应用工程，选择Custom工程，也就是创建一个自定义空工程；
2. 用本案例中的Custom.cpp和Custom.h替换你新建的工程中的文件；你可以通过beyondcompare查看有哪些修改和代码新增；
3. 参考案例源码中的graph.config在graph.config中插入aiconfig元素；
4. 在CMakeList.txt中如下一句中加入media_mini
    target_link_libraries(Host matrixdaemon hiai_common  media_mini)
5. 编译运行；查看日志，运行成功应该有如下内容（只是把获取到帧数打印出来而已）：
    [INFO] APP(15235,workspace_mind_studio_faceCameraInputandPreprocess):2020-01-28-09:58:09.782.347 (15238) got frame, count= 64,[/home/ascend/AscendProjects/faceCameraInputandPreprocess/src/Custom/Custom.cpp:187:DoCapProcess], Msg: running ok
[INFO] APP(15235,workspace_mind_studio_faceCameraInputandPreprocess):2020-01-28-09:58:09.974.324 (15238) got frame, count= 65,[/home/ascend/AscendProjects/faceCameraInputandPreprocess/src/Custom/Custom.cpp:187:DoCapProcess], Msg: running ok
[INFO] APP(15235,workspace_mind_studio_faceCameraInputandPreprocess):2020-01-28-09:58:10.182.325 (15238) got frame, count= 66,[/home/ascend/AscendProjects/faceCameraInputandPreprocess/src/Custom/Custom.cpp:187:DoCapProcess], Msg: running ok
[INFO] APP(15235,workspace_mind_studio_faceCameraInputandPreprocess):2020-01-28-09:58:10.374.329 (15238) got frame, count= 67,[/home/ascend/AscendProjects/faceCameraInputandPreprocess/src/Custom/Custom.cpp:187:DoCapProcess], Msg: running ok
[INFO] APP(15235,wo

    