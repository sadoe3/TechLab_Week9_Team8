local Version = "v2"
local ElapsedTime = 0
local TickCount = 0

function BeginPlay()
    -- 최초 시작에만 출력되어야 한다.
    print("BeginPlay", Version)
end

function Tick(DeltaTime)
    ElapsedTime = ElapsedTime + DeltaTime

    -- 매 프레임 로그가 쌓이지 않도록 1초마다 출력한다.
    if ElapsedTime >= 1.0 then
        ElapsedTime = 0
        TickCount = TickCount + 1

        print("Tick", Version, TickCount)
    end
end

function EndPlay()
    -- 종료 시점에 적용된 버전을 확인한다.
    print("EndPlay", Version)
end
