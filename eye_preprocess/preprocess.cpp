#include "preprocess.h"
#include <opencv2/opencv.hpp>

cv::Mat preprocessEye(const cv::Mat& eyeGray)
{
    cv::Mat proc;

    // 1. Grayscale 변환
    if (eyeGray.channels() == 3)
        cv::cvtColor(eyeGray, proc, cv::COLOR_BGR2GRAY);
    else
        proc = eyeGray.clone();

    // 2. CLAHE (국소 대비 향상) → 어두운 환경에서도 pupil 강조
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(proc, proc);

    // 3. Median Blur → 동공 경계는 살리고 노이즈만 제거
    cv::medianBlur(proc, proc, 5);

    // 4. Adaptive Threshold (조명 변화에 강함)
    cv::adaptiveThreshold(proc, proc, 255,
        cv::ADAPTIVE_THRESH_MEAN_C, // 평균 기반
        cv::THRESH_BINARY_INV,
        19,  // blockSize (홀수) → 주변 영역 크기
        5);  // 상수 C → 낮을수록 더 민감

    // 5. Morphology Close → 작은 흰 점 제거, pupil 검은 원 유지
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(proc, proc, cv::MORPH_CLOSE, kernel);

    return proc; // resize는 빼고 원본 크기 유지
}
