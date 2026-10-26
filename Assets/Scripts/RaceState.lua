-- World 내 공용 레이스 상태
local Race = {
    State = "NotStarted",
    Player = nil,
    CountdownRemaining = 0.0,
    ElapsedTime = 0.0,
    CheckpointCount = 0,
    LastCheckpoint = 0,
    FinishTime = 0.0,
    PendingTriggers = {}
}

-- 새 주행 초기화
function Race.Reset(Player, CountdownSeconds, CheckpointCount)
    Race.State = "Countdown"
    Race.Player = Player
    Race.CountdownRemaining = CountdownSeconds
    Race.ElapsedTime = 0.0
    Race.CheckpointCount = CheckpointCount
    Race.LastCheckpoint = 0
    Race.FinishTime = 0.0
    Race.PendingTriggers = {}
end

-- 실행 상태 및 외부 참조 정리
function Race.Stop()
    Race.State = "NotStarted"
    Race.Player = nil
    Race.PendingTriggers = {}
end

-- 플레이어 통과 알림 등록
function Race.NotifyTrigger(TriggerLabel, OtherActor)
    if Race.State ~= "Running" or not OtherActor:IsValid() then
        return
    end

    local Player = Race.Player
    if not Player or not Player:IsValid() or OtherActor.UUID ~= Player.UUID then
        return
    end

    Race.PendingTriggers[#Race.PendingTriggers + 1] = TriggerLabel
end

return Race