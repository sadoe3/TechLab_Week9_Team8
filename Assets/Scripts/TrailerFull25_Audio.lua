-- 촬영 복사본의 일반 재생용 사운드
local Duration = 25.0
local GoalTime = 19.92
local Elapsed = 0.0
local Active = false
local Finished = false
local GoalPlayed = false
local EngineSoundId = -1
local GoalSoundId = -1
local OwnsBGM = false

-- 구간 강도의 부드러운 전환
local function Smooth(Value)
	local Alpha = math.max(0.0, math.min(1.0, Value))
	return Alpha * Alpha * (3.0 - 2.0 * Alpha)
end

-- 이번 촬영에서 시작한 재생 정리
local function StopAudio()
	if EngineSoundId >= 0 then
		audio.stop(EngineSoundId)
		EngineSoundId = -1
	end
	if GoalSoundId >= 0 then
		audio.stop(GoalSoundId)
		GoalSoundId = -1
	end
	if OwnsBGM then
		audio.stop_bgm()
		OwnsBGM = false
	end
	Active = false
end

-- PIE 시작 시 대기, 시퀀서 Play 시 음원 시작
function BeginPlay()
	script.set_tick_group("PrePhysics")
end

function Tick(DeltaTime)
	if not world.is_sequence_playing() then
		StopAudio()
		Elapsed = 0.0
		Finished = false
		GoalPlayed = false
		return
	end
	if Finished then
		return
	end
	if not Active then
		OwnsBGM = audio.play_bgm("Assets/Audio/BGM/FirstMap.mp3", 0.0)
		EngineSoundId = audio.play("Assets/Audio/SFX/EngineLoop.wav", true, 0.0)
		Active = true
	end

	-- 일반 재생의 누적 시간, 시퀀서 탐색 동기화 제외
	Elapsed = math.min(Duration, Elapsed + math.max(0.0, DeltaTime))
	local FadeIn = Smooth(Elapsed / 0.5)
	local FadeOut = 1.0 - Smooth((Elapsed - 24.3) / 0.7)
	local MusicVolume = 0.28 + 0.22 * Smooth((Elapsed - 10.5) / 1.0)
	if Elapsed >= 19.0 and Elapsed < 22.0 then
		MusicVolume = 0.36
	end
	if OwnsBGM then
		audio.set_bgm_volume(MusicVolume * FadeIn * FadeOut)
	end

	-- 정차 엔진음, 출발 가속, 결승 감속
	if EngineSoundId >= 0 then
		local Acceleration = Smooth((Elapsed - 10.4) / 1.6)
		local EngineVolume = 0.16 + 0.26 * Acceleration
		local EnginePitch = 0.85 + 0.95 * Acceleration
		if Elapsed >= 19.0 then
			local Deceleration = Smooth((Elapsed - 19.0) / 2.4)
			EnginePitch = 1.8 - 0.95 * Deceleration
			EngineVolume = 0.42 * (1.0 - Smooth((Elapsed - 21.2) / 0.7))
		end
		audio.set_volume(EngineSoundId, EngineVolume * FadeIn)
		audio.set_pitch(EngineSoundId, EnginePitch)
		if Elapsed >= 21.9 then
			audio.stop(EngineSoundId)
			EngineSoundId = -1
		end
	end

	-- 결승선 통과 시 기존 골인 효과음
	if not GoalPlayed and Elapsed >= GoalTime then
		GoalSoundId = audio.play("Assets/Audio/SFX/GoalIn.wav", false, 0.7)
		GoalPlayed = true
	end
	if Elapsed >= Duration then
		StopAudio()
		Finished = true
	end
end

-- PIE 종료 시 남은 재생 정리
function EndPlay()
	StopAudio()
end
