#pragma once

#include <opencv2/opencv.hpp>

/**
 * @class BlinkDetector
 * @brief 눈 깜빡임을 감지하여 클릭 이벤트를 관리하는 클래스입니다.
 * 눈이 감긴 상태(눈동자가 보이지 않음)가 일정 프레임 이상 지속되는지 추적합니다.
 */
class BlinkDetector {
public:
    /**
     * @brief BlinkDetector 생성자
     * @param required_frames 몇 프레임 동안 눈이 감겨야 깜빡임으로 인정할지 설정합니다.
     */
    BlinkDetector(int required_frames = 5);

    /**
     * @brief 매 프레임마다 눈동자 감지 여부를 업데이트하여 깜빡임 상태를 확인합니다.
     * @param is_pupil_detected 현재 프레임에서 눈동자가 감지되었는지 여부 (true/false)
     */
    void checkBlink(bool is_pupil_detected);

    /**
     * @brief 현재 깜빡임이 감지되었는지 확인합니다.
     * @return 깜빡임이 감지되었으면 true, 아니면 false를 반환합니다.
     */
    bool isBlinking();

    /**
     * @brief 깜빡임 카운터와 상태를 초기화합니다. 클릭 처리 후 호출해야 합니다.
     */
    void reset();

private:
    int blink_counter;                  // 눈이 감긴 연속 프레임 수를 세는 카운터
    int required_consecutive_frames;    // 깜빡임으로 인정하기 위해 필요한 연속 프레임 수
    bool blinking_state;                // 현재 깜빡임이 감지되었는지 상태를 저장하는 플래그
};