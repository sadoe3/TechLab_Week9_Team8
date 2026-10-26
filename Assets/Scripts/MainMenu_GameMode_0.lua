function BeginPlay()
    audio.play_bgm("Assets/Audio/BGM/TitleScreen.mp3", 0.7)
end

-- 임시 게임 시작 입력
function Tick(DeltaTime)
    if input.is_pressed("E") then
        world.change_scene("Assets/Scenes/MainScene.scene")
    end
end

function EndPlay()
    audio.stop_bgm()
end
