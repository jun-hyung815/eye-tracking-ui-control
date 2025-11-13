# opencv_project

### 눈동자 인식 & 커서 컨트롤 개요

#### 전체 파이프라인

1. 카메라 캡처 → 거울 모드(flip(frame, 1)) → 그레이 변환

2. 얼굴 검출(Haar) → 상단 60%만 눈 후보 ROI(top)

3. 눈 검출(Haar) → 각 눈 박스를 안/위쪽 중심부로 강하게 축소(sx=14%, sy=38%)

4. 각 눈에서 어두운 질량 중심으로 동공 중심을 추정 → ROI 중심 기준 정규화된 시선 (nx, ny) ∈ [-1,1]

5. 양쪽 눈이 잡히면 평균 → 1차 EMA로 부드럽게(emaX, emaY)

6. (캘리브 후) 2차 다항식 맵으로 (emaX, emaY) → (sx, sy) 화면 좌표 변환 → 2차 EMA로 잔떨림 억제(emaSX, emaSY) → SetCursorPos()

7. 깜빡이 클릭: 프레임별로 좌/우 눈의 동공 탐지 성공 여부를 보고, 한쪽만 연속 BLINK_MISS_FRAMES 프레임 미검출이면 클릭 트리거(좌=left click, 우=right click). 쿨다운으로 중복 방지.

#### 주요 구조 & 수식
1) 시선 추정: darkCentroidNorm()

- 전처리: GaussianBlur(7x7) → equalizeHist → bitwise_not
  (어두운 동공을 밝게 만들어서 가중치로 쓰기 위함)

- 임계값: t = mean + 0.6*std → THRESH_TOZERO로 그보다 밝은(=원영상 어두운) 픽셀만 남김

- 잡음 정리: morph open(3) → close(5)

- 무게중심: moments(w) → (cx, cy)

- 정규화:

<img width="306" height="63" alt="Image" src="https://github.com/user-attachments/assets/de27235a-e476-42d0-97b3-cd74e5c609cc" />

(왼쪽/위가 음수, 오른쪽/아래가 양수)

가드 추가가 잘 들어가 있음: 빈/작은 Mat 체크, 타입 체크, equalizeHist(blur, eq)로 수정, inv.empty() 체크 등.

2) 보정(캘리브) & 좌표 맵핑: Poly2

- 2차 다항식(교차항 포함)으로 화면 좌표를 근사:

<img width="401" height="72" alt="Image" src="https://github.com/user-attachments/assets/fe1d9dad-d593-4b71-8624-d4eaff4fbece" />​


- 최소 6개 샘플(9점 권장)을 SVD 최소제곱으로 풂(DECOMP_SVD).

- 샘플 수집: 숫자키 1..9로 현재 응시(nx,ny) → 정해진 화면 점(sx,sy) 매핑을 쌓고, ENTER로 fit().

3) 필터링

- 시선 필터(1차 EMA): emaX, emaY (α=0.25)

- 화면 좌표 필터: emaSX, emaSY (α=0.35) → 잔떨림 억제

4) 커서 제어

- controlOn && modelReady일 때만 SetCursorPos(ix, iy)

- 좌표는 화면 경계로 클램프.

5) 눈 깜빡이 클릭

- 왼/오 눈 각각 동공이 잡혔는지 기록:

  - leftSeen && !rightSeen → 오른쪽 눈 사라짐 → missR++

  - !leftSeen && rightSeen → 왼쪽 눈 사라짐 → missL++

  - 그 외(둘 다 보이거나 둘 다 X) → 둘 다 0으로 리셋(일반 깜빡임 무시)

- missL >= BLINK_MISS_FRAMES이면 좌클릭, missR >= ...이면 우클릭.

- BLINK_COOLDOWN_MS로 반복 클릭 방지.

#### 안정화 장치(에러 방지)

- ROI 이중 클리핑: et를 faceROI와, er를 프레임 전체와 교집합 → 음수/경계 초과 방지

- 최소 크기 체크: er.width/height가 너무 작으면 스킵

- eyeGray 타입/비어있음 체크

- darkCentroidNorm 내부 가드 + try/catch로 프레임 스킵

- 샘플 등록 시 got(이번 프레임 시선 유효)일 때만 추가

#### 성능/지연

- Haar 기반이라 CPU만으로도 30fps 근방 가능(해상도/CPU에 따라 차이).

- 매 프레임 SVD는 안 하고, ENTER 눌렀을 때만 학습 → 실시간 성능 영향 적음.
