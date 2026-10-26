local SlowDownMultiplier = 0.5

function OnBeginOverlap(OtherActor)
    print("[Swamp] BeginOverlap", OtherActor.Name)
    OtherActor.Velocity = OtherActor.Velocity * SlowDownMultiplier
    -- TODO: Fog를 활성화한다.
end

function OnEndOverlap(OtherActor)
    print("[Swamp] EndOverlap", OtherActor.Name)
    -- TODO: Fog를 비활성화한다.
end
