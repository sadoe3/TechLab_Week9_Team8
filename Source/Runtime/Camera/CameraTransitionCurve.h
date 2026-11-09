#pragma once

#include "Math/RichCurve.h"
#include <algorithm>
#include <cmath>

// 시간 비율의 곡선 종류
enum class ECameraTransitionCurveType
{
    Keys,
    Bezier
};

// 정규화 시간과 시점 합성 비율의 매핑
struct FCameraTransitionCurve
{
    ECameraTransitionCurveType Type = ECameraTransitionCurveType::Keys;
    FRichCurve Keys;
    FVector2 Control1 = FVector2(0.42f, 0.0f);
    FVector2 Control2 = FVector2(0.58f, 1.0f);

    float Eval(float TimeAlpha) const
    {
        // 유효 시간 범위와 정확한 양 끝점
        const float Time = std::isfinite(TimeAlpha)
            ? std::clamp(TimeAlpha, 0.0f, 1.0f) : 0.0f;

        if (Time <= 0.0f || Time >= 1.0f)
        {
            return Time;
        }

        float Alpha = Time;
        if (Type == ECameraTransitionCurveType::Keys)
        {
            Alpha = Keys.Eval(Time, Time);
        }
        else
        {
            if (!std::isfinite(Control1.X) || !std::isfinite(Control1.Y)
                || !std::isfinite(Control2.X) || !std::isfinite(Control2.Y))
            {
                return Time;
            }

            // 시간축 단조성을 위한 X 제어점 범위
            const double X1 = std::clamp(Control1.X, 0.0f, 1.0f);
            const double X2 = std::clamp(Control2.X, 0.0f, 1.0f);
            const auto Evaluate = [](double U, double P1, double P2)
                {
                    const double V = 1.0 - U;
                    return 3.0 * V * V * U * P1 + 3.0 * V * U * U * P2 + U * U * U;
                };

            // X(U) = Time을 만족하는 곡선 매개변수 탐색
            double Lower = 0.0;
            double Upper = 1.0;

            for (int32 Iteration = 0; Iteration < 24; ++Iteration)
            {
                const double U = (Lower + Upper) * 0.5;
                const double X = Evaluate(U, X1, X2);

                if (X == Time)
                {
                    Lower = U;
                    Upper = U;
                    break;
                }

                if (X < Time)
                {
                    Lower = U;
                }
                else
                {
                    Upper = U;
                }
            }

            Alpha = static_cast<float>(Evaluate((Lower + Upper) * 0.5, Control1.Y, Control2.Y));
        }

        // 합성 범위 보장과 잘못된 결과의 선형 대체
        return std::isfinite(Alpha) ? std::clamp(Alpha, 0.0f, 1.0f) : Time;
    }
};