#include <iostream>
#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;

int main(int argc, char** argv) {

    if (argc < 2) {
        cout << "사용법: " << argv[0] << " <입력 이미지 경로>" << endl;
        return 1;
    }
    // ⑴ Gray scale 이미지로 촬영한 x-ray 단층 이미지를 입력 받음.
    string input_path = argv[1];
    Mat inputImage = imread(input_path, IMREAD_GRAYSCALE);
    cout<<input_path<<" 를 입력 받았습니다..."<<endl;

    inputImage.convertTo(inputImage, CV_64FC1); // 입력된 이미지를 64-bit double, 1 채널 형식으로 변환
    int angle = 360;
    
    // ⑵ 출력 이미지 생성
    Mat Sinogram = Mat(inputImage.cols, angle, CV_64FC1);

    // (3) 이미지 중심 좌표 계산
    double center = (inputImage.cols / 2.);

    // (4) 좌표 변환 행렬 생성, x-ray 이미지의 센터를 기준으로 회전 이미지 필터가 적용되도록
    double shift0[] = {  1, 0, -center,
                        0, 1, -center,
                        0, 0, 1};
    double shift1[] = {  1, 0, center,
                        0, 1, center,
                        0, 0, 1};
    Mat filter_1 = Mat(3, 3, CV_64FC1, shift0);
    Mat filter_2 = Mat(3, 3, CV_64FC1, shift1);
    Mat filter_scailing;

    // 각도에 따라 Radon 변환을 수행한다.
    double* theta = new double[angle];
    for (int t = 0; t < angle; t++)
    {
        theta[t] = t * CV_PI / angle;                       // 각도를 라디안으로 변환한다.

        double R[] = { cos(theta[t]), sin(theta[t]), 0,     // 가중치(scailing)
                        -sin(theta[t]), cos(theta[t]), 0,
                            0, 0, 1 };
        Mat filter_scailing = Mat(3, 3, CV_64FC1, R);

        // 좌표 변환 행렬 적용하여 회전 이미지 생성
        Mat rotation = filter_2 * filter_scailing * filter_1;
        Mat rotated;
        warpPerspective(inputImage, rotated, rotation, Size(inputImage.cols, inputImage.rows), cv::WARP_INVERSE_MAP);
        
        // 회전된 이미지의 합산을 결과 sinogram에 저장한다.
        for (int j = 0; j < rotated.cols; j++)
        {
            double* p1 = Sinogram.ptr<double>(j);
            for (int i = 0; i < rotated.rows; i++)
            {
                double* p2 = rotated.ptr<double>(i);
                p1[t] += p2[j];
            }
        }
    }

    // sinogram의 이미지를 0~255 범위로 정규화한다.
    Mat radon_normalized;
    normalize(Sinogram, radon_normalized, 0, 255, NORM_MINMAX, CV_8U);


    // sinogram 이미지를 jpg로 저장한다.
    string output_path = "result/sinogram.jpg";
    imwrite(output_path, radon_normalized);

    cout <<output_path<<" 파일로 결과를 저장했습니다.\n";

    delete[] theta;

    return 0;
}
