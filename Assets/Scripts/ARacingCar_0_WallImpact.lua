-- 현재 주행 상태 공유
local Race = require("RaceState")

-- 벽 충돌 파티클 설정
local WallParticleComponentName = "WallParticles"
local WallParticleCount = 8
local WallParticleOffset = 0.03

-- 벽 충돌 효과음 설정
local WallHitSoundPath = "Assets/Audio/SFX/WallHit.wav"
local WallHitSoundVolume = 0.7

-- 반복 충돌의 재생 간격
local WallHitInterval = 0.1
local WallHitCooldown = 0.0

-- 벽 충돌 Shake 프리셋
local WallImpactShake = {
    pattern = "Wave",
    duration = 0.25,
    blend_in = 0.02,
    blend_out = 0.15,
    single_instance = false,
    play_space = "CameraLocal",

    --y = { amplitude = 0.15, frequency = 18.0, phase = 0.0 },
    --z = { amplitude = 0.09, frequency = 23.0, phase = 0.0 },
    roll = { amplitude = 0.8, frequency = 20.0, phase = 0.0 }
}

function BeginPlay()
    -- 물리 이동 전 쿨다운 갱신
    script.set_tick_group("PrePhysics")
end

function Tick(DeltaTime)
    -- 재생 대기 시간 감소
    WallHitCooldown = math.max(0.0, WallHitCooldown - DeltaTime)
end

function OnHit(OtherActor, Hit, OtherComponent)
    -- 주행 상태와 재생 간격 확인
    if Race.State ~= "Running" or WallHitCooldown > 0.0 then
        return
    end

    -- 유효한 Blocking Hit 확인
    if not OtherActor:IsValid() or not OtherComponent:IsValid() or
        not Hit.bBlockingHit then
        return
    end

    -- 바닥·천장 방향 충돌 제외
    if math.abs(Hit.ImpactNormal.z) > 0.5 then
        return
    end

    -- 파티클 방출 결과와 무관한 재생 간격 제한
    WallHitCooldown = WallHitInterval

    -- 충돌 지점의 벽 파티클 방출
    local position = Hit.ImpactPoint + Hit.ImpactNormal * WallParticleOffset
    obj:EmitParticleBurst(
        WallParticleComponentName, position, Hit.ImpactNormal, WallParticleCount)

    -- 충돌 효과음과 카메라 Shake 재생
    audio.play(WallHitSoundPath, false, WallHitSoundVolume)
    world.start_camera_shake("WallImpact", WallImpactShake, 1.0)
end