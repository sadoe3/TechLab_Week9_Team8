-- 공용 상태 및 레이스 설정
local Race = require("RaceState")
-- 플레이어 자동차 이름
local PlayerActorName = "ARacingCar_0"
local CountdownSeconds = 3.0
local CheckpointCount = 4

-- 출발 신호, 딜레이 시간
local GoText = "출발!"
local GoDisplaySeconds = 1.0
local GoDisplayRemaining = 0.0

-- 체크포인트 Label과 통과 순번
local CheckpointIndices = {
    Checkpoint_1 = 1,
    Checkpoint_2 = 2,
    Checkpoint_3 = 3,
    Checkpoint_4 = 4
}

-- 체크포인트 및 완주 판정
local function ProcessPendingTriggers()
    local TriggerCount = #Race.PendingTriggers

    for Index = 1, TriggerCount do
        local Label = Race.PendingTriggers[Index]
        Race.PendingTriggers[Index] = nil

        if Race.State == "Running" then
            local CheckpointIndex = CheckpointIndices[Label]

            if CheckpointIndex
                and CheckpointIndex == Race.LastCheckpoint + 1
                and CheckpointIndex <= Race.CheckpointCount then
                Race.LastCheckpoint = CheckpointIndex

                print(string.format("[Race] Checkpoint: %d/%d",
                    Race.LastCheckpoint, Race.CheckpointCount))

            elseif Label == "Finish" then
                if Race.LastCheckpoint == Race.CheckpointCount then
                    -- 완주 상태와 기록 확정
                    Race.State = "Finished"
                    Race.FinishTime = Race.ElapsedTime
                    Race.Player.Velocity = Vector(0, 0, 0)

                    print(string.format("[Race] Finished: %.2f seconds",
                        Race.FinishTime))
                else
                    print(string.format("[Race] Checkpoints required: %d/%d",
                        Race.LastCheckpoint, Race.CheckpointCount))
                end
            end
        end
    end
end

-- 출발 상태 초기화
function BeginPlay()
    script.set_tick_group("PostPhysics")

    local Player = world.find_actor(PlayerActorName)
    if not Player or not Player:IsValid() then
        Race.Stop()
        error("[Race] Player is unavailable: " .. PlayerActorName)
    end

    Race.Reset(Player, CountdownSeconds, CheckpointCount)
    Player.Velocity = Vector(0, 0, 0)
    game.set_score(0)

    GoDisplayRemaining = 0.0
    game.set_countdown(tostring(math.ceil(CountdownSeconds)))
    game.set_race_time(0.0)

    print(string.format("[Race] Countdown: %d", math.ceil(CountdownSeconds)))
end

-- 물리 이동 이후 레이스 진행 갱신
function Tick(DeltaTime)
    if Race.State == "NotStarted" then
        return
    end

    -- 실행 중 플레이어 삭제 처리
    if not Race.Player or not Race.Player:IsValid() then
        print("[Race] Player is unavailable")
        Race.Stop()
        return
    end

    if Race.State == "Countdown" then
        local PreviousNumber = math.ceil(Race.CountdownRemaining)
        Race.CountdownRemaining = math.max(0.0, Race.CountdownRemaining - DeltaTime)
        local CurrentNumber = math.ceil(Race.CountdownRemaining)

        if CurrentNumber == 0 then
            Race.State = "Running"
            GoDisplayRemaining = GoDisplaySeconds
            game.set_countdown(GoText)
            print("[Race] Go")
        elseif CurrentNumber ~= PreviousNumber then
            game.set_countdown(tostring(CurrentNumber))
            print(string.format("[Race] Countdown: %d", CurrentNumber))
        end

        return
    end

    -- 출발 문구 잠깐!
    if GoDisplayRemaining > 0.0 then
        GoDisplayRemaining = GoDisplayRemaining - DeltaTime
        if GoDisplayRemaining <= 0.0 then
            GoDisplayRemaining = 0.0
            game.set_countdown("")
        end
    end

    if Race.State == "Running" then
        -- 이번 이동 프레임의 시간 누적
        Race.ElapsedTime = Race.ElapsedTime + DeltaTime
        -- 주행 시간 
        game.set_race_time(Race.ElapsedTime)
        -- 이번 이동에서 수신한 통과 알림 처리
        ProcessPendingTriggers()
    end
end

-- 레이스 소유권 정리
function EndPlay()
    GoDisplayRemaining = 0.0
    game.set_countdown("")
    Race.Stop()
end
