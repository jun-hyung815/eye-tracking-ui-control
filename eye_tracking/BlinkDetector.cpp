// BlinkDetector.cpp

#include "BlinkDetector.h"

// ������: �ʿ��� ���� ������ ���� �ʱ�ȭ�մϴ�.
BlinkDetector::BlinkDetector(int required_frames) {
    required_consecutive_frames = required_frames;
    reset(); // ��� ���� ������ �ʱ� ���·� ����
}

// �� ������ ������ ���� ����� ������� ���¸� ������Ʈ�ϴ� �Լ�
void BlinkDetector::checkBlink(bool is_pupil_detected) {
    if (!is_pupil_detected) {
        // �����ڰ� �������� ������ ī���͸� 1 ������ŵ�ϴ�.
        blink_counter++;
    }
    else {
        // �����ڰ� �ٽ� �����Ǹ�, ���Ӽ��� �������Ƿ� ī���͸� 0���� �����մϴ�.
        blink_counter = 0;
    }

    // ī���Ͱ� ������ ������ ���� �����߰�, ���� ����� ���°� �ƴ϶��
    if (blink_counter >= required_consecutive_frames && !blinking_state) {
        // ����� ���¸� true�� �����մϴ�.
        blinking_state = true;
    }
}

// ���� ����� ���¸� ��ȯ�ϴ� �Լ�
bool BlinkDetector::isBlinking() {
    return blinking_state;
}

// ��� ���¸� �ʱ�ȭ�ϴ� �Լ�. Ŭ�� �̺�Ʈ ó�� �� �ݵ�� ȣ���ؾ� �մϴ�.
void BlinkDetector::reset() {
    blink_counter = 0;
    blinking_state = false;
}