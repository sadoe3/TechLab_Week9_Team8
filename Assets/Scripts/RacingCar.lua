-- 가속도는 엔진 단위 / 초², 조향 속도는 도 / 초입니다.
local Acceleration = 4.0
local SteeringSpeed = 90.0
local FullSteeringSpeed = 2.0
local CoastingDamping = 0.5
local BrakeDamping = 1.5
local StopSpeed = 0.01
local SwampMaxSpeed = 2.0
local SwampOverlapCount = 0
local BaseFOV = 60.0
local SpeedFOVIncrease = 30.0
local FOVInterpSpeed = 20.0
local WallParticleComponentName = "WallParticles"
local WallParticleCount = 8
local WallParticleOffset = 0.03
local WallHitInterval = 0.1
local WallHitCooldown = 0.0

local WallHitSoundPath = "Assets/Audio/SFX/WallHit.wav"
local WallHitSoundVolume = 0.7

local EngineSoundId = -1
local EngineVolume = 0.2
local EnginePitch = 1.0

local BGMPath = "Assets/Audio/BGM/FirstMap.mp3"

local Race = require("RaceState")
local score = 0

function BeginPlay()
    script.set_tick_group("PrePhysics")
    WallHitCooldown = 0.0
    score = 0
    audio.play_bgm(BGMPath, 0.7)
    EngineSoundId = audio.play("Assets/Audio/SFX/EngineLoop.wav", true, EngineVolume)
end

function Tick(DeltaTime)
    WallHitCooldown = math.max(0.0, WallHitCooldown - DeltaTime)
    if Race.State ~= "Running" then
        obj.Velocity = Vector(0, 0, 0)
        return
    end
    local MoveAxis = 0.0
    local TurnAxis = 0.0
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
    local SpeedLimit = SwampOverlapCount > 0 and math.min(MaxSpeed, SwampMaxSpeed) or MaxSpeed
    Speed = math.max(-SpeedLimit, math.min(Speed, SpeedLimit))
    if (IsBraking or Throttle == 0.0) and math.abs(Speed) < StopSpeed then
        Speed = 0.0
    end

    -- 속도에 따라 엔진 음 조절
    if EngineSoundId >= 0 then
        -- 전진·후진 속력을 최대 속력에 대한 0~1 비율로 바꿉니다.
        local SpeedRatio = 0.0
        if MaxSpeed > 0.0 then
            SpeedRatio = math.min(math.abs(Speed) / MaxSpeed, 1.0)
        end

        -- 현재 설정대로 볼륨은 0.1 ~ 0.4로 변화합니다.
        local TargetVolume = 0.1 + (0.3 * SpeedRatio)

        -- 피치는 정지 시 1.0, 최고 속력에서 2.0이 됩니다.
        local TargetPitch = 1.0 + (1.0 * SpeedRatio)

        -- BlendAlpa: 프레임 시간에 맞춰 부드럽게 따라갈 비율입니다.
        --local BlendAlpha = 1.0 - math.exp(-6.0 * DeltaTime)

        -- 볼륨과 피치를 각각 목표값 쪽으로 이동시킵니다.
        --EngineVolume = EngineVolume + (TargetVolume - EngineVolume) * BlendAlpha
        --EnginePitch = EnginePitch + (TargetPitch - EnginePitch) * BlendAlpha

        EngineVolume = TargetVolume
        EnginePitch = TargetPitch

        -- 같은 엔진음 재생에 두 값을 적용합니다.
        audio.set_volume(EngineSoundId, EngineVolume)
        audio.set_pitch(EngineSoundId, EnginePitch)
    end

    -- 후진은 차체 회전을 반대로 적용해야 A/D 입력 방향으로 이동합니다.
    local SteeringScale = math.max(-1.0, math.min(Speed / FullSteeringSpeed, 1.0))
    Rotation.yaw = Rotation.yaw + Steering * SteeringSpeed * SteeringScale * DeltaTime
    obj.Rotation = Rotation

    -- 조향은 속력을 바꾸지 않습니다. 실제 이동과 충돌 처리는 Character Tick이 담당합니다.
    Yaw = math.rad(Rotation.yaw)
    obj.Velocity = Vector(math.cos(Yaw) * Speed, math.sin(Yaw) * Speed, Velocity.z)


    local SpeedRatio = MaxSpeed > 0.0 and math.min(math.abs(Speed) / MaxSpeed, 1.0) or 0.0
    local TargetFOV = math.max(1.0, math.min(BaseFOV + SpeedFOVIncrease * SpeedRatio, 179.0))
    local Alpha = 1.0 - math.exp(-FOVInterpSpeed * math.max(0.0, DeltaTime))
    obj.CameraFOV = obj.CameraFOV + (TargetFOV - obj.CameraFOV) * Alpha
end

-- 측면 Blocking Hit의 파티클 재생
function OnHit(OtherActor, Hit, OtherComponent)
    if Race.State ~= "Running" then
        return
    end
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

    audio.play(WallHitSoundPath, false, WallHitSoundVolume)

    if emitted > 0 then
        WallHitCooldown = WallHitInterval

    end
end

local CoinParticleComponentName = "CoinParticles"
local CoinParticleCount = 4

local CoinSoundPath = "Assets/Audio/SFX/Coin.wav"
local CoinSoundVolume = 1.0

-- 코인 획득 / 점수 처리 / 파티클 설정
function OnBeginOverlap(OtherActor)
    if OtherActor:IsSwamp() then
        SwampOverlapCount = SwampOverlapCount + 1
        return
    end
    if Race.State ~= "Running" or not OtherActor:IsValid() then
        return
    end
    if(OtherActor.Label == "Coin") then
        local position = OtherActor.Location
        score = score + 5
        Race.CoinCount = Race.CoinCount + 1
        print("Score " .. score)
        game.set_score(score)

        OtherActor:Destroy()
        
        audio.play(CoinSoundPath, false, CoinSoundVolume)
        
        obj:EmitParticleBurst(CoinParticleComponentName, position, Vector(0, 0, 1), CoinParticleCount)
    end
end

function OnEndOverlap(OtherActor)
    if OtherActor:IsSwamp() then
        SwampOverlapCount = math.max(0, SwampOverlapCount - 1)
    end
end

function EndPlay()
    if EngineSoundId >= 0 then
        audio.stop(EngineSoundId)

        EngineSoundId = -1
    end

    audio.stop_bgm()
end
