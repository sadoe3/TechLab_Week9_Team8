local OverlapCount = 0

function OnBeginOverlap(OtherActor)
    print("[Swamp] BeginOverlap", OtherActor.Name)
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
