//#include <opencv2/opencv.hpp>
//#include <opencv2/highgui.hpp>
//#include <opencv2/imgproc.hpp>
//#include <opencv2/objdetect.hpp>
//
//int main() {
//    // Load the pre-trained Haar cascade classifier for face detection
//    cv::CascadeClassifier face, eyes;
//    if (!face.load("haarcascade_frontalface_default.xml")) {
//        // Handle error if classifier not loaded
//        return -1;
//    }
//    if (!eyes.load("haarcascade_eye.xml")) {
//        // Handle error if classifier not loaded
//        return -1;
//    }
//
//    // Open the default camera
//    cv::VideoCapture cap(0);
//    if (!cap.isOpened()) {
//        // Handle error if camera not opened
//        return -1;
//    }
//
//    cv::Mat frame;
//    while (true) {
//        cap >> frame; // Read a new frame from the camera
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::Mat gray_frame;
//        cv::cvtColor(frame, gray_frame, cv::COLOR_BGR2GRAY); // Convert to grayscale
//        cv::equalizeHist(gray_frame, gray_frame); // Equalize histogram for better detection
//
//        std::vector<cv::Rect> faceRect, eyesRect;
//        // detectMultiScale(�Է� �̹���, ����� ����, �̹��� ��� ����, ���� �̿� ��, �÷���, ���� ���� �ּ� ũ��, �ִ� ũ��)
//        if(!face.empty()) face.detectMultiScale(gray_frame, faceRect, 1.1, 8, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30));
//        if (!eyes.empty()) eyes.detectMultiScale(gray_frame, eyesRect, 1.1, 4, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30));
//
//        // Draw rectangles around detected things
//        for (const auto& face : faceRect) {
//            cv::rectangle(frame, face, cv::Scalar(0, 255, 0), 2);
//        }
//        for (const auto& eyes : eyesRect) {
//            cv::rectangle(frame, eyes, cv::Scalar(0, 255, 0), 2);
//        }
//
//        cv::imshow("Face Detection", frame);
//        if (cv::waitKey(1) == 'q') { // Press 'q' to quit
//            break;
//        }
//    }
//
//    cap.release();
//    cv::destroyAllWindows();
//    return 0;
//}

#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <iostream>

int main() {
    // Haar Cascade �з��� �ε�
    cv::CascadeClassifier face_cascade, eyes_cascade;
    if (!face_cascade.load("haarcascade_frontalface_default.xml")) {
        std::cerr << "Error: face cascade not loaded." << std::endl;
        return -1;
    }
    if (!eyes_cascade.load("haarcascade_eye.xml")) {
        std::cerr << "Error: eyes cascade not loaded." << std::endl;
        return -1;
    }

    // �⺻ ī�޶� ����
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: camera not opened." << std::endl;
        return -1;
    }

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) {
            break;
        }

        cv::Mat gray_frame;
        cv::cvtColor(frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::equalizeHist(gray_frame, gray_frame);

        std::vector<cv::Rect> faces;
        face_cascade.detectMultiScale(gray_frame, faces, 1.1, 10, 0, cv::Size(30, 30));

        // �� �󱼿� ���� �ݺ�
        for (const auto& face : faces) {

            // �� ������ ROI�� ����
            cv::Mat face_roi_gray = gray_frame(face);
            cv::Mat face_roi_color = frame(face);

            std::vector<cv::Rect> eyes;
            // �� �ȿ��� �� ����
            eyes_cascade.detectMultiScale(face_roi_gray, eyes, 1.1, 10, 0, cv::Size(20, 20));

            // �� ���� ���� �ݺ�
            if (!eyes_cascade.empty())
            {
                cv::rectangle(frame, face, cv::Scalar(0, 255, 0), 2);
                for (const auto& eye : eyes) {
                    cv::rectangle(face_roi_color, eye, cv::Scalar(255, 0, 0), 2);

                    // �� ������ ROI�� �����Ͽ� ������ ã��
                    cv::Mat eye_roi = face_roi_gray(eye);

                    // 1. ����ȭ (Thresholding)
                    cv::Mat binary_eye;
                    // �Ӱ谪�� ���� ȯ�濡 ���� �����ؾ� �� �� �ֽ��ϴ�.
                    cv::threshold(eye_roi, binary_eye, 80, 255, cv::THRESH_BINARY_INV);

                    // 2. �������� �������� ������ ����
                    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
                    cv::erode(binary_eye, binary_eye, kernel, cv::Point(-1, -1), 1);
                    cv::dilate(binary_eye, binary_eye, kernel, cv::Point(-1, -1), 2);

                    // 3. ������ ã��
                    std::vector<std::vector<cv::Point>> contours;
                    cv::findContours(binary_eye, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                    double max_area = 0;
                    cv::Point pupil_center;
                    bool pupil_found = false;

                    // 4. ���� ū �������� ã�� �߽��� ���
                    for (const auto& contour : contours) {
                        double area = cv::contourArea(contour);
                        if (area > max_area) {
                            max_area = area;
                            cv::Moments mu = cv::moments(contour);
                            // ���Ʈ�� �̿��� �߽��� ���
                            pupil_center = cv::Point(mu.m10 / mu.m00, mu.m01 / mu.m00);
                            pupil_found = true;
                        }
                    }

                    if (pupil_found) {
                        // ���� ������ ��ǥ��� ��ȯ�Ͽ� �� �׸���
                        cv::circle(face_roi_color, cv::Point(eye.x + pupil_center.x, eye.y + pupil_center.y), 3, cv::Scalar(0, 0, 255), -1);
                    }
                }
            }
        }

        cv::imshow("Pupil Detection", frame);
        if (cv::waitKey(1) == 'q') {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}