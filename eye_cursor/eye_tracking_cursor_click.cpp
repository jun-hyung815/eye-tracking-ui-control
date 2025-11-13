// gaze_absolute_cursor_win_blink_click.cpp
// OpenCV만: 시선(nx,ny) -> 2차 다항식 매핑으로 절대좌표 + 숫자키(1~9) 캘리브레이션 + 왼/오른쪽 눈 깜빡이 클릭 (안정화 패치)
#include <opencv2/opencv.hpp>
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <vector>

using namespace cv;
using std::cout; using std::endl;

struct Sample {
    float nx, ny;   // 입력: 시선 정규화
    float sx, sy;   // 타깃: 화면 px
};

struct Poly2 {
    Mat A, B; // 6x1
    bool fit(const std::vector<Sample>& S) {
        if (S.size() < 6) return false;
        Mat M((int)S.size(), 6, CV_32F), X((int)S.size(), 1, CV_32F), Y((int)S.size(), 1, CV_32F);
        for (int i = 0; i < (int)S.size(); ++i) {
            float nx = S[i].nx, ny = S[i].ny;
            M.at<float>(i, 0) = 1.f;  M.at<float>(i, 1) = nx;  M.at<float>(i, 2) = ny;
            M.at<float>(i, 3) = nx * ny; M.at<float>(i, 4) = nx * nx; M.at<float>(i, 5) = ny * ny;
            X.at<float>(i, 0) = S[i].sx; Y.at<float>(i, 0) = S[i].sy;
        }
        Mat Acoef, Bcoef;
        bool ok1 = solve(M, X, Acoef, DECOMP_SVD);
        bool ok2 = solve(M, Y, Bcoef, DECOMP_SVD);
        if (!(ok1 && ok2)) return false;
        A = Acoef.clone(); B = Bcoef.clone(); return true;
    }
    bool map(float nx, float ny, float& sx, float& sy) const {
        if (A.empty() || B.empty()) return false;
        float f[6] = { 1.f,nx,ny,nx * ny,nx * nx,ny * ny };
        sx = 0.f; sy = 0.f;
        for (int i = 0; i < 6; ++i) { sx += A.at<float>(i, 0) * f[i]; sy += B.at<float>(i, 0) * f[i]; }
        return true;
    }
};

static float ema1(float prev, float cur, float a) { return prev * (1.f - a) + cur * a; }

// --- 시선 검출: 어두운 질량 중심 -> (nx, ny) ---
static bool darkCentroidNorm(const Mat& eyeGray, float& nx, float& ny) {
    // ★ FIX: 빈/작은/타입 체크 (기존 CV_Assert 제거)
    if (eyeGray.empty() || eyeGray.total() == 0 || eyeGray.rows < 5 || eyeGray.cols < 5 || eyeGray.type() != CV_8UC1)
        return false;

    Mat blur; GaussianBlur(eyeGray, blur, Size(7, 7), 0);
    Mat eq;   equalizeHist(blur, eq);             // ★ FIX: equalizeHist(blur, eq) (기존 코드 버그)
    Mat inv;  bitwise_not(eq, inv);

    if (inv.empty() || inv.total() == 0) return false; // ★ FIX: 가드
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

    nx = (cx - centerX) / max(1.f, eyeGray.cols * 0.5f);
    ny = (cy - centerY) / max(1.f, eyeGray.rows * 0.5f);
    nx = std::clamp(nx, -1.5f, 1.5f);
    ny = std::clamp(ny, -1.5f, 1.5f);
    return true;
}

// --- Windows 커서 ---
static void setCursorAbs(int x, int y) { SetCursorPos(x, y); }
static void clickLeft() {
    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, in, sizeof(INPUT));
}
static void clickRight() {
    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(2, in, sizeof(INPUT));
}

int main() {
    // --- 분류기 ---
    std::string faceXml = "haarcascade_frontalface_default.xml";
    std::string eyeXml = "haarcascade_eye_tree_eyeglasses.xml";
    CascadeClassifier faceC, eyeC;
    if (!faceC.load(faceXml) || !eyeC.load(eyeXml)) {
        std::cerr << "Load cascade failed. Check paths.\n"; return -1;
    }

    // --- 카메라 & 화면 ---
    VideoCapture cap(0);
    if (!cap.isOpened()) { std::cerr << "Camera open failed\n"; return -1; }
    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);

    const int SW = GetSystemMetrics(SM_CXSCREEN);
    const int SH = GetSystemMetrics(SM_CYSCREEN);

    // 9점 타깃
    POINT targets[9] = {
        {int(0.10 * SW), int(0.10 * SH)}, {int(0.50 * SW), int(0.10 * SH)}, {int(0.90 * SW), int(0.10 * SH)},
        {int(0.10 * SW), int(0.50 * SH)}, {int(0.50 * SW), int(0.50 * SH)}, {int(0.90 * SW), int(0.50 * SH)},
        {int(0.10 * SW), int(0.90 * SH)}, {int(0.50 * SW), int(0.90 * SH)}, {int(0.90 * SW), int(0.90 * SH)}
    };

    // 시선 평균
    float emaX = 0.f, emaY = 0.f;
    const float EMA_A = 0.25f;

    std::vector<Sample> samples;
    Poly2 model;
    bool modelReady = false, controlOn = false, showDbg = true;

    // 화면 좌표 EMA로 흔들림 억제
    float emaSX = (float)SW * 0.5f, emaSY = (float)SH * 0.5f;
    const float EMA_SPOS = 0.35f;

    // ===== 눈 깜빡이 클릭 감지 =====
    const int BLINK_MISS_FRAMES = 4;       // 연속 미검출 프레임 수
    const int BLINK_COOLDOWN_MS = 600;     // 클릭 쿨다운
    int missL = 0, missR = 0;
    ULONGLONG lastClickTimeL = 0, lastClickTimeR = 0;
    auto nowMs = []() { return GetTickCount64(); };

    while (true) {
        Mat frame; cap >> frame; if (frame.empty()) break;
        flip(frame, frame, 1);
        Mat gray; cvtColor(frame, gray, COLOR_BGR2GRAY);

        // --- 얼굴 & 눈 ---
        bool got = false; float nxMean = 0.f, nyMean = 0.f; int used = 0;
        bool leftSeen = false, rightSeen = false;

        std::vector<Rect> faces;
        faceC.detectMultiScale(gray, faces, 1.1, 3, 0, Size(120, 120));
        if (!faces.empty()) {
            Rect f = *std::max_element(faces.begin(), faces.end(),
                [](const Rect& a, const Rect& b) {return a.area() < b.area(); });
            rectangle(frame, f, Scalar(0, 255, 0), 2);

            Rect top(f.x, f.y, f.width, (int)(f.height * 0.6));
            top &= Rect(0, 0, frame.cols, frame.rows);                       // ★ FIX: 경계 클리핑
            Mat faceROI = gray(top);

            std::vector<Rect> eyes;
            eyeC.detectMultiScale(faceROI, eyes, 1.1, 2, 0, Size(28, 28), Size(220, 220));
            std::sort(eyes.begin(), eyes.end(), [](const Rect& a, const Rect& b) {return a.x < b.x; });

            float faceCenterX = f.x + f.width * 0.5f;

            for (size_t i = 0; i < eyes.size() && i < 2; i++) {
                Rect e = eyes[i];
                int sx = (int)(e.width * 0.14);
                int sy = (int)(e.height * 0.38);
                Rect et(e.x + sx, e.y + sy, e.width - 2 * sx, e.height - 2 * sy);
                et &= Rect(0, 0, faceROI.cols, faceROI.rows);                // ★ FIX: faceROI 경계 클리핑
                if (et.width < 12 || et.height < 12) continue;

                Rect er(et.x + top.x, et.y + top.y, et.width, et.height);
                er &= Rect(0, 0, gray.cols, gray.rows);                      // ★ FIX: 프레임 경계 재클리핑
                if (er.width < 8 || er.height < 8) continue;                 // ★ FIX: 최소 크기
                rectangle(frame, er, Scalar(255, 200, 0), 1);

                Mat eyeGray = gray(er);
                if (eyeGray.empty() || eyeGray.total() == 0 || eyeGray.type() != CV_8UC1) continue; // ★ FIX

                float nx = 0.f, ny = 0.f;
                bool ok = false;
                try {                                                        // ★ FIX: 예외 방지
                    ok = darkCentroidNorm(eyeGray, nx, ny);
                }
                catch (const cv::Exception& ex) {
                    std::cerr << "[darkCentroidNorm] " << ex.what() << std::endl;
                    ok = false;
                }

                bool isLeftSide = (er.x + er.width * 0.5f) < faceCenterX;

                if (ok) {
                    // 시각화
                    int cx = er.x + er.width / 2 + (int)(nx * (er.width * 0.5f));
                    int cy = er.y + er.height / 2 + (int)(ny * (er.height * 0.5f));
                    line(frame, Point(cx, er.y), Point(cx, er.y + er.height), Scalar(0, 0, 255), 2);
                    line(frame, Point(er.x, cy), Point(er.x + er.width, cy), Scalar(0, 255, 0), 2);

                    nxMean += nx; nyMean += ny; used++;
                    if (isLeftSide) leftSeen = true; else rightSeen = true;

                    if (showDbg) {
                        putText(frame, cv::format("nx=%.2f ny=%.2f", nx, ny),
                            Point(er.x, er.y - 6), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 255), 1);
                    }
                }
                else {
                    putText(frame, "pupil?", Point(er.x, er.y - 6),
                        FONT_HERSHEY_SIMPLEX, 0.45, Scalar(40, 40, 255), 1);
                }
            }

            // --- 시선 평균 & 커서 이동 ---
            if (used > 0) {
                nxMean /= used; nyMean /= used;
                emaX = ema1(emaX, nxMean, EMA_A);
                emaY = ema1(emaY, nyMean, EMA_A);
                got = true;

                if (controlOn && modelReady) {
                    float sx, sy;
                    if (model.map(emaX, emaY, sx, sy)) {
                        emaSX = ema1(emaSX, sx, EMA_SPOS);
                        emaSY = ema1(emaSY, sy, EMA_SPOS);
                        int ix = std::clamp((int)std::lround(emaSX), 0, SW - 1);
                        int iy = std::clamp((int)std::lround(emaSY), 0, SH - 1);
                        setCursorAbs(ix, iy);
                    }
                }
            }

            // --- 깜빡이 클릭 로직 ---
            if (leftSeen && !rightSeen) {
                missR++; missL = 0;
            }
            else if (!leftSeen && rightSeen) {
                missL++; missR = 0;
            }
            else {
                missL = 0; missR = 0; // 양쪽 모두(또는 둘 다 X) → 일반 깜빡임으로 간주
            }

            ULONGLONG now = nowMs();
            if (missL >= BLINK_MISS_FRAMES) {
                if (now - lastClickTimeL > (ULONGLONG)BLINK_COOLDOWN_MS) {
                    clickLeft();
                    lastClickTimeL = now;
                    putText(frame, "LEFT CLICK", Point(f.x, f.y - 10),
                        FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 255), 2);
                }
                missL = 0;
            }
            if (missR >= BLINK_MISS_FRAMES) {
                if (now - lastClickTimeR > (ULONGLONG)BLINK_COOLDOWN_MS) {
                    clickRight();
                    lastClickTimeR = now;
                    putText(frame, "RIGHT CLICK", Point(f.x + f.width / 2, f.y - 10),
                        FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 255), 2);
                }
                missR = 0;
            }
        }

        // --- HUD ---
        putText(frame, controlOn ? "Gaze->Cursor: ON" : "Gaze->Cursor: OFF",
            Point(20, 40), FONT_HERSHEY_SIMPLEX, 0.8, controlOn ? Scalar(0, 255, 0) : Scalar(200, 200, 200), 2);
        putText(frame, modelReady ? "Model: READY (ENTER to refit)"
            : "Model: NOT FITTED (1..9 then ENTER)",
            Point(20, 70), FONT_HERSHEY_SIMPLEX, 0.7, modelReady ? Scalar(0, 255, 255) : Scalar(50, 200, 255), 2);
        putText(frame, "1..9: add sample  ENTER: fit  G: toggle control  0: clear  Q: quit",
            Point(20, frame.rows - 20), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(230, 230, 230), 2);

        imshow("Gaze -> Absolute Cursor + Blink Click (Windows)", frame);
        int k = waitKey(1);
        if (k == 'q' || k == 27) break;
        if (k == 'g' || k == 'G') controlOn = !controlOn;
        if (k == 'v' || k == 'V') showDbg = !showDbg;
        if (k == '0') { samples.clear(); modelReady = false; }

        auto addSample = [&](int idx, const char* name) {
            if (!got) return; // ★ FIX: 현재 시선이 유효할 때만 등록
            Sample s; s.nx = emaX; s.ny = emaY;
            s.sx = (float)targets[idx].x; s.sy = (float)targets[idx].y;
            samples.push_back(s);
            cout << "Add sample " << name << " nx=" << s.nx << " ny=" << s.ny
                << " -> (" << s.sx << "," << s.sy << ")\n";
            };
        if (k == '1') addSample(0, "TL");
        if (k == '2') addSample(1, "T");
        if (k == '3') addSample(2, "TR");
        if (k == '4') addSample(3, "L");
        if (k == '5') addSample(4, "C");
        if (k == '6') addSample(5, "R");
        if (k == '7') addSample(6, "BL");
        if (k == '8') addSample(7, "B");
        if (k == '9') addSample(8, "BR");

        if (k == 13) { // ENTER
            if (samples.size() >= 6) {
                modelReady = model.fit(samples);
                cout << (modelReady ? "[Fit] OK (" : "[Fit] FAIL (") << samples.size() << " samples)\n";
            }
            else {
                cout << "[Fit] Need >= 6 samples. Current: " << samples.size() << "\n";
            }
        }
    }
    return 0;
}
