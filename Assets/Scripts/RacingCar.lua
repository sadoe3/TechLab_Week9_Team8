-- 이동 속도는 초당 엔진 단위
-- 회전 속도는 초당 각도입니다.
local MovementSpeed = 2.0
local RotationSpeed = 90.0

local score = 0

function Tick(DeltaTime)
    local MoveAxis = 0.0
    local TurnAxis = 0.0

    if input.is_down("W") 
    then 
        MoveAxis = MoveAxis + 1.0 
    end
    
    if input.is_down("S") 
    then 
        MoveAxis = MoveAxis - 1.0 
    end

    if input.is_down("A") 
    then 
        TurnAxis = TurnAxis - 1.0 
    end
    
    if input.is_down("D") 
    then 
        TurnAxis = TurnAxis + 1.0 
    end

    -- A/D로 차량을 회전합니다. 복사한 회전 값을 수정한 뒤, 액터에 다시 적용합니다.
    local Rotation = obj.Rotation
    Rotation.yaw = Rotation.yaw + TurnAxis * RotationSpeed * DeltaTime
    obj.Rotation = Rotation

    -- 삼각함수에 사용할 라디안 각도로 변환하고, XY 평면의 전방 방향을 계산합니다.
    local YawRadians = math.rad(Rotation.yaw)
    local Forward = Vector(math.cos(YawRadians), math.sin(YawRadians), 0.0)

    -- Character의 Sweep 이동용 속도 설정
    obj.Velocity = Forward * (MoveAxis * MovementSpeed)
end

function OnBeginOverlap(OtherActor)
    if(OtherActor.Label == "Coin")
    then
        score = score + 5;
        print("Score " .. score)
        game.set_score(score);
        OtherActor:Destroy()
    end
end