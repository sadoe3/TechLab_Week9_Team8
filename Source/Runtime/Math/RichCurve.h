#pragma once

#include "Containers/Array.h"
#include "Math/Vector2.h"

// 언리얼의 RichCurve 경량화
// 시간-값 매핑을통해 선형보간
class FRichCurve
{
public:
    void AddKey(float Time, float Value)
    {
        // 시간순 삽입과 동일 시간 교체
        for (int32 Index = 0; Index < Keys.Num(); ++Index)
        {
            if (Keys[Index].X == Time)
            {
                Keys[Index].Y = Value;
                return;
            }
            if (Keys[Index].X > Time)
            {
                Keys.Insert(FVector2(Time, Value), Index);
                return;
            }
        }
        Keys.Add(FVector2(Time, Value));
    }

    float Eval(float Time, float DefaultValue = 0.0f) const
    {
        // 빈 곡선과 시작점 처리
        if (Keys.IsEmpty())
        {
            return DefaultValue;
        }
        if (Time <= Keys[0].X)
        {
            return Keys[0].Y;
        }

        // 인접 키의 선형 보간
        for (int32 Index = 1; Index < Keys.Num(); ++Index)
        {
            const FVector2& Right = Keys[Index];
            if (Time <= Right.X)
            {
                const FVector2& Left = Keys[Index - 1];
                const float Alpha = (Time - Left.X) / (Right.X - Left.X);
                return Left.Y + (Right.Y - Left.Y) * Alpha;
            }
        }
        return Keys.Last().Y;
    }

private:
    // X: 시간, Y: 값
    TArray<FVector2> Keys;
};