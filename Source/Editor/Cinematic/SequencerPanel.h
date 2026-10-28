#pragma once

#include <functional>
#include "Editor/EditorUI/EditorPanel.h"
#include "Cinematic/SequencePlayer.h"

// 시퀀스 편집 상태와 파일·재생 요청 관리
class FSequencerPanel : public IEditorPanel
{
public:
	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return "Sequencer"; }

	// 재생 시점의 PIE World 조회
	void SetSequenceWorldQuery(std::function<UWorld*()> InQuery) { SequenceWorldQuery = std::move(InQuery); }
	void SetEditorWorldQuery(std::function<UWorld*()> InQuery) { EditorWorldQuery = std::move(InQuery); }

private:
	void DrawSequenceSection();
	void DrawToolbar();
	void DrawTimeline();
	void DrawKeyEditor();
	void DrawTrackMenu();
	void AddTrack(ESequenceTrackType Type, const FString& ActorName);
	void OpenKeyEditor(int32 TrackIndex, int32 KeyIndex, float Time);
	void SeekPreview(float Time);
	UWorld* GetEditingWorld() const;
	bool ValidateTargets(UWorld* World);
	bool GetSequencePath(FString& OutPath);
	float SnapTime(float Time) const;

	// 창 표시 여부와 독립적인 편집 상태
	FSequenceData Sequence;
	int32 SelectedTrack = -1;
	int32 SelectedKey = -1;
	FString SelectedActor;
	ImGuiTextFilter ActorFilter;
	TMap<FString, bool> CollapsedBindings;
	char SequencePath[512] = "Sequences/Intro.sequence";
	FString SequenceStatus;
	float Playhead = 0.0f;
	float PixelsPerSecond = 100.0f;
	float TimeOffset = 0.0f;
	bool bSnap = true;
	uint32 PreviewGeneration = 0;

	// 적용 전 키 편집 내용
	bool bOpenKeyPopup = false;
	int32 EditingTrack = -1;
	int32 EditingKey = -1;
	FSequenceKey EditedKey;
	char KeyTarget[512] = {};
	FString KeyError;

	std::function<UWorld*()> SequenceWorldQuery;
	std::function<UWorld*()> EditorWorldQuery;
};
