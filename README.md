# Eye See You 👁️👄👁️

<img width="500" height="500" alt="image" src="https://github.com/user-attachments/assets/97963db4-9972-42f4-b3b4-8da5a9599334" />


## **팀 구성 및 역할**
| 팀원       | 역할                                                |
|------------|---------------------------------------------------|
| **김국현** | 클릭 기능 구현 |
| **김민우** | 아이트래킹 기능 구현 |
| **김준형** | <ins>__영상 전처리__</ins> |

## **프로젝트 개요**

**EyeSeeYou**는 눈동자의 움직임을 통해 모니터 화면의 커서를 제어할 수 있는 시스템입니다.

이 프로젝트는 루게릭 환자들이나 지체 장애인같이 신체가 불편하여 마우스를 조작하기 힘든 사람들을 위해 구상되었습니다.

또한 누구나 저렴한 가격에 아이트래킹 기능을 사용할 수 있도록 웹캠만 있으면 사용 가능한 시스템을 만들기 위해 노력했습니다.

사용자의 눈동자의 변화를 좌표로 변환하고 9점 캘리브레이션을 통해 모니터의 2차원 좌표를 추산하여 웹캠만으로 아이트래킹을 구현했습니다.

또한 이미지 전처리 과정을 통해 눈동자 인식의 정확도를 높였습니다.

## **주요 목표**
1. **아이트래킹 기능 구현**
    - OpenCV의 영상 처리 기술을 이용하여 눈동자 움직임을 계산
    - 계산된 눈동자 움직임과 마우스 제어를 연동
2. **좌클릭, 우클릭 등의 부가 기능 추가**
    - 왼쪽 눈과 오른쪽 눈의 깜빡임을 구분
    - 왼쪽 눈 깜빡 -> 좌클릭
    - 오른쪽 눈 깜빡 -> 우클릭 
3. **이미지 전처리를 통한 성능 강화**
    - 웹캠 만으로 처리하는 눈동자 검출의 정확도를 이미지 전처리를 통해 성능 극대화
  
## **프로젝트 일정**
### <2025.09.19-2025.09.25>

## **개발환경**
1. 운영체제: Window 11
2. 언어: C++
3. 도구: OpenCV 4

## **System Flow**
<img width="500" height="800" alt="image" src="https://github.com/user-attachments/assets/716f6e00-1e96-4404-a3e9-709bfb65b3a8" />

## **주요기능**

### **이미지 전처리**
- 노이즈, 조명, 반사광 등 여러 환경에 의해 동공 검출의 성능이 저하될 수 있음
- 이미지 전처리를 통해 눈동자 ROI 내의 불필요한 정보 등을 필터링하여 눈동자 검출 성능 향상
- Gray Scale 변환, 대비 향상, 노이즈 제거, 이진화, 작은 점 제거 등의 이미 지 전처리 과정 수행
    - Gray Scale
      
      <img width="353" height="246" alt="image" src="https://github.com/user-attachments/assets/32f2c2f3-300b-476f-995d-6e10b00ae32e" />

    - Preprocessed Eyes
 
      <img width="355" height="247" alt="image" src="https://github.com/user-attachments/assets/42d43fa0-b62f-49a2-bf56-08265b6a84e5" />
      
### **아이 트래킹을 통한 커서 이동**
- OpneCV의 Cascade 검출기(haar)를 활용해 사용자의 얼굴 및 눈동자를 검출
- 눈동자 ROI의 크기를 최소화 하여 불필요한 요소를 최대한 제거
  
  <기본 ROI>

  <img width="237" height="69" alt="image" src="https://github.com/user-attachments/assets/0003a47b-8fe9-49a0-8678-c598479b7584" />

  <ROI 최소화>
  
  <img width="268" height="84" alt="image" src="https://github.com/user-attachments/assets/b683951c-d593-4c62-96cc-4f1cb62f086a" />


- 어두운 부분의 질량 중심을 눈동자 중심 좌표로 사용
  
  <img width="367" height="143" alt="image" src="https://github.com/user-attachments/assets/e30d07dd-aa37-4469-89d1-c71e9f6e91ed" />

- 눈 ROI 중심을 기준으로 좌우(상하)판단 (nx, ny)

  <img width="399" height="160" alt="image" src="https://github.com/user-attachments/assets/d4c7f3e0-1f20-412c-8002-214b6b437fec" />
  <br></br>
  <img width="306" height="63" alt="Image" src="https://github.com/user-attachments/assets/de27235a-e476-42d0-97b3-cd74e5c609cc" />

  (왼쪽/위가 음수, 오른쪽/아래가 양수)


- 눈동자의 좌표 값을 읽어 9가지 방향 구분

  <img width="500" height="400" alt="image" src="https://github.com/user-attachments/assets/a0dda7fe-d1bc-4c05-83f3-cef352104ee8" />

- 9점 캘리브레이션을 통한 모니터 화면 2차원 좌표 계산 -> 커서 이동

  <img width="445" height="322" alt="image" src="https://github.com/user-attachments/assets/9bf4aa46-f7ac-4d70-b896-ad0f2ffb08d1" />

  - 사용 모델: 2차 다항식 회귀
    <입력 -> 출력 매칭>
  
    <img width="400" height="74" alt="image" src="https://github.com/user-attachments/assets/fc42a836-71a1-4be2-a587-1539c4e25722" />

    <9개의 샘플>

    <img width="276" height="38" alt="image" src="https://github.com/user-attachments/assets/665fcd9a-09bf-440a-87d3-341821147c22" />

    <입력 행렬 *M* (9 x 6)>

    <img width="410" height="139" alt="image" src="https://github.com/user-attachments/assets/0486d4f1-c792-4759-835f-f2c1255a6fc8" />

    <출력 벡터 *X*, *Y* (9 x 1)>

    <img width="247" height="131" alt="image" src="https://github.com/user-attachments/assets/6660f28a-0d23-4e0e-84e6-1dd06c04f260" />

    <최소제곱으로 계수 구함>

    <img width="325" height="74" alt="image" src="https://github.com/user-attachments/assets/3363a12c-a031-4b6a-89cc-0f8b0e938d09" />

    <코드로 구현 -OpenCV(solve)>

    ``` 
    solve(M, X, A, DECOMP_SVD);  // A = [a0 a1 a2 a3 a4 a5]^T
    solve(M, Y, B, DECOMP_SVD);  // B = [b0 b1 b2 b3 b4 b5]^T
    ```

### **눈깜빡임을 통한 커서 클릭 제어**
- 얼굴 중심 x좌표를 기준으로 눈동자 ROI 중심의 x좌표가 얼굴 중심보다 작으면 왼쪽 눈, 크면 오른쪽 눈으로 구별
  
  <img width="273" height="80" alt="image" src="https://github.com/user-attachments/assets/561559a3-40e3-4c7d-bec4-325911ccc728" />
  
- 눈동자 ROI 일정 프레임 이상 사라지면 클릭으로 인식
  
  <img width="392" height="308" alt="image" src="https://github.com/user-attachments/assets/3369a764-66f8-495c-8ed5-9e6e970cf42d" />

## Trouble Shooting
### 1. 눈동자 검출의 어려움

#### 어두운 조명, 낮은 카메라 해상도, 눈썹 등의 방해요소가 원인이 되어 눈동자 검출의 어려움을 겪음
   - 고화질 카메라를 사용하여 극복하려 시도했으나 고화질에 의한 버벅임 발생과 여전히 조도, 밝기 등의 방해요소 존재
   - 기존의 카메라를 사용하되 Gray Scale, 이진화 등의 이미지 전처리 과정을 진행하여 눈동자 검출의 성능을 극대화함

### 2. 눈동자 움직임 수치화의 어려움

#### 처음에는 눈동자 ROI의 범위가 넓어 검은자 외의 어두운 부분이 방해가 되어 눈동자의 좌표가 제대로 계산되지 않음
   - 눈동자 ROI를 최소화 하여 어두운 부분의 후보를 최대한 검은자가 되도록 하여 해결
     
#### 또한 단순히 눈동자의 검은자와 흰자의 비율을 가지고 좌표를 계산하려 했으나 눈동자의 움직임이 크지 않아 좌표 값이 계산되지 않음
   - 눈동자의 어두운 부분(검은자)의 질량 중심의 좌표를 눈동자 ROI의 중심 좌표를 기준으로하여 좌표를 계산해 해결

### 3. 클릭 알고리즘 구현의 어려움

#### 처음에는 for문을 이용하여 왼쪽부터 왼쪽 눈 오른쪽 눈으로 구별 -> 왼쪽 눈을 감았을 때 오른쪽 눈이 왼쪽 눈으로 인식되어 버림
   - 얼굴의 중심 x좌표 값을 기준으로 눈동자 ROI의 x좌표가 그것보다 작으면 왼쪽 눈, 크면 오른쪽 눈으로 구별하도록 하여 해결
     
#### 눈동자 ROI가 사라지면 깜빡인걸로 인식하여 클릭을 구현 -> 눈동자 인식이 잠시라도 풀리면 클릭으로 간주 됨
   - 일정 프레임 이상 ROI가 사라졌을 때만 깜빡인 것으로 인식하도록 구현하여 해결
     
## 보완 사항
1. 사용자 얼굴을 고정해야하는 문제 해결
   - 현재는 사용자의 얼굴의 위치가 바뀌면 모니터 화면의 좌표와 눈동자의 좌표가 틀어져 제대로 커서가 이동하지 않음
   - AI(Mediapipe face mesh 등)을 사용하여 정규화를 통해 얼굴의 위치가 바뀌어도 원하는 곳으로 커서를 이동시킬 수 있게 함
2. 좌클릭, 우클릭 외의 추가 기능 구현
   - 현재는 좌클릭, 우클릭의 기능만 구현되어 있음
   - 이 또한 AI를 활용하여 사용자의 고개가 움직이면 그에 맞게 화면이 스크롤 되는 기능을 추가 할 수 있을 것으로 보임

## 시연 영상

### 눈동자 이미지 전처리

![Image](https://github.com/user-attachments/assets/e48638bd-20b0-4465-945b-74606f7845e5)

### 아이트래킹 - 커서 이동

![Image](https://github.com/user-attachments/assets/7cf0a9df-e59e-4cd4-81e9-cb3bf7091c6b)

### 아이트래킹 - 클릭

![Image](https://github.com/user-attachments/assets/268d02d5-fcf8-4198-a136-82c91139f70a)
