#include <opencv2/opencv.hpp>
#include <iostream>
#include <cmath>
#include <string>

using namespace cv;
using namespace std;

Mat backprojection(Mat sinogram);
Mat Ramp(Mat sinogram);

int main(int argc, char** argv){

    if (argc < 2) {
        cout << "사용법: " << argv[0] << " <CT Tomography 이미지 경로>" << endl;
        return 1;
    }

    /*(1) radon transform으로 얻은 sinogram.jpg 이미지 등록*/ 
    string input_path = "result/sinogram.jpg";
    cout<<input_path<<" 파일을 받았습니다..."<<endl;

    Mat sinogram=imread(input_path,IMREAD_GRAYSCALE);                       
    // radon transform 단계에서 sinogram을 grayscale로 저장했기 때문에 Grayscale로 이미지를 받아온다.
    //BRG, HSV 등으로 픽셀 값이 표현되지 않기 때문에 이미지를 1채널(Gray scale tone)으로 처리할 수 있어 편리함.
    sinogram.convertTo(sinogram, CV_64FC1);
    // back projection 단계에서 Mat 변수의 원소들을 double 크기로 처리할 것이기 때문에 64FC1(64비트, double, 1채널)로 변환해준다.

    /*original image가 sinogram이랑 크기 차이가 있을 때, 결과 이미지에서 그 차이만큼 여백이 생기는 오류가 있으므로 original 크기를 결과 이미지에 적용할 수 있게 한다.*/
    Mat original=imread(argv[1],IMREAD_GRAYSCALE);  
    original.convertTo(original, CV_64FC1);
    
    /*(2) Back projection 전 sinogram에 filter 처리*/
    Mat filtering_image=Ramp(sinogram); 
    
    /*(3) Back projection 함수 호출*/
    Mat sample=backprojection(filtering_image);

    /*(4) 저장된 sample의 각 픽셀은 모든 각도 이미지의 gray scale 값이 누적돼서 저장되기 때문에 다시 0-255 gray scale로 표준화해준다.*/
    Mat normalized;
    normalize(sample, normalized, 0, 255, NORM_MINMAX, CV_64FC1);

    /*(5) 결과 이미지 저장*/
    Rect roi(0, 0, original.cols, original.rows);
    Mat cropped = normalized(roi);

    string result_path = "result/fbp.jpg";
    
    imwrite(result_path, cropped); 
    cout<<"Filtered back projection한 결과가 "<<result_path<<" 로 저장되었습니다."<<endl;
}

/*backprojection 함수*/
Mat backprojection(Mat sinogram) {
    int height = sinogram.size().height;    // intensity(L)의 범위
    int width = sinogram.size().width;      // = 360(2π)
    Mat sample(height, height, CV_64FC1, Scalar(0));    // backprojection 결과의 value를 저장할 Mat 행렬, 복원 이미지의 크기를 sinogram의 너비를 가진 정사각형으로 생성한다. (*)

    double weight = CV_PI / width;         // 각 픽셀의 투사 각도에 대한 간격을 설정한다, [ π(고정값) / 2π(signogram으로부터) ]
    for (int x = 0; x < height; x++) {      // for 함수를 통해 각각의 복원 이미지 픽셀을 계산한다. 
        
        for (int y = 0; y < height; y++) {  
        // x, y 는 복원 이미지(result)에서의 위치 좌표를 각각 의미한다. 35 line(*) 에서 복원 이미지의 크기를 sinogram 너비를 가진 정사각형으로 설정했기 때문에, x,y의 범위는 sinogram의 넓이까지 허용된다.
            
            for (int angle = 0; angle < width; angle++) {
                double intensity = (x - 0.5 * height) * cos(weight * angle) + (y - 0.5 * height) * sin(weight * angle) + 0.5 * height; 
                // cos(weight * angle), sin(weight * angle) : 투사되는 angle에 따라 해당 픽셀의 위치를 계산한다[inverse radon transform]
                // (x - 0.5 * height), (y - 0.5 * height) : 복원 이미지에서 x, y의 이미지는 sinogram의 너비의 반으로 이동한 값으로 보정된다.

                if (intensity >= 0 && intensity < height)
                    sample.at<double>(x, y) += sinogram.at<double>(intensity, angle);
                // 계산된 intensity가 복원 이미지의 유효한 범위(정해진 사이즈) 안에 있는 경우에만 최종 결과에 합성시킨다.
            }
            sample.at<double>(x, y) = max(0.0, sample.at<double>(x, y));
            // intensity 계산 중에 음수 값이 발생할 수 있어서(노이즈, 투사의 부정확성, 계산 오차 등에 의한 불안정한 결과) 그러한 음수로 표현되는 픽셀이 제외되도록 조정
        }
        
    }


    // 결과 이미지 상하좌우 반전
    rotate(sample, sample, ROTATE_90_CLOCKWISE);
    rotate(sample, sample, ROTATE_90_CLOCKWISE);
    rotate(sample, sample, ROTATE_90_CLOCKWISE);

    return sample;
}

Mat Ramp(Mat sinogram)
{
    Mat my_filtering;
    transpose(sinogram, my_filtering);
    // 입력된 signogram을 transpose하여 세로 방향의 투사 각도를 가로 방향으로 바꾼다.

    Mat DFT[2] = {my_filtering, Mat::zeros(my_filtering.size(), CV_64FC1)};

    Mat calculating;       // DFT(Discrete fourier transform) 계산을 위한 입력 매트릭스
    merge(DFT, 2, calculating);
    dft(calculating, calculating, DFT_ROWS | DFT_COMPLEX_OUTPUT, 0);    // Opencv의 dft 계산 함수 활용
    split(calculating, DFT);

    unsigned int x, y;
    for (x = 0; x < DFT[0].size().height; x++)
    {
        for (y = 0; y < DFT[0].size().width; y++)
        {
            // Ramp Filter
            double frequency = (y + DFT[0].size().width / 2) % DFT[0].size().width;     // 주파수 값 계산
            double ramp = abs(frequency - DFT[0].size().width / 2.0) / (DFT[0].size().width / 2.0);    
            // 주파수 값과 배열의 절반 크기 간의 차이를 구한 뒤, 배열의 절반 크기로 나누는 방식을 사용한다.
            // 이를 통해 주파수 값이 배열에서 멀어질 수록 Ramp 값이 감소하게 된다.
            DFT[0].at<double>(x, y) *= ramp;    // 실수
            DFT[1].at<double>(x, y) *= ramp;    // 허수
            // 계산된 Ramp 값을 DFT 배열의 해당 요소에 곱하므로써 고주파 성분을 강조하는 효과를 얻는다.
        }
    }

    merge(DFT, 2, calculating);
    dft(calculating, my_filtering, DFT_INVERSE | DFT_ROWS | DFT_REAL_OUTPUT, 0);

    transpose(my_filtering, my_filtering);
    // 재 transpose (원래 방향으로 복구)

    return my_filtering;
}