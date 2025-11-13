#include <opencv2/opencv.hpp>
#include <iostream>
#include "BlinkDetector.h"
using namespace cv;
using std::cout; using std::endl;

static Point2f emaPoint(const Point2f& prev, const Point2f& cur, float alpha = 0.25f) {
    return prev * (1.0f - alpha) + cur * alpha;
}

static bool findPupil(const Mat& eyeGray, Point& pupil, float& radius)
{
    // 1) ��ó��
    Mat blurImg; GaussianBlur(eyeGray, blurImg, Size(7, 7), 0);
    // ����Ǯ/���̶���Ʈ ���Ÿ� ���� ���� �� ����
    Mat eq; equalizeHist(blurImg, eq);
    // 2) ������ ��ο�: Otsu + ����
    Mat bin;
    threshold(eq, bin, 0, 255, THRESH_BINARY_INV | THRESH_OTSU);

    // 3) ���� �������� ��Ƽ ����
    morphologyEx(bin, bin, MORPH_OPEN, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));

    // 4) ū ������ �߽��� �ĺ���
    std::vector<std::vector<Point>> contours;
    findContours(bin, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        // �����ϸ� ������ �õ�
        std::vector<Vec3f> circles;
        HoughCircles(blurImg, circles, HOUGH_GRADIENT, 1, eyeGray.rows / 8, 200, 15, eyeGray.rows / 16, eyeGray.rows / 3);
        if (circles.empty()) return false;
        Vec3f c = circles[0];
        pupil = Point(cvRound(c[0]), cvRound(c[1]));
        radius = c[2];
        return true;
    }
    // ���� ū ������ ����
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
    // 0) �з��� �ε� (OpenCV ��ġ ����� haarcascade ���� ��θ� �����ּ���)
    // ��: Linux: /usr/share/opencv4/haarcascades/..., Windows: <opencv>/build/etc/haarcascades/...
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

    // ���� ���� ������ ���� ��ü ����, �Ķ���ʹ� ������ ��
    BlinkDetector left_eye_detector(5);
    BlinkDetector right_eye_detector(5);

    Point2f emaLeft(-1, -1), emaRight(-1, -1); // EMA �ʱ�ȭ
    while (true) {
        Mat frame; cap >> frame;
        if (frame.empty()) break;

        // === �¿� ����(�ſ� ���) ===
        cv::flip(frame, frame, 1);

        Mat gray; cvtColor(frame, gray, COLOR_BGR2GRAY);

        // 1) �� ����
        std::vector<Rect> faces;
        faceCasc.detectMultiScale(gray, faces, 1.1, 3, 0, Size(120, 120));
        for (const Rect& f : faces) {
            rectangle(frame, f, Scalar(0, 255, 0), 2);

            // 2) �� ROI���� �� ���� (��ݺ� �켱)
            Rect upperFace = Rect(f.x, f.y, f.width, (int)std::round(f.height * 0.6));
            upperFace &= Rect(0, 0, frame.cols, frame.rows);
            Mat faceROI = gray(upperFace);

            std::vector<Rect> eyes;
            eyeCasc.detectMultiScale(faceROI, eyes, 1.1, 2, 0, Size(30, 30));

            // �� �߾� x (������ ��ǥ��)
            const float faceCenterX = f.x + f.width * 0.5f;

            // �� �����ӿ����� ����� ���� �׸�
            bool foundL = false, foundR = false;
            bool isLeftSide = false;
            Point2f normL(0, 0), normR(0, 0);
            float radL = 0.f, radR = 0.f;
            Rect eyeRectL, eyeRectR;

            for (const Rect& eInFace : eyes) {
                // ������ ��ǥ���� �� �ڽ�
                Rect eyeRect(eInFace.x + upperFace.x, eInFace.y + upperFace.y, eInFace.width, eInFace.height);
                rectangle(frame, eyeRect, Scalar(255, 200, 0), 2);

                Mat eyeGray = gray(eyeRect).clone();

                // 3) ���� ã��
                Point pupil; float r = 0;
                bool ok = findPupil(eyeGray, pupil, r);

                // �� �߽� x
                float eyeCenterX = eyeRect.x + eyeRect.width * 0.5f;
                isLeftSide = (eyeCenterX < faceCenterX);   // Left : TRUE / Right : FALSE

                if (ok) {
                    // ������ ��ǥ�� ȯ��� ����
                    Point pupilInFrame = Point(eyeRect.x + pupil.x, eyeRect.y + pupil.y);
                    circle(frame, pupilInFrame, (int)std::max(2.f, r), Scalar(0, 0, 255), 2);

                    // �� ROI �߽� ���� ����ȭ (-1..1)
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

            // 4) ��鸲 ����(EMA) + �� ���� ���
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
            
            if (eyes.size() == 1) {
                if (!isLeftSide) { // ���� �� ����� ó��
                    left_eye_detector.checkBlink(false); // 'ok' ����(true/false)�� ���� ����
                    if (left_eye_detector.isBlinking()) {
                        std::cout << "LEFT CLICK!" << std::endl;
                        putText(frame, "LEFT CLICK!", Point(50, 80), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
                        left_eye_detector.reset();
                    }
                }
                else { // ������ �� ����� ó��
                    right_eye_detector.checkBlink(false); // 'ok' ����(true/false)�� ���� ����
                    if (right_eye_detector.isBlinking()) {
                        std::cout << "RIGHT CLICK!" << std::endl;
                        putText(frame, "RIGHT CLICK!", Point(50, 120), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
                        right_eye_detector.reset();
                    }
                }
            }
            else {
                left_eye_detector.checkBlink(true);
                right_eye_detector.checkBlink(true);
            }
        }

        // �ȳ� �ؽ�Ʈ
        putText(frame, "Press 'q' to quit", Point(20, 30), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);
        imshow("Eye Tracker (OpenCV)", frame);
        char key = (char)waitKey(1);
        if (key == 'q' || key == 27) break;
    }
    return 0;
}
