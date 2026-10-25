-- 임시 게임 시작 입력
function Tick(DeltaTime)
    if input.is_pressed("E") then
        world.change_scene("Assets/Scenes/MainScene.scene")
    end
end