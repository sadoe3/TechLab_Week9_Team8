#pragma once

// 검은 띠의 높이와 시간 보간 상태를 관리한다. 화면 합성은 Renderer가 담당한다.
class FCameraLetterBox
{
public:
    // 현재 높이에서 목표 높이로 전환한다. 목표 높이 0은 숨김, 시간 0은 즉시 적용이다.
    void Start(float InTargetHeightRatio, float InDuration);
    // 진행 중이거나 유지 중인 띠를 즉시 제거한다.
    void Stop();
    // DeltaTime은 0 이상의 초 단위 값이다. 완료된 높이는 유지한다.
    void Update(float DeltaTime);

    float GetHeightRatio() const { return HeightRatio; }

private:
    // 위·아래 각각의 띠 높이 / 전체 화면 높이. 범위는 0~0.5이다.
    float HeightRatio = 0.0f;
    float StartHeightRatio = 0.0f;
    float TargetHeightRatio = 0.0f;
    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
};
