-- 공용 상태 및 레이스 설정
local Race = require("RaceState")
-- 플레이어 자동차 이름
local PlayerActorName = "ARacingCar_0"
local CountdownSeconds = 3.0
local CheckpointCount = 4

local TotalCoinCount = 20
local TargetTime = 10.0     -- 만점 시간
local TimeLimit = 50.0      -- 시간 점수 0점 
local CoinWeight = 0.6      -- 코인:시간 = 6:4
local TimeWeight = 0.4      
local MaxScore = 10000

local function Clamp01(Value)
    if Value < 0.0 then return 0.0 end
    if Value > 1.0 then return 1.0 end
    return Value
end

local function BuildFinalScore(FinishTime, CoinCount)
    local CoinRatio = Clamp01(CoinCount / TotalCoinCount)
    local TimeRatio = Clamp01((TimeLimit - FinishTime) / (TimeLimit - TargetTime))

    local CoinPoints = math.floor(MaxScore * CoinWeight * CoinRatio)
    local TimePoints = math.floor(MaxScore * TimeWeight * TimeRatio)

    return CoinPoints + TimePoints, CoinPoints, TimePoints
end

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
                    -- 주행을 종료하고 이번 점수와 시간을 확정합니다.
                    Race.State = "Finished"
                    Race.FinishScore = game.get_score()
                    Race.FinishTime = Race.ElapsedTime
                    Race.Player.Velocity = Vector(0, 0, 0)

                    -- 주행 중 넓어진 시야를 기본 FOV로 복원합니다.
                    -- RacingCar.lua의 BaseFOV(60.0)와 같은 값을 사용합니다.
                    Race.Player.CameraFOV = 60.0

                    -- 최고 기록을 각각 갱신하고 파일에 저장합니다.
                    local BestScore, BestFinishTime, SaveSucceeded =
                        game.update_best_records(
                            Race.FinishScore,
                            Race.FinishTime)

                    Race.BestScore = BestScore
                    Race.BestFinishTime = BestFinishTime

                    if not SaveSucceeded then
                        print("[Race] Failed to save leaderboard records")
                    end

                    -- UI에서 읽을 기록을 C++에 전달한 뒤 완주 UI를 요청합니다.
                    game.set_race_result(
                        Race.FinishScore,
                        Race.FinishTime,
                        Race.BestScore,
                        Race.BestFinishTime)
                    game.show_finish_ui()

                    print(string.format(
                        "[Race] Finished: score=%d, time=%.2f seconds",
                        Race.FinishScore,
                        Race.FinishTime))

                    local FinalScore, CoinPoints, TimePoints =
                        BuildFinalScore(Race.FinishTime, Race.CoinCount)

                    print(string.format("[Score] Coin  %d/%d -> %d",
                        Race.CoinCount, TotalCoinCount, CoinPoints))
                    print(string.format("[Score] Time  %.2fs (target %.0f / limit %.0f) -> %d",
                        Race.FinishTime, TargetTime, TimeLimit, TimePoints))
                    print(string.format("[Score] Final %d / %d", FinalScore, MaxScore))
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
    -- 새 주행에는 이전 결과 화면이 남지 않도록 합니다.
    game.reset_result_ui()
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
    -- C++에 보관된 결과 화면 상태를 씬 종료 시 정리합니다.
    game.reset_result_ui()
    GoDisplayRemaining = 0.0
    game.set_countdown("")
    Race.Stop()
end
