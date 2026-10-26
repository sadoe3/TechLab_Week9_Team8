local SlowDownMultiplier = 0.2
local MinSlowDownSpeed = 2.0
local OverlapCount = 0

function OnBeginOverlap(OtherActor)
    print("[Swamp] BeginOverlap", OtherActor.Name)
    local Velocity = OtherActor.Velocity
    local SpeedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y
    if SpeedSquared >= MinSlowDownSpeed * MinSlowDownSpeed then
        OtherActor.Velocity = Velocity * SlowDownMultiplier
    end
    OverlapCount = OverlapCount + 1
    obj.FogEnabled = true
end

function OnEndOverlap(OtherActor)
    print("[Swamp] EndOverlap", OtherActor.Name)
    OverlapCount = math.max(0, OverlapCount - 1)
    obj.FogEnabled = OverlapCount > 0
end

function EndPlay()
    obj.FogEnabled = false
end
