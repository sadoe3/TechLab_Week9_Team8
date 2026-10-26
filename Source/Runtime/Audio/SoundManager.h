#pragma once

#include <fmod.hpp>

#include "Core/EngineString.h"
#include "Core/Types.h"
#include "Containers/Map.h"

class FSoundManager
{
public:
	FSoundManager() = default;
	~FSoundManager() = default;

	FSoundManager(const FSoundManager&) = delete;
	FSoundManager& operator=(const FSoundManager&) = delete;

	bool Init();
	void Update();
	void Shutdown();

	// 성공하면 양수 재생 ID, 실패하면 -1을 반환합니다.
	int64 PlaySound(const FString& FilePath, bool bLoop = false, float Volume = 1.0f);
	// 해당 재생 ID의 효과음만 정지합니다.
	bool StopSound(int64 SoundId);
	// 해당 재생의 볼륨만 변경합니다. 범위는 0.0~1.0입니다.
	bool SetSoundVolume(int64 SoundId, float Volume);

	// 모든 효과음과 BGM을 일시정지하거나 재개합니다.
	bool SetAllPaused(bool bPaused);
	// BGM 자원을 정리하고 모든 재생을 정지합니다.
	bool StopAll();

	// 새 BGM 재생에 성공하면 기존 BGM을 교체합니다.
	bool PlayBGM(const FString& FilePath, float Volume = 1.0f);
	// 현재 BGM을 정지하고 스트리밍 음원을 해제합니다.
	void StopBGM();
	bool SetBGMPaused(bool bPaused);
	// 현재 BGM 채널의 볼륨을 변경합니다. 범위는 0.0~1.0입니다.
	bool SetBGMVolume(float Volume);

private:
	FMOD::Sound* GetSoundOrNull(const FString& FilePath);
	
private:
	FMOD::System* SoundSystem = nullptr;

	// 효과음 캐시와 별도로 관리하는 현재 BGM 음원과 재생 채널입니다.
	FMOD::Sound* BGMSound = nullptr;
	FMOD::Channel* BGMChannel = nullptr;
	
	// 파일 경로를 키로 사용해 로드한 효과음을 보관합니다.
	TMap<FString, FMOD::Sound*> SoundCacheMap;

	// 개별 재생의 ID와 채널을 연결합니다.
	TMap<int64, FMOD::Channel*> ActiveSoundChannels;
	// 이전 ID가 새 재생에 연결되지 않도록 전체 정지에서도 유지합니다.
	int64 NextSoundId = 1;
};
