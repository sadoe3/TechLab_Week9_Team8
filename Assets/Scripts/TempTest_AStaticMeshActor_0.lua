-- 예제 상태
local Version = "v1"
local Started = false
local ElapsedTime = 0
local Frame = 0
local NextHeartbeat = 0.5

-- 실행 시점 출력
local function Log(Message)
    print(string.format(
        "[%s][Actor %d][Frame %d][%.2fs] %s",
        Version, obj.UUID, Frame, ElapsedTime, Message))
end

-- 대기와 재개 확인
local function RunSequence()
    Log("1. Start: 2 Second Wait")

    wait(2.0)
    Log("2. Wait End")

    wait(0)
    Log("3. Resume On Next Tick")

    wait(2.0)
    Log("4. Coroutine End")
end

function BeginPlay()
    Log("BeginPlay")
end

function Tick(DeltaTime)
    Frame = Frame + 1
    ElapsedTime = ElapsedTime + DeltaTime

    -- 환경별 작업 한 번 등록
    if not Started then
        Started = true
        local ID = start_coroutine(RunSequence)
        Log("Coroutine Register ID=" .. ID)
    end

    -- 대기 중 일반 Tick 진행 확인
    if ElapsedTime >= NextHeartbeat then
        NextHeartbeat = ElapsedTime + 0.5
        Log("During Normal Tick")
    end
end

function EndPlay()
    Log("EndPlay")
end