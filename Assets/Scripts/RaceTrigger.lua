-- 공용 레이스 상태
local Race = require("RaceState")

-- 트리거 통과 전달
function OnBeginOverlap(OtherActor)
    Race.NotifyTrigger(obj.Label, OtherActor)
end