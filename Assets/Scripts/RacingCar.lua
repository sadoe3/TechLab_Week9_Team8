-- 이동 속도는 초당 엔진 단위
-- 회전 속도는 초당 각도입니다.
local MovementSpeed = 2.0
local RotationSpeed = 90.0
local WallParticleComponentName = "WallParticles"
local WallParticleCount = 8
local WallParticleOffset = 0.03
local WallHitInterval = 0.1
local WallHitCooldown = 0.0

local score = 0

function Tick(DeltaTime)
    WallHitCooldown = math.max(0.0, WallHitCooldown - DeltaTime)
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

-- 측면 Blocking Hit의 파티클 재생
function OnHit(OtherActor, Hit, OtherComponent)
    if WallHitCooldown > 0.0 or not OtherActor:IsValid() or
        not OtherComponent:IsValid() or not Hit.bBlockingHit then
        return
    end

    -- 바닥·천장 방향 충돌 제외
    if math.abs(Hit.ImpactNormal.z) > 0.5 then
        return
    end

    local position = Hit.ImpactPoint + Hit.ImpactNormal * WallParticleOffset
    local emitted = obj:EmitParticleBurst(
        WallParticleComponentName, position, Hit.ImpactNormal, WallParticleCount)

    if emitted > 0 then
        WallHitCooldown = WallHitInterval
    end
end

local CoinParticleComponentName = "CoinParticles"
local CoinParticleCount = 4

-- 코인 획득 / 점수 처리 / 파티클 설정
function OnBeginOverlap(OtherActor)
    if(OtherActor.Label == "Coin")
    then
    local position = OtherActor.Location
    score = score + 5
    print("Score " .. score)
    OtherActor:Destroy()
    obj:EmitParticleBurst(CoinParticleComponentName, position, Vector(0, 0, 1), CoinParticleCount)
    end
end
