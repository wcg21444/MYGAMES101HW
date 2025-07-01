#pragma once
#include <json/json.h>
#include <iostream>
#include <fstream>

namespace Settings
{
    inline int spp;    // 每像素采样数
    inline int width;  // 图像宽度
    inline int height; // 图像高度
    inline int n_thrd; // 线程数

    // 从JSON字符串反序列化
    inline void loadSettings(const std::string &filepath)
    {
        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        std::ifstream file(filepath);
        std::string parseErrors;

        // 检查文件是否打开成功
        if (!file.is_open())
        {
            std::cerr << "错误：无法打开文件 " << filepath << std::endl;
            exit(1);
        }

        // 解析JSON
        if (!Json::parseFromStream(readerBuilder, file, &root, &parseErrors))
        {
            std::cerr << "JSON解析错误: " << parseErrors << std::endl;
            exit(1);
        }
        spp = root["spp"].asInt();
        width = root["width"].asInt();
        height = root["height"].asInt();
        n_thrd = root["n_thrd"].asInt();
    }
}