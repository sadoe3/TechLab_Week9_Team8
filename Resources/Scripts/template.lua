-- 환경별 누적 시간
local ElapsedTime = 0

function BeginPlay()
    print("[BeginPlay]", obj.UUID)
end

function Tick(DeltaTime)
    ElapsedTime = ElapsedTime + DeltaTime
end

function EndPlay()
    print("[EndPlay]", obj.UUID, ElapsedTime)
end

function OnBeginOverlap(OtherActor)
    -- 겹침 연동 대기
    print("[OnBeginOverlap]", OtherActor ~= nil)
end