local moveSpeed = 5.0

function BeginPlay()
    -- 초기 속도 설정
    obj.Velocity = Vector(0, 0, 0)
    print("[Velocity Test]", obj.Name, obj.Velocity)
end

function Tick(dt)
    -- 전후 입력 계산
    local axis = 0

    if input.is_down("W") then
        axis = axis + 1
    end

    if input.is_down("S") then
        axis = axis - 1
    end

    -- Actor 방향 기준 월드 속도 계산
    local yaw = math.rad(obj.Rotation.yaw)

    obj.Velocity = Vector(
        math.cos(yaw) * moveSpeed * axis,
        math.sin(yaw) * moveSpeed * axis,
        0
    )
end

function EndPlay()
    print("[Velocity Test End]", obj.Name)
end