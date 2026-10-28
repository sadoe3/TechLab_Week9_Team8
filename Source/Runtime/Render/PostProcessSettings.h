#pragma once

// Camera와 View가 값으로 보관하는 공통 Post Process 설정이다.
struct FPostProcessSettings
{
    // 기존 Linear 렌더 경로에서 출력용 sRGB 변환을 허용한다.
    bool bEnableGammaCorrection = true;

    // false여도 아래의 Vignetting 강도와 모양 설정은 유지한다.
    bool bEnableVignette = false;

    // 가장자리의 어둡기. 0은 감쇠 없음, 1은 최대 감쇠다.
    float VignetteIntensity = 0.5f;

    // 화면 중심에서 감쇠가 시작되는 거리다.
    // 화면 높이의 절반에 해당하는 거리를 1로 사용한다.
    float VignetteRadius = 0.5f;

    // Radius 바깥에서 최대 감쇠에 도달하기까지의 거리다.
    // Radius와 같은 단위이며, 0보다 큰 값을 사용한다.
    float VignetteSoftness = 0.5f;
};
