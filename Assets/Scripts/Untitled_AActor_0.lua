function Tick(dt)
    if input.is_pressed("W") then
        print("[Input] W pressed", input.is_down("W"))
    end

    if input.is_released("W") then
        print("[Input] W released", input.is_down("W"))
    end

    if input.is_pressed("Ctrl") then
        print("[Input] Ctrl pressed")
    end

    if input.is_pressed("R") then
        print("[Input] R pressed")
    end
end