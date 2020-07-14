// VideoWatermarking.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <stdbool.h>
#include <math.h>

#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <windows.h>

#pragma execution_character_set( "utf-8" )

using namespace std;
namespace bpo = boost::program_options;

cv::Mat addWatermark(cv::Mat &src, string &text, double &position);
cv::Mat addWatermarkRGB(cv::Mat &src, string &text, double& position, bool allChannel);
cv::Mat addWatermarkYUV(cv::Mat& src, string& text, double& position);
cv::Mat getWatermark(cv::Mat &src);
vector<cv::Mat> getWatermarkRGB(cv::Mat &src);
vector<cv::Mat> getWatermarkYUV(cv::Mat &src);
cv::Mat getTransposeImage(const cv::Mat& input);

int main(int argc, char** argv)
{
    SetConsoleOutputCP(65001);
    bpo::options_description description("Options");
    description.add_options()
        ("input,i", bpo::value<string>(), "输入的文件 Example: video.mp4")
        ("output,o", bpo::value<string>()->default_value("output.avi"), "输出的文件 Example: output.avi")
        ("watermark,w", bpo::value<string>()->default_value("Copyright"), "水印内容")
        ("dump,d", bpo::value<bool>()->default_value(false), "输出JPG帧序列")
        ("position,p", bpo::value<double>()->default_value(0.2222), "水印位置，默认 0.2")
        ("frame,f", bpo::value<int>(), "指定处理帧数量")
        ("yuv,v", "YUV模式")
        ("mode,m", bpo::value<string>(), "运行模式[Decode/Encode]") // Encode / Decode
        ("help,H", "显示帮助")
        ;

    bpo::variables_map vm;
    try {
        bpo::store(bpo::parse_command_line(argc, argv, description), vm);
        bpo::notify(vm);
    }
    catch (...) {
        fprintf(stderr, "输入了未定义的参数\n");
        return 1;
    }

    if (vm.count("help")) {
        std::cout << description << std::endl;
        return 1;
    }

    if (vm.count("input")) {
        string file = vm["input"].as<std::string>();

        if (vm.count("mode"))
        {
            if (vm.count("yuv")) {
                fprintf(stderr, "YUV模式\n");
            }
            else {
                fprintf(stderr, "RGB模式\n");
            }
            string mode = vm["mode"].as<std::string>();
            if (boost::iequals(mode, "decode")) {
                fprintf(stderr, "解码模式\n");
                cv::Mat src;

                // 加载图片
                src = cv::imread(file, cv::IMREAD_COLOR);
                vector<cv::Mat> result;
                if (vm.count("yuv")) {
                    result = getWatermarkYUV(src);
                }
                else {
                    result = getWatermarkRGB(src);
                }

                src.release();

                string outfile = "decoded_";
                for (int i = 0; i < result.size(); i++) {
                    cv::imwrite(outfile + std::to_string(i) + ".jpg", result[i]);
                    fprintf(stderr, "文件已保存%s\n", outfile);
                }
                return 0;
            }
            else {
                fprintf(stderr, "编码模式\n");

                cv::VideoCapture inputVideo(file);
                if (!inputVideo.isOpened()) {
                    fprintf(stderr, "视频读取失败%s\n", file);
                    return 1;
                }

                int totalFrameCount = inputVideo.get(cv::CAP_PROP_FRAME_COUNT), currentFrameCount = 0;

                string outPath = "output.avi";
                if (vm.count("output")) {
                    outPath = vm["output"].as<std::string>();
                }
                cv::VideoWriter outputVideo;
                outputVideo.open(outPath, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), inputVideo.get(cv::CAP_PROP_FPS),
                    cv::Size(inputVideo.get(cv::CAP_PROP_FRAME_WIDTH), inputVideo.get(cv::CAP_PROP_FRAME_HEIGHT)), true);

                if (!outputVideo.isOpened())
                {
                    fprintf(stderr, "无法写入视频\n");
                    return -1;
                }

                cv::Mat frame;
                string text = "Copyright";
                if(vm.count("watermark")) {
                    text = vm["watermark"].as<std::string>();
                }

                bool dumpFrame = false;
                if (vm.count("dump")) {
                    dumpFrame = vm["dump"].as<bool>();
                }
                
                double position = 0.2222;
                if (vm.count("position")) {
                    position = vm["position"].as<double>();
                }

                bool limitFrame = false;
                int limitFrameCount = 0;
                if (vm.count("frame")) {
                    limitFrame = true;
                    limitFrameCount = vm["frame"].as<int>();
                }


                while (1) {
                    
                    inputVideo >> frame;
                    // 视频结束后停止
                    if (frame.empty())
                        break;

                    cv::Mat result;
                    if (vm.count("yuv")) {
                        result = addWatermarkYUV(frame, text, position);
                    }
                    else {
                        result = addWatermarkRGB(frame, text, position, true);
                    }
                    outputVideo << result;
                    
                    if (dumpFrame) {
                        string outfile = "frame" + std::to_string(currentFrameCount) + ".jpg";
                        cv::imwrite(outfile, result);
                        fprintf(stderr, "文件已保存%s\n", outfile);
                    }

                    fprintf(stderr, "Frame %d / %d\r", currentFrameCount, totalFrameCount);
                    
                    currentFrameCount++;
                    if (currentFrameCount >= totalFrameCount) break;
                    if (limitFrame && currentFrameCount >= limitFrameCount) break;
                }

                inputVideo.release();
                outputVideo.release();

                cv::destroyAllWindows();

                return 0;
            }
        }
        else {
            fprintf(stderr, "没有指定处理模式\n");
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "没有指定输入文件\n");
        return 1;
    }
}



cv::Mat addWatermark(cv::Mat &src, string &text, double& position) {
    // 寻找最适合的DFT大小
    int m = cv::getOptimalDFTSize(src.rows);
    int n = cv::getOptimalDFTSize(src.cols);

    cv::Mat padded; // 扩充后的图像变量
    cv::Mat magnitudeImage;

    // 根据照片大小设置水印大小宽度
    double textSize = 0.0;
    int textWidth = 0;

    int minImgSize = src.rows > src.cols ? src.cols : src.rows;

    if (minImgSize < 150)
    {
        textSize = 1.0;
        textWidth = 1;
    }
    else if (minImgSize >= 150 && minImgSize < 300)
    {
        textSize = 1.5;
        textWidth = 2;
    }
    else if (minImgSize >= 300 && minImgSize < 400)
    {
        textSize = 2.5;
        textWidth = 3;
    }
    else if (minImgSize >= 400 && minImgSize < 650)
    {
        textSize = 5.0;
        textWidth = 5;
    }
    else if (minImgSize >= 650 && minImgSize < 1000)
    {
        textSize = 6.0;
        textWidth = 6;
    }
    else if (minImgSize >= 1000)
    {
        textSize = 7.5;;
        textWidth = 7;
    }

    cv::copyMakeBorder(src, padded, 0, m - src.rows, 0, n - src.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));

    // 复制一份作为存放结果,转换为float型
    cv::Mat planes[] = { cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(),CV_32F) };
    cv::Mat complete;
    // 合并成多个通道的图，第一个通道为刚才复制的目标，第二个为0
    cv::merge(planes, 2, complete);

    // 离散傅里叶变换
    cv::dft(complete, complete);


    double minv = 0.0, maxv = 0.0;
    double* minp = &minv;
    double* maxp = &maxv;
    // 计算最大值和最小值
    cv::minMaxIdx(complete, minp, maxp);
    //fprintf(stderr, "minv %.6f maxv %.6f\n", minv, maxv);

    // 计算平均值，决定水印强度
    int meanvalue = cv::mean(complete)[0];
    int watermark_scale = 0;
    //fprintf(stderr, "mean %d\n", meanvalue);
    if (meanvalue > 128)
    {
        watermark_scale = -log(abs(minv));
    }
    else
    {
        watermark_scale = log(abs(maxv));
    }
    //fprintf(stderr, "watermark_scale %.6f\n", watermark_scale);
    auto font = cv::FONT_HERSHEY_PLAIN;
    cv::Scalar color(watermark_scale, watermark_scale, watermark_scale);
    cv::Point pos(src.cols * position, src.rows * position);
    cv::putText(complete, text, pos, cv::FONT_HERSHEY_PLAIN, textSize, color, textWidth);
    cv::flip(complete, complete, -1);
    cv::putText(complete, text, pos, cv::FONT_HERSHEY_PLAIN, textSize, color, textWidth);
    cv::flip(complete, complete, -1);

    // 逆傅里叶变换
    idft(complete, complete);

    split(complete, planes);

    magnitude(planes[0], planes[1], planes[0]);
    cv::Mat result = planes[0];
    result = result(cv::Rect(0, 0, src.cols, src.rows));
    // 标准化
    normalize(result, result, 0, 1, cv::NORM_MINMAX);

    padded.release();
    magnitudeImage.release();
    planes[1].release();
    cv::Mat out;
    // 转回u_char型的Mat
    result.convertTo(out, CV_8U, 255.0);
    result.release();
    return out;
}

// 给RGB通道叠加水印
cv::Mat addWatermarkRGB(cv::Mat& src, string& text, double& position, bool allChannel)
{

    int col = src.cols;
    int row = src.rows;

    vector<cv::Mat> src_channels;
    cv::Mat target;
    vector<cv::Mat> target_channels;
    if (row > col) {
        cv::split(getTransposeImage(src), src_channels);
    }
    else {
        cv::split(src, src_channels);
    }
    
    // 分别处理三个通道
    for (int i = 0; i < src_channels.size(); i++) {
        if (1) {
            target_channels.push_back(addWatermark(src_channels[i], text, position));
        }
        else {
            target_channels.push_back(src_channels[i]);
        }
    }
    cv::merge(target_channels, target);
    if (row > col) {
        return getTransposeImage(target);
    }
    return target;
}

// 给YUV的Y通道加水印
cv::Mat addWatermarkYUV(cv::Mat& src, string& text, double& position)
{
    vector<cv::Mat> src_channels;
    cv::Mat srcYUV;
    cv::Mat targetYUV;
    cv::Mat targetBGR;
    vector<cv::Mat> target_channels;

    cv::cvtColor(src, srcYUV, cv::COLOR_BGR2YCrCb);

    cv::split(srcYUV, src_channels);
    // 分别处理三个通道
    for (int i = 0; i < src_channels.size(); i++) {
        if (i == 0) {
            target_channels.push_back(addWatermark(src_channels[i], text, position));
        }
        else {
            target_channels.push_back(src_channels[i]);
        }
    }
    cv::merge(target_channels, targetYUV);
    cv::cvtColor(targetYUV, targetBGR, cv::COLOR_YCrCb2BGR);
    return targetBGR;
}

cv::Mat getWatermark(cv::Mat& src) {
    int m = cv::getOptimalDFTSize(src.rows);
    int n = cv::getOptimalDFTSize(src.cols);

    cv::Mat padded; // 扩充后的图像变量
    cv::Mat magnitudeImage;

    double textSize = 1.5;
    int textWidth = 2;

    cv::copyMakeBorder(src, padded, 0, m - src.rows, 0, n - src.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));

    //复制一份作为存放结果,转换为float型
    cv::Mat planes[] = { cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(),CV_32F) };
    cv::Mat complete;
    //合并成多个通道的图，第一个通道为刚才复制的目标，第二个为0
    cv::merge(planes, 2, complete);

    //离散傅里叶变换
    cv::dft(complete, complete);

    //分离实部和虚部
    split(complete, planes);
    magnitude(planes[0], planes[1], planes[0]);
    magnitudeImage = planes[0];

    //对数运算
    magnitudeImage += cv::Scalar::all(1);
    log(magnitudeImage, magnitudeImage);

    //裁剪
    magnitudeImage = magnitudeImage(cv::Rect(0, 0, src.cols, src.rows));

    //标准化到0~1
    normalize(magnitudeImage, magnitudeImage, 0, 1, cv::NORM_MINMAX);

    int cx = magnitudeImage.cols / 2;
    int cy = magnitudeImage.rows / 2;
    cv::Mat temp;
    cv::Mat q0(magnitudeImage, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(magnitudeImage, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(magnitudeImage, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(magnitudeImage, cv::Rect(cx, cy, cx, cy));
    q0.copyTo(temp);
    q3.copyTo(q0);
    temp.copyTo(q3);
    q1.copyTo(temp);
    q2.copyTo(q1);
    temp.copyTo(q2);

    cv::Mat out;
    // 转回u_char型的Mat
    magnitudeImage.convertTo(out, CV_8UC3, 255.0);
    magnitudeImage.release();
    return out;
}

vector<cv::Mat> getWatermarkRGB(cv::Mat& src)
{
    vector<cv::Mat> src_channels;
    cv::Mat target;
    vector<cv::Mat> target_channels;
    cv::split(src, src_channels);
    // 分别处理三个通道
    for (int i = 0; i < src_channels.size(); i++) {
        target_channels.push_back(getWatermark(src_channels[i]));
    }
    return target_channels;
}

vector<cv::Mat> getWatermarkYUV(cv::Mat& src)
{
    vector<cv::Mat> src_channels;
    vector<cv::Mat> target_channels;
    cv::Mat srcYUV;
    cv::cvtColor(src, srcYUV, cv::COLOR_BGR2YCrCb);

    cv::split(srcYUV, src_channels);
    // 分别处理三个通道
    for (int i = 0; i < src_channels.size(); i++) {
        if (i == 0) {
            target_channels.push_back(getWatermark(src_channels[i]));
        }
    }
    return target_channels;
}

cv::Mat getTransposeImage(const cv::Mat& input)
{
    cv::Mat result;
    transpose(input, result);
    return result;
}