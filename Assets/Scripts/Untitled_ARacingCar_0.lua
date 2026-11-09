-- 전환 대상과 튜닝값
local TargetName = "TransitionCamera"
local BlendTime = 4
local LockOutgoing = true
local BlendFunction = "Linear"
local BlendExp = 2.0

-- 공통 전환 요청
local function RequestViewTarget(Target)
    local Accepted = world.set_view_target(
        Target, BlendTime, LockOutgoing, BlendFunction, BlendExp)

    if not Accepted then
        print("[CameraTransition] 전환 요청 실패")
    end
end

function Tick(DeltaTime)
    -- 배치 카메라로 전환
    if input.is_pressed("Q") then
        local Target = world.find_actor(TargetName)

        if Target == nil then
            print("[CameraTransition] 대상 없음: " .. TargetName)
            return
        end

        RequestViewTarget(Target)

    -- 소유 플레이어로 복귀
    elseif input.is_pressed("E") then
        RequestViewTarget(obj)
    end
end