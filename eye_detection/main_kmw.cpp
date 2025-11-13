#include <opencv2/opencv.hpp>
#include <iostream>
using namespace cv;
using std::cout; using std::endl;

static Point2f emaPoint(const Point2f& prev, const Point2f& cur, float alpha = 0.25f) {
    return prev * (1.0f - alpha) + cur * alpha;
}

static bool findPupil(const Mat& eyeGray, Point& pupil, float& radius)
{
    // 1) 전처리
    Mat blurImg; GaussianBlur(eyeGray, blurImg, Size(7, 7), 0);
    // 눈꺼풀/하이라이트 제거를 위해 상위 톤 억제
    Mat eq; equalizeHist(blurImg, eq);
    // 2) 동공은 어두움: Otsu + 반전
    Mat bin;
    threshold(eq, bin, 0, 255, THRESH_BINARY_INV | THRESH_OTSU);

    // 3) 열림 연산으로 잡티 제거
    morphologyEx(bin, bin, MORPH_OPEN, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));

    // 4) 큰 컨투어 중심을 후보로
    std::vector<std::vector<Point>> contours;
    findContours(bin, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        // 실패하면 허프원 시도
        std::vector<Vec3f> circles;
        HoughCircles(blurImg, circles, HOUGH_GRADIENT, 1, eyeGray.rows / 8, 200, 15, eyeGray.rows / 16, eyeGray.rows / 3);
        if (circles.empty()) return false;
        Vec3f c = circles[0];
        pupil = Point(cvRound(c[0]), cvRound(c[1]));
        radius = c[2];
        return true;
    }
    // 가장 큰 컨투어 선택
    size_t idxMax = 0; double maxA = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double a = contourArea(contours[i]);
        if (a > maxA) { maxA = a; idxMax = i; }
    }
    Point2f c; float r;
    minEnclosingCircle(contours[idxMax], c, r);
    pupil = Point(cvRound(c.x), cvRound(c.y));
    radius = r;
    return true;
}

int main()
{
    // 0) 분류기 로드 (OpenCV 설치 경로의 haarcascade 파일 경로를 맞춰주세요)
    // 예: Linux: /usr/share/opencv4/haarcascades/..., Windows: <opencv>/build/etc/haarcascades/...
    std::string face_cascade_path = "haarcascade_frontalface_default.xml";
    std::string eye_cascade_path = "haarcascade_eye_tree_eyeglasses.xml";

    CascadeClassifier faceCasc, eyeCasc;
    if (!faceCasc.load(face_cascade_path) || !eyeCasc.load(eye_cascade_path)) {
        std::cerr << "Failed to load cascades. Check paths.\n";
        return -1;
    }

    VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open camera\n";
        return -1;
    }
    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);

    Point2f emaLeft(-1, -1), emaRight(-1, -1); // EMA 초기화
    while (true) {
        Mat frame; cap >> frame;
        if (frame.empty()) break;

        // === 좌우 반전(거울 모드) ===
        cv::flip(frame, frame, 1);

        Mat gray; cvtColor(frame, gray, COLOR_BGR2GRAY);

        // 1) 얼굴 검출
        std::vector<Rect> faces;
        faceCasc.detectMultiScale(gray, faces, 1.1, 3, 0, Size(120, 120));
        for (const Rect& f : faces) {
            rectangle(frame, f, Scalar(0, 255, 0), 2);

            // 2) 얼굴 ROI에서 눈 검출 (상반부 우선)
            Rect upperFace = Rect(f.x, f.y, f.width, (int)std::round(f.height * 0.6));
            upperFace &= Rect(0, 0, frame.cols, frame.rows);
            Mat faceROI = gray(upperFace);

            std::vector<Rect> eyes;
            eyeCasc.detectMultiScale(faceROI, eyes, 1.1, 2, 0, Size(30, 30));

            // 얼굴 중앙 x (프레임 좌표계)
            const float faceCenterX = f.x + f.width * 0.5f;

            // 한 프레임에서의 결과를 담을 그릇
            bool foundL = false, foundR = false;
            Point2f normL(0, 0), normR(0, 0);
            float radL = 0.f, radR = 0.f;
            Rect eyeRectL, eyeRectR;

            for (const Rect& eInFace : eyes) {
                // 프레임 좌표계의 눈 박스
                Rect eyeRect(eInFace.x + upperFace.x, eInFace.y + upperFace.y, eInFace.width, eInFace.height);
                rectangle(frame, eyeRect, Scalar(255, 200, 0), 2);

                Mat eyeGray = gray(eyeRect).clone();

                // 3) 동공 찾기
                Point pupil; float r = 0;
                bool ok = findPupil(eyeGray, pupil, r);

                // 눈 중심 x
                float eyeCenterX = eyeRect.x + eyeRect.width * 0.5f;
                bool isLeftSide = (eyeCenterX < faceCenterX);

                if (ok) {
                    // 프레임 좌표로 환산된 동공
                    Point pupilInFrame = Point(eyeRect.x + pupil.x, eyeRect.y + pupil.y);
                    circle(frame, pupilInFrame, (int)std::max(2.f, r), Scalar(0, 0, 255), 2);

                    // 눈 ROI 중심 기준 정규화 (-1..1)
                    Point2f centerEye(eyeRect.x + eyeRect.width * 0.5f, eyeRect.y + eyeRect.height * 0.5f);
                    Point2f offset = Point2f((float)pupilInFrame.x, (float)pupilInFrame.y) - centerEye;
                    Point2f norm(offset.x / (eyeRect.width * 0.5f),
                        offset.y / (eyeRect.height * 0.5f));

                    if (isLeftSide) {
                        foundL = true;  normL = norm;  radL = r;  eyeRectL = eyeRect;
                    }
                    else {
                        foundR = true;  normR = norm;  radR = r;  eyeRectR = eyeRect;
                    }
                }
                else {
                    putText(frame, "pupil?", Point(eyeRect.x, eyeRect.y - 8),
                        FONT_HERSHEY_SIMPLEX, 0.5, Scalar(50, 50, 255), 1);
                }
            }

            // 4) 흔들림 감소(EMA) + 라벨 고정 출력
            if (foundL) {
                if (emaLeft.x < -0.5f) emaLeft = normL;
                else                   emaLeft = emaPoint(emaLeft, normL, 0.2f);
                putText(frame, cv::format("L(%.2f, %.2f)", emaLeft.x, emaLeft.y),
                    Point(eyeRectL.x, eyeRectL.y - 8), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 255), 1);
            }
            if (foundR) {
                if (emaRight.x < -0.5f) emaRight = normR;
                else                    emaRight = emaPoint(emaRight, normR, 0.2f);
                putText(frame, cv::format("R(%.2f, %.2f)", emaRight.x, emaRight.y),
                    Point(eyeRectR.x, eyeRectR.y - 8), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 255), 1);
            }

        }

        // 안내 텍스트
        putText(frame, "Press 'q' to quit", Point(20, 30), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);
        imshow("Eye Tracker (OpenCV)", frame);
        char key = (char)waitKey(1);
        if (key == 'q' || key == 27) break;
    }
    return 0;
}
