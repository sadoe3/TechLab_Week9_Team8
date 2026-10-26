-- 가속도는 엔진 단위 / 초², 조향 속도는 도 / 초입니다.
local Acceleration = 4.0
local SteeringSpeed = 90.0
local FullSteeringSpeed = 2.0
local CoastingDamping = 2.0
local BrakeDamping = 20.0
local StopSpeed = 0.01

local score = 0

function Tick(DeltaTime)
    -- W/S는 추진력, A/D는 조향만 담당합니다. 동시 입력은 S와 A가 우선합니다.
    local Throttle = 0.0
    if input.is_down("S") then
        Throttle = -1.0
    elseif input.is_down("W") then
        Throttle = 1.0
    end

    local Steering = 0.0
    if input.is_down("A") then
        Steering = -1.0
    elseif input.is_down("D") then
        Steering = 1.0
    end

    -- 속력의 부호로 후진을 표현하므로 차체를 180도 돌릴 필요가 없습니다.
    local Velocity = obj.Velocity
    local Rotation = obj.Rotation
    local Yaw = math.rad(Rotation.yaw)
    local Speed = math.sqrt(Velocity.x * Velocity.x + Velocity.y * Velocity.y)
    if Velocity.x * math.cos(Yaw) + Velocity.y * math.sin(Yaw) < 0.0 then
        Speed = -Speed
    end

    local IsBraking = input.is_down("Space")
    if IsBraking then
        Speed = Speed * math.exp(-BrakeDamping * DeltaTime)
    elseif Throttle ~= 0.0 then
        Speed = Speed + Throttle * Acceleration * DeltaTime
    else
        Speed = Speed * math.exp(-CoastingDamping * DeltaTime)
    end

    local MaxSpeed = math.max(0.0, obj.MaxSpeed)
    Speed = math.max(-MaxSpeed, math.min(Speed, MaxSpeed))
    if (IsBraking or Throttle == 0.0) and math.abs(Speed) < StopSpeed then
        Speed = 0.0
    end

    -- 후진은 차체 회전을 반대로 적용해야 A/D 입력 방향으로 이동합니다.
    local SteeringScale = math.max(-1.0, math.min(Speed / FullSteeringSpeed, 1.0))
    Rotation.yaw = Rotation.yaw + Steering * SteeringSpeed * SteeringScale * DeltaTime
    obj.Rotation = Rotation

    -- 조향은 속력을 바꾸지 않습니다. 실제 이동과 충돌 처리는 Character Tick이 담당합니다.
    Yaw = math.rad(Rotation.yaw)
    obj.Velocity = Vector(math.cos(Yaw) * Speed, math.sin(Yaw) * Speed, Velocity.z)
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