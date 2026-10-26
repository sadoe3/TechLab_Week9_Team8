#include "EnginePCH.h"
#include "SoundManager.h"

#include <cassert>
#include <objbase.h>
#include <fmod_errors.h>

bool FSoundManager::Init()
{
	assert(SoundSystem == nullptr);

	// FMOD를 사용할 메인 스레드의 COM을 STA 방식으로 초기화합니다.
	// 개발 중에는 assert로 확인하고, Release에서도 실패를 반환합니다.
	const HRESULT COMResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	assert(SUCCEEDED(COMResult));
	if (FAILED(COMResult))
	{
		return false;
	}

	// 모든 초기화가 끝날 때까지 임시 포인터로 관리합니다.
	FMOD::System* NewSoundSystem = nullptr;
	FMOD_RESULT Result = FMOD::System_Create(&NewSoundSystem);
	if (Result != FMOD_OK)
	{
		CoUninitialize();

		return false;
	}

	// 최대 64개의 재생 채널을 사용하도록 기본 설정으로 초기화합니다.
	Result = NewSoundSystem->init(64, FMOD_INIT_NORMAL, nullptr);
	if (Result != FMOD_OK)
	{
		// 생성한 시스템과 성공했던 COM 초기화를 정리합니다.
		NewSoundSystem->release();
		CoUninitialize();

		return false;
	}

	// 모든 단계가 성공했을 때만 멤버에 저장합니다.
	SoundSystem = NewSoundSystem;

	return true;
}

void FSoundManager::Update()
{
	assert(SoundSystem != nullptr);

	// 게임 프레임마다 FMOD 시스템의 내부 상태를 갱신합니다.
	const FMOD_RESULT Result = SoundSystem->update();

	if (Result != FMOD_OK)
	{
		HTR_LOG(Error, "FMOD update failed: {}", FMOD_ErrorString(Result));
	}

	auto SoundIterator = ActiveSoundChannels.begin();
	while (SoundIterator != ActiveSoundChannels.end())
	{
		const int64 SoundId = SoundIterator->first;
		FMOD::Channel* Channel = SoundIterator->second;

		// 현재 항목을 제거해도 순회를 이어갈 수 있도록 먼저 이동합니다.
		++SoundIterator;

		bool bIsPlaying = false;
		const FMOD_RESULT ChannelResult = Channel->isPlaying(&bIsPlaying);
		const bool bFinished = ChannelResult == FMOD_OK && !bIsPlaying;
		const bool bInvalid = ChannelResult == FMOD_ERR_INVALID_HANDLE ||
			ChannelResult == FMOD_ERR_CHANNEL_STOLEN;

		if (bFinished || bInvalid)
		{
			// 음원 캐시는 유지하고 종료되거나 무효화된 재생 연결만 제거합니다.
			ActiveSoundChannels.Remove(SoundId);
		}
	}
}

void FSoundManager::Shutdown()
{
	assert(SoundSystem != nullptr);

	// FMOD 시스템이 살아 있을 때 BGM 재생과 스트리밍 자원을 정리합니다.
	StopBGM();

	// 시스템 해제 후 무효화될 채널 참조를 남기지 않습니다.
	ActiveSoundChannels.Reset();

	for (const auto& SoundEntry : SoundCacheMap)
	{
		// FMOD가 생성한 음원은 시스템이 살아 있을 때 release()로 해제합니다.
		SoundEntry.second->release();
	}
	// 포인터 항목만 비우는 것으로는 음원이 해제되지 않으므로 마지막에 비웁니다.
	SoundCacheMap.Reset();

	// FMOD 시스템과 관련 자원을 해제합니다.
	SoundSystem->release();
	SoundSystem = nullptr;

	// Init()을 실행했던 메인 스레드에서 COM 사용을 정리합니다.
	CoUninitialize();
}

int64 FSoundManager::PlaySound(const FString& FilePath, bool bLoop, float Volume)
{
	// 초기화가 완료된 상태에서 호출해야 합니다.
	assert(SoundSystem != nullptr);
	assert(Volume >= 0.0f && Volume <= 1.0f);

	// 캐시의 음원을 가져오거나, 없으면 새로 로드합니다.
	FMOD::Sound* SoundOrNull = GetSoundOrNull(FilePath);
	if (SoundOrNull == nullptr)
	{
		return -1;
	}

	// 옵션을 적용하기 전에 소리가 나지 않도록 일시정지 상태로 생성합니다.
	FMOD::Channel* Channel = nullptr;
	const FMOD_RESULT Result = SoundSystem->playSound(SoundOrNull, nullptr, true, &Channel);
	if (Result != FMOD_OK)
	{
		return -1;
	}

	// 공유하는 음원 대신 이번 재생 채널에 반복 여부와 볼륨을 적용합니다.
	const FMOD_MODE LoopMode = bLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
	if (Channel->setMode(LoopMode) != FMOD_OK ||
		Channel->setVolume(Volume) != FMOD_OK ||
		Channel->setPaused(false) != FMOD_OK)
	{
		// 설정에 실패한 채널은 남겨 두지 않습니다.
		Channel->stop();
		return -1;
	}

	// 성공한 재생에 ID를 발급하고 해당 채널과 연결합니다.
	const int64 SoundId = NextSoundId++;
	ActiveSoundChannels.Add(SoundId, Channel);

	return SoundId;
}

bool FSoundManager::StopSound(int64 SoundId)
{
	// 존재하지 않거나 이미 제거된 재생 ID는 실패합니다.
	FMOD::Channel* Channel = ActiveSoundChannels.FindRef(SoundId);
	if (Channel == nullptr)
	{
		return false;
	}

	const FMOD_RESULT Result = Channel->stop();
	if (Result == FMOD_OK ||
		Result == FMOD_ERR_INVALID_HANDLE ||
		Result == FMOD_ERR_CHANNEL_STOLEN)
	{
		// 정지했거나 이미 무효화된 채널의 ID를 제거합니다.
		ActiveSoundChannels.Remove(SoundId);
	}

	return Result == FMOD_OK;
}

bool FSoundManager::SetSoundVolume(int64 SoundId, float Volume)
{
	assert(Volume >= 0.0f && Volume <= 1.0f);

	// 재생 ID에 연결된 채널을 찾습니다.
	FMOD::Channel* Channel = ActiveSoundChannels.FindRef(SoundId);
	if (Channel == nullptr)
	{
		return false;
	}

	// 같은 음원을 사용하는 다른 재생에는 영향을 주지 않습니다.
	const FMOD_RESULT Result = Channel->setVolume(Volume);
	if (Result == FMOD_ERR_INVALID_HANDLE ||
		Result == FMOD_ERR_CHANNEL_STOLEN)
	{
		// Update() 전에 무효화된 채널도 관리 목록에서 제거합니다.
		ActiveSoundChannels.Remove(SoundId);
	}

	return Result == FMOD_OK;
}

bool FSoundManager::SetAllPaused(bool bPaused)
{
	assert(SoundSystem != nullptr);

	// FMOD가 관리하는 기본 그룹을 가져옵니다. 직접 해제하지 않습니다.
	FMOD::ChannelGroup* MasterGroup = nullptr;
	const FMOD_RESULT Result = SoundSystem->getMasterChannelGroup(&MasterGroup);
	if (Result != FMOD_OK)
	{
		return false;
	}

	// 개별 채널의 일시정지 상태와 별도로 그룹 전체를 제어합니다.
	return MasterGroup->setPaused(bPaused) == FMOD_OK;
}

bool FSoundManager::StopAll()
{
	assert(SoundSystem != nullptr);

	// 그룹 정지만으로는 음원이 해제되지 않으므로 BGM부터 정리합니다.
	StopBGM();

	FMOD::ChannelGroup* MasterGroup = nullptr;
	const FMOD_RESULT Result = SoundSystem->getMasterChannelGroup(&MasterGroup);
	if (Result != FMOD_OK)
	{
		return false;
	}

	// 남은 효과음을 모두 정지하되, 재사용할 효과음 캐시는 유지합니다.
	const FMOD_RESULT StopResult = MasterGroup->stop();
	if (StopResult == FMOD_OK)
	{
		// 전체 정지에 성공했으므로 재생 연결을 모두 제거합니다.
		// NextSoundId는 초기화하지 않습니다.
		ActiveSoundChannels.Reset();
	}

	return StopResult == FMOD_OK;
}

bool FSoundManager::PlayBGM(const FString& FilePath, float Volume)
{
	assert(SoundSystem != nullptr);
	assert(Volume >= 0.0f && Volume <= 1.0f);

	// 새 BGM 준비에 실패해도 기존 BGM은 유지합니다.
	FMOD::Sound* NewSound = nullptr;
	const FMOD_MODE Mode = FMOD_2D | FMOD_LOOP_NORMAL;
	FMOD_RESULT Result = SoundSystem->createStream(FilePath.c_str(), Mode, nullptr, &NewSound);
	if (Result != FMOD_OK)
	{
		return false;
	}

	// 볼륨 설정 전 소리가 나지 않도록 일시정지 상태로 생성합니다.
	FMOD::Channel* NewChannel = nullptr;
	Result = SoundSystem->playSound(NewSound, nullptr, true, &NewChannel);
	if (Result != FMOD_OK)
	{
		NewSound->release();
		return false;
	}

	// 설정과 재생 시작에 실패하면 새로 준비한 자원만 정리합니다.
	if (NewChannel->setVolume(Volume) != FMOD_OK ||
		NewChannel->setPaused(false) != FMOD_OK)
	{
		NewChannel->stop();
		NewSound->release();
		return false;
	}

	// 새 BGM이 정상적으로 시작된 뒤 기존 BGM을 교체합니다.
	StopBGM();
	BGMSound = NewSound;
	BGMChannel = NewChannel;

	return true;
}

void FSoundManager::StopBGM()
{
	if (BGMChannel != nullptr)
	{
		// 음원을 해제하기 전에 채널 재생부터 정지합니다.
		BGMChannel->stop();
		BGMChannel = nullptr;
	}

	if (BGMSound != nullptr)
	{
		// 스트리밍 음원의 파일과 관련 자원을 해제합니다.
		BGMSound->release();
		BGMSound = nullptr;
	}
}

bool FSoundManager::SetBGMPaused(bool bPaused)
{
	if (BGMChannel == nullptr)
	{
		return false;
	}

	// 재생 위치를 유지한 채 BGM만 일시정지하거나 재개합니다.
	return BGMChannel->setPaused(bPaused) == FMOD_OK;
}

bool FSoundManager::SetBGMVolume(float Volume)
{
	assert(Volume >= 0.0f && Volume <= 1.0f);

	if (BGMChannel == nullptr)
	{
		return false;
	}

	// 다른 효과음에 영향을 주지 않고 BGM 채널의 볼륨만 변경합니다.
	return BGMChannel->setVolume(Volume) == FMOD_OK;
}

FMOD::Sound* FSoundManager::GetSoundOrNull(const FString& FilePath)
{
	// 오디오 시스템을 초기화한 뒤 호출해야 합니다.
	assert(SoundSystem != nullptr);

	// 이미 로드한 음원이면 같은 객체를 재사용합니다.
	if (FMOD::Sound* CachedSound = SoundCacheMap.FindRef(FilePath))
	{
		return CachedSound;
	}

	// 거리 감쇠 없는 2D 효과음을 PCM 데이터로 메모리에 로드합니다.
	FMOD::Sound* Sound = nullptr;
	const FMOD_MODE Mode = FMOD_2D | FMOD_CREATESAMPLE;
	const FMOD_RESULT Result = SoundSystem->createSound(FilePath.c_str(), Mode, nullptr, &Sound);

	// 파일 없음이나 지원하지 않는 형식 등의 오류를 기록합니다.
	if (Result != FMOD_OK)
	{
		const char* ErrorMessage = FMOD_ErrorString(Result);
		HTR_LOG(Error, "FMOD load failed [{}]: {}", FilePath, ErrorMessage);
		return nullptr;
	}

	// 성공한 음원만 캐시에 등록합니다. 아직 재생하지는 않습니다.
	SoundCacheMap.Add(FilePath, Sound);

	return Sound;
}
