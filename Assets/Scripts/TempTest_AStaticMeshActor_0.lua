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

-- 대기 후 부드러운 이동
local function RunSequence()
    local StartLocation = obj.Location
    local Distance = 10.0
    local Duration = 2.0

    Log("1. 이동 전 1초 대기")
    wait(1.0)

    local MoveStartTime = ElapsedTime

    while true do
        local Alpha = math.min(
            (ElapsedTime - MoveStartTime) / Duration, 1.0)

        obj.Location = StartLocation + Vector(Distance * Alpha, 0, 0)

        if Alpha >= 1.0 then
            break
        end

        -- 다음 소유 컴포넌트 Tick까지 대기
        wait(0)
    end

    Log("2. 이동 완료")
    wait(1.0)

    obj.Location = StartLocation
    Log("3. 원래 위치로 복귀")
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