#include <opencv2/opencv.hpp>
#include <iostream>
#include <algorithm>
using namespace cv;
using std::cout; using std::endl;

struct Calib1D {
    bool hasC = false, hasN = false, hasP = false; // C=Center, N=Negative(L/Up), P=Positive(R/Down)
    float C = 0.f, N = 0.f, P = 0.f;
    float map(float x) const {
        if (!(hasC && hasN && hasP)) return x;
        if (x <= C) {
            float d = std::max(1e-4f, C - N);
            return std::clamp((x - C) / d, -1.5f, 0.f);
        }
        else {
            float d = std::max(1e-4f, P - C);
            return std::clamp((x - C) / d, 0.f, 1.5f);
        }
    }
    bool ready() const { return hasC && hasN && hasP && N < C && C < P; }
};

struct Calib2D {
    Calib1D X, Y; // X: Left(-)/Right(+), Y: Up(-)/Down(+)
    bool ready() const { return X.ready() && Y.ready(); }
};

static float ema1(float prev, float cur, float a) { return prev * (1.f - a) + cur * a; }

// 어두운 질량 중심으로 동공 중심 (cx, cy) 추정 → (nx, ny) 정규화 반환
static bool darkCentroidNorm(const Mat& eyeGray, float& nx, float& ny) {
    CV_Assert(eyeGray.type() == CV_8UC1);
    Mat blur; GaussianBlur(eyeGray, blur, Size(7, 7), 0);
    Mat eq; equalizeHist(blur, eq);
    Mat inv; bitwise_not(eq, inv);
    Scalar m, s; meanStdDev(inv, m, s);
    double t = m[0] + 0.6 * s[0];
    Mat w; threshold(inv, w, t, 255, THRESH_TOZERO);
    morphologyEx(w, w, MORPH_OPEN, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
    morphologyEx(w, w, MORPH_CLOSE, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
    Moments mu = moments(w, false);
    if (mu.m00 < 2e4) return false;

    float cx = (float)(mu.m10 / mu.m00);
    float cy = (float)(mu.m01 / mu.m00);

    float centerX = (eyeGray.cols - 1) * 0.5f;
    float centerY = (eyeGray.rows - 1) * 0.5f;
    nx = (cx - centerX) / std::max(1.f, eyeGray.cols * 0.5f);  // -1..1 (왼:-, 오:+)
    ny = (cy - centerY) / std::max(1.f, eyeGray.rows * 0.5f);  // -1..1 (위:-, 아래:+)

    nx = std::clamp(nx, -1.5f, 1.5f);
    ny = std::clamp(ny, -1.5f, 1.5f);
    return true;
}

int main() {
    std::string faceXml = "haarcascade_frontalface_default.xml";
    std::string eyeXml = "haarcascade_eye_tree_eyeglasses.xml";
    CascadeClassifier faceC, eyeC;
    if (!faceC.load(faceXml) || !eyeC.load(eyeXml)) {
        std::cerr << "Load cascade failed. Check paths.\n"; return -1;
    }

    VideoCapture cap(0);
    if (!cap.isOpened()) { std::cerr << "Camera open failed\n"; return -1; }
    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);

    Calib2D calib;
    bool showDbg = true;
    //const bool USE_DIAGONAL = false; // true로 바꾸면 8방(대각 포함)
    const bool USE_DIAGONAL = true; // true로 바꾸면 8방(대각 포함)

    // EMA
    float emaX = 0.f, emaY = 0.f;
    const float EMA_A = 0.25f;

    // 임계값 & 히스테리시스 (축별)
    float thX = 0.35f, thY = 0.35f;
    float hyX = 0.06f, hyY = 0.06f;

    std::string label = "CENTER";

    while (true) {
        Mat frame; cap >> frame; if (frame.empty()) break;
        flip(frame, frame, 1);
        Mat gray; cvtColor(frame, gray, COLOR_BGR2GRAY);

        bool got = false;
        float nxMean = 0.f, nyMean = 0.f;
        int used = 0;

        // 얼굴
        std::vector<Rect> faces;
        faceC.detectMultiScale(gray, faces, 1.1, 3, 0, Size(120, 120));
        if (!faces.empty()) {
            Rect f = *std::max_element(faces.begin(), faces.end(),
                [](const Rect& a, const Rect& b) {return a.area() < b.area(); });
            rectangle(frame, f, Scalar(0, 255, 0), 2);

            Rect top(f.x, f.y, f.width, (int)(f.height * 0.6));
            top &= Rect(0, 0, frame.cols, frame.rows);
            Mat faceROI = gray(top);

            std::vector<Rect> eyes;
            eyeC.detectMultiScale(faceROI, eyes, 1.1, 2, 0, Size(28, 28), Size(220, 220));
            std::sort(eyes.begin(), eyes.end(), [](const Rect& a, const Rect& b) {return a.x < b.x; });

            for (size_t i = 0; i < eyes.size() && i < 2; i++) {
                Rect e = eyes[i];
                // ROI 강축소(상하도 중요하므로 위쪽을 좀 더 자름)
                int sx = (int)(e.width * 0.14);
                int sy = (int)(e.height * 0.38);
                Rect et(e.x + sx, e.y + sy, e.width - 2 * sx, e.height - 2 * sy);
                et &= Rect(0, 0, faceROI.cols, faceROI.rows);
                if (et.width < 12 || et.height < 12) continue;

                Rect er(et.x + top.x, et.y + top.y, et.width, et.height);
                rectangle(frame, er, Scalar(255, 200, 0), 1);

                Mat eyeGray = gray(er);
                float nx, ny;
                if (!darkCentroidNorm(eyeGray, nx, ny)) continue;

                // 시각화: x, y 위치
                int cx = er.x + er.width / 2 + (int)(nx * (er.width * 0.5f));
                int cy = er.y + er.height / 2 + (int)(ny * (er.height * 0.5f));
                line(frame, Point(cx, er.y), Point(cx, er.y + er.height), Scalar(0, 0, 255), 2);
                line(frame, Point(er.x, cy), Point(er.x + er.width, cy), Scalar(0, 255, 0), 2);

                nxMean += nx; nyMean += ny; used++;
                if (showDbg) {
                    putText(frame, cv::format("nx=%.2f ny=%.2f", nx, ny),
                        Point(er.x, er.y - 6), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 255), 1);
                }
            }

            if (used > 0) {
                nxMean /= used; nyMean /= used;

                // 캘리브레이션 맵 적용
                float ax = calib.X.map(nxMean);
                float ay = calib.Y.map(nyMean);

                // EMA
                emaX = ema1(emaX, ax, EMA_A);
                emaY = ema1(emaY, ay, EMA_A);

                // 분류 (히스테리시스)
                auto decideAxis = [](float v, float th, float hy) {
                    // 음수: N(Left/Up), 양수: P(Right/Down), 중앙: C
                    float N = -(th + hy);
                    float P = (th + hy);
                    if (v <= N) return -1;        // Negative side
                    if (v >= P) return +1;        // Positive side
                    if (std::abs(v) < th) return 0; // 확실한 중앙
                    return 2; // 그 외 구간은 이전 상태 유지용
                    };

                static int lastX = 0, lastY = 0; // -1,0,+1
                int cxState = decideAxis(emaX, thX, hyX);
                int cyState = decideAxis(emaY, thY, hyY);

                if (cxState != 2) lastX = cxState;
                if (cyState != 2) lastY = cyState;

                // 레이블 결정(기본: 대각선 OFF)
                if (!USE_DIAGONAL) {
                    if (lastY == -1)      label = "UP";
                    else if (lastY == +1) label = "DOWN";
                    else if (lastX == -1) label = "LEFT";
                    else if (lastX == +1) label = "RIGHT";
                    else                  label = "CENTER";
                }
                else {
                    // 대각선 포함
                    if (lastX == 0 && lastY == 0) label = "CENTER";
                    else if (lastX == -1 && lastY == 0) label = "LEFT";
                    else if (lastX == +1 && lastY == 0) label = "RIGHT";
                    else if (lastX == 0 && lastY == -1) label = "UP";
                    else if (lastX == 0 && lastY == +1) label = "DOWN";
                    else if (lastX == -1 && lastY == -1) label = "LEFT-UP";
                    else if (lastX == -1 && lastY == +1) label = "LEFT-DOWN";
                    else if (lastX == +1 && lastY == -1) label = "RIGHT-UP";
                    else if (lastX == +1 && lastY == +1) label = "RIGHT-DOWN";
                }

                got = true;

                // 디버그 HUD 바 2개
                if (showDbg) {
                    // X bar
                    int barW = std::min(800, frame.cols - 40);
                    int x0 = 20, y0 = frame.rows - 50;
                    rectangle(frame, Rect(x0, y0 - 10, barW, 20), Scalar(50, 50, 50), FILLED);
                    int mid = x0 + barW / 2;
                    line(frame, Point(mid, y0 - 12), Point(mid, y0 + 12), Scalar(200, 200, 200), 1);
                    int posX = x0 + (int)((emaX + 1.f) * 0.5f * barW);
                    circle(frame, Point(posX, y0), 7, Scalar(0, 255, 0), FILLED);
                    int lpos = x0 + (int)((-thX + 1.f) * 0.5f * barW);
                    int rpos = x0 + (int)(((thX)+1.f) * 0.5f * barW);
                    line(frame, Point(lpos, y0 - 12), Point(lpos, y0 + 12), Scalar(0, 200, 255), 2);
                    line(frame, Point(rpos, y0 - 12), Point(rpos, y0 + 12), Scalar(0, 200, 255), 2);
                    putText(frame, "X", Point(x0 + barW + 8, y0 + 5), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(230, 230, 230), 2);

                    // Y bar
                    y0 = frame.rows - 20;
                    rectangle(frame, Rect(x0, y0 - 10, barW, 20), Scalar(50, 50, 50), FILLED);
                    mid = x0 + barW / 2;
                    line(frame, Point(mid, y0 - 12), Point(mid, y0 + 12), Scalar(200, 200, 200), 1);
                    int posY = x0 + (int)((emaY + 1.f) * 0.5f * barW);
                    circle(frame, Point(posY, y0), 7, Scalar(0, 255, 0), FILLED);
                    lpos = x0 + (int)((-thY + 1.f) * 0.5f * barW);
                    rpos = x0 + (int)(((thY)+1.f) * 0.5f * barW);
                    line(frame, Point(lpos, y0 - 12), Point(lpos, y0 + 12), Scalar(0, 200, 255), 2);
                    line(frame, Point(rpos, y0 - 12), Point(rpos, y0 + 12), Scalar(0, 200, 255), 2);
                    putText(frame, "Y", Point(x0 + barW + 8, y0 + 5), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(230, 230, 230), 2);
                }
            }
            else {
                label = "SEARCHING";
            }
        }
        else {
            label = "NO FACE";
        }

        // 라벨 출력
        putText(frame, label, Point(30, 100), FONT_HERSHEY_SIMPLEX, 2.0,
            label == "LEFT" ? Scalar(0, 200, 255) :
            label == "RIGHT" ? Scalar(0, 255, 0) :
            label == "UP" ? Scalar(255, 200, 0) :
            label == "DOWN" ? Scalar(200, 0, 255) : Scalar(255, 255, 255), 6, LINE_AA);

        // 안내
        putText(frame, "1:CENTER  2:LEFT  3:RIGHT  4:UP  5:DOWN  V:debug  Q:quit",
            Point(20, 40), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(230, 230, 230), 2);

        // 캘리브 상태
        if (!calib.ready()) {
            std::string st = std::string("Calib X[") + (calib.X.hasC ? 'C' : '-') + (calib.X.hasN ? 'L' : '-') + (calib.X.hasP ? 'R' : '-') + "] "
                + "Y[" + (calib.Y.hasC ? 'C' : '-') + (calib.Y.hasN ? 'U' : '-') + (calib.Y.hasP ? 'D' : '-') + "]";
            putText(frame, st, Point(20, 70), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);
        }

        imshow("Gaze (OpenCV only, 5-way)", frame);
        int k = waitKey(1);
        if (k == 'q' || k == 27) break;
        if (k == 'v' || k == 'V') showDbg = !showDbg;

        // === 5점 캘리브레이션 ===
        if (k == '1') { calib.X.C = emaX; calib.Y.C = emaY; calib.X.hasC = true; calib.Y.hasC = true; }
        if (k == '2') { calib.X.N = emaX; calib.X.hasN = true; } // LEFT
        if (k == '3') { calib.X.P = emaX; calib.X.hasP = true; } // RIGHT
        if (k == '4') { calib.Y.N = emaY; calib.Y.hasN = true; } // UP (Y음수쪽)
        if (k == '5') { calib.Y.P = emaY; calib.Y.hasP = true; } // DOWN (Y양수쪽)
    }
    return 0;
}
