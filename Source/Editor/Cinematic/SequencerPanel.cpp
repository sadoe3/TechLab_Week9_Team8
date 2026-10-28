#include "EnginePCH.h"
#include "Editor/Cinematic/SequencerPanel.h"
#include "Camera/PlayerCameraManager.h"
#include "Cinematic/SequenceSerializer.h"
#include "Engine/Level.h"
#include <algorithm>
#include <cmath>

void FSequencerPanel::OnRender()
{
	// 독립 창의 최초 크기와 닫기 상태
	ImGui::SetNextWindowSize(ImVec2(1150.0f, 640.0f), ImGuiCond_FirstUseEver);
	bool bOpen = IsOpen();
	if (!ImGui::Begin(GetPanelName(), &bOpen))
	{
		SetOpen(bOpen);
		ImGui::End();
		return;
	}
	SetOpen(bOpen);
	DrawSequenceSection();
	ImGui::End();
}

void FSequencerPanel::DrawSequenceSection()
{
	if (!ImGui::CollapsingHeader("Sequence", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	// 패널에서 시작한 재생의 시간만 동기화
	if (PreviewGeneration != 0 && PreviewGeneration == FSequencePlayer::GetPlaybackGeneration())
	{
		Playhead = FSequencePlayer::GetPosition();
	}
	DrawToolbar();
	DrawTimeline();
	DrawKeyEditor();
	ImGui::TextWrapped("Double-click a channel to enter a key value. Double-click a diamond to edit it. Ctrl+Wheel: zoom. Shift+Wheel: pan.");
	ImGui::TextWrapped("World-space channels. Snap: 30 fps. Ruler scrubbing previews in PIE only. Edits apply on the next Play.");
	if (!SequenceStatus.empty())
	{
		ImGui::TextWrapped("%s", SequenceStatus.c_str());
	}
}

UWorld* FSequencerPanel::GetEditingWorld() const
{
	// 재생 중인 월드 우선 조회
	UWorld* World = SequenceWorldQuery ? SequenceWorldQuery() : nullptr;
	return World ? World : (EditorWorldQuery ? EditorWorldQuery() : nullptr);
}

float FSequencerPanel::SnapTime(float Time) const
{
	// 초 단위 시간과 30 fps 스냅
	const float Snapped = bSnap ? std::round(Time * 30.0f) / 30.0f : Time;
	return std::clamp(Snapped, 0.0f, Sequence.Length);
}

void FSequencerPanel::DrawToolbar()
{
	UWorld* PlayWorld = SequenceWorldQuery ? SequenceWorldQuery() : nullptr;
	ImGui::BeginDisabled(!PlayWorld || (Sequence.Tracks.IsEmpty() && Sequence.Length <= 0.0f));
	if (ImGui::Button("Play"))
	{
		// 편집 배열의 독립 재생과 선택 시각 적용
		if (Sequence.Validate(SequenceStatus) && ValidateTargets(PlayWorld))
		{
			const float Start = Playhead >= Sequence.Length ? 0.0f : Playhead;
			FSequencePlayer::Play(Sequence, PlayWorld);
			FSequencePlayer::Seek(Start);
			FSequencePlayer::SetPaused(false);
			PreviewGeneration = FSequencePlayer::GetPlaybackGeneration();
			SequenceStatus = "Playing edited timeline in PIE.";
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	const bool bOwnsPlayback = PreviewGeneration != 0 && PreviewGeneration == FSequencePlayer::GetPlaybackGeneration();
	ImGui::BeginDisabled(!bOwnsPlayback || !FSequencePlayer::IsPlaying());
	if (ImGui::Button(FSequencePlayer::IsPaused() ? "Resume" : "Pause"))
	{
		FSequencePlayer::SetPaused(!FSequencePlayer::IsPaused());
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		FSequencePlayer::Stop();
		if (APlayerCameraManager* Manager = PlayWorld ? PlayWorld->GetPlayerCameraManager() : nullptr)
		{
			Manager->StopCameraFade();
			Manager->StopCameraLetterBox();
		}
		PreviewGeneration = 0;
		Playhead = 0.0f;
		SequenceStatus = "Playback stopped. Actor transforms remain at the current pose.";
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	float NewPosition = Playhead;
	if (ImGui::InputFloat("##Position", &NewPosition, 0.0f, 0.0f, "%.3f s") && std::isfinite(NewPosition))
	{
		SeekPreview(SnapTime(NewPosition));
	}
	ImGui::SameLine();
	ImGui::Text("frame %.0f", static_cast<double>(Playhead) * 30.0);
	ImGui::SameLine();
	if (ImGui::GetContentRegionAvail().x < 420.0f)
	{
		ImGui::NewLine();
	}
	ImGui::SetNextItemWidth(90.0f);
	if (ImGui::InputFloat("Length##Length", &Sequence.Length, 0.0f, 0.0f, "%.2f s"))
	{
		// 마지막 키와 카메라 전환 종료보다 짧은 재생 길이 방지
		float MinimumLength = 0.0f;
		for (const FSequenceTrack& Track : Sequence.Tracks)
		{
			for (const FSequenceKey& Key : Track.Keys)
			{
				const float End = Key.Time + (Track.Type == ESequenceTrackType::ViewTarget ? Key.BlendParams.BlendTime : 0.0f);
				MinimumLength = std::max(MinimumLength, End);
			}
		}
		Sequence.Length = std::isfinite(Sequence.Length) ? std::max(MinimumLength, Sequence.Length) : MinimumLength;
		Playhead = std::min(Playhead, Sequence.Length);
	}
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &Sequence.bLoop);
	ImGui::SameLine();
	ImGui::Checkbox("Hold End", &Sequence.bHoldEnd);
	ImGui::SetNextItemWidth(90.0f);
	if (ImGui::InputFloat("Rate##Rate", &Sequence.Rate, 0.0f, 0.0f, "%.2f"))
	{
		Sequence.Rate = std::isfinite(Sequence.Rate) ? std::clamp(Sequence.Rate, 0.01f, 100.0f) : 1.0f;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &bSnap);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", PlayWorld ? "PIE Preview" : "Start PIE to preview");

	ImGui::Separator();
	if (ImGui::Button("+ Actor"))
	{
		ImGui::OpenPopup("Add Actor");
	}
	ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 0.0f), ImVec2(600.0f, 350.0f));
	if (ImGui::BeginPopup("Add Actor"))
	{
		ActorFilter.Draw("Filter");
		UWorld* World = GetEditingWorld();
		ULevel* Level = World ? World->GetPersistentLevel() : nullptr;
		if (Level)
		{
			for (AActor* Actor : Level->GetActors())
			{
				if (Actor && !Actor->IsPendingDestroy() && ActorFilter.PassFilter(Actor->GetName().c_str())
					&& ImGui::Selectable(Actor->GetName().c_str()))
				{
					SelectedActor = Actor->GetName();
					AddTrack(ESequenceTrackType::LocationX, SelectedActor);
					AddTrack(ESequenceTrackType::LocationY, SelectedActor);
					AddTrack(ESequenceTrackType::LocationZ, SelectedActor);
				}
			}
		}
		else
		{
			ImGui::TextDisabled("No scene world available.");
		}
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Track"))
	{
		ImGui::OpenPopup("Add Track");
	}
	if (ImGui::BeginPopup("Add Track"))
	{
		DrawTrackMenu();
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(SelectedTrack < 0);
	if (ImGui::Button("Key"))
	{
		OpenKeyEditor(SelectedTrack, -1, Playhead);
		EditedKey.Value = FSequencePlayer::GetTrackValue(Sequence.Tracks[SelectedTrack], GetEditingWorld());
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("New"))
	{
		Sequence = FSequenceData();
		SelectedTrack = SelectedKey = -1;
		SelectedActor.clear();
		CollapsedBindings.Empty();
		Playhead = TimeOffset = 0.0f;
		PreviewGeneration = 0;
		SequenceStatus = "New timeline. Save to keep your edits.";
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(std::clamp(ImGui::GetContentRegionAvail().x - 140.0f, 140.0f, 420.0f));
	ImGui::InputText("##SequencePath", SequencePath, sizeof(SequencePath));
	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		FString Path;
		if (GetSequencePath(Path) && Sequence.Validate(SequenceStatus))
		{
			SequenceStatus = SequenceSerializer::Save(Sequence, Path) ? "Saved: " + Path : "Save failed. Check Output Log.";
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		FString Path;
		FSequenceData Loaded;
		if (GetSequencePath(Path))
		{
			if (SequenceSerializer::Load(Loaded, Path))
			{
				Sequence = std::move(Loaded);
				SelectedTrack = SelectedKey = -1;
				SelectedActor.clear();
				CollapsedBindings.Empty();
				Playhead = TimeOffset = 0.0f;
				PreviewGeneration = 0;
				SequenceStatus = "Loaded: " + Path;
			}
			else
			{
				SequenceStatus = "Load failed. Current edits preserved. Check Output Log.";
			}
		}
	}
}

void FSequencerPanel::DrawTrackMenu()
{
	// 바인딩 대상과 화면 공용 채널 구분
	ImGui::TextDisabled("Actor: %s", SelectedActor.empty() ? "Select an actor" : SelectedActor.c_str());
	for (int32 Index = 0; Index < SequenceSerializer::GetTrackTypeCount(); ++Index)
	{
		const ESequenceTrackType Type = SequenceSerializer::GetTrackTypeAt(Index);
		const bool bGlobal = Type >= ESequenceTrackType::Fade;
		ImGui::BeginDisabled(!bGlobal && SelectedActor.empty());
		if (ImGui::Selectable(SequenceSerializer::ToString(Type)))
		{
			AddTrack(Type, bGlobal ? FString() : SelectedActor);
		}
		ImGui::EndDisabled();
	}
}

void FSequencerPanel::AddTrack(ESequenceTrackType Type, const FString& ActorName)
{
	// 동일 속성의 중복 제어 방지
	for (int32 Index = 0; Index < Sequence.Tracks.Num(); ++Index)
	{
		if (Sequence.Tracks[Index].Type == Type && Sequence.Tracks[Index].TargetName == ActorName)
		{
			SelectedTrack = Index;
			SelectedKey = -1;
			return;
		}
	}
	FSequenceTrack Track;
	Track.Type = Type;
	Track.TargetName = ActorName;
	SelectedTrack = static_cast<int32>(Sequence.Tracks.Add(Track));
	SelectedKey = -1;
	CollapsedBindings.Add(ActorName, false);
}

void FSequencerPanel::DrawTimeline()
{
	// 라벨 열과 시간축의 독립 좌표
	const float Width = std::max(320.0f, ImGui::GetContentRegionAvail().x);
	const float LabelWidth = std::clamp(Width * 0.25f, 160.0f, 250.0f);
	const float RowHeight = ImGui::GetFrameHeight() + 4.0f;
	const ImVec2 Ruler = ImGui::GetCursorScreenPos();
	const float TimeOriginX = Ruler.x + LabelWidth;
	ImDrawList* Draw = ImGui::GetWindowDrawList();
	ImGui::InvisibleButton("##TimeRuler", ImVec2(Width, RowHeight));
	const bool bRulerHovered = ImGui::IsItemHovered();
	if (ImGui::IsItemActive() && ImGui::GetIO().MousePos.x >= TimeOriginX)
	{
		SeekPreview(SnapTime(TimeOffset + (ImGui::GetIO().MousePos.x - TimeOriginX) / PixelsPerSecond));
	}
	Draw->AddRectFilled(Ruler, ImVec2(Ruler.x + Width, Ruler.y + RowHeight), IM_COL32(32, 33, 35, 255));
	Draw->AddText(ImVec2(Ruler.x + 8, Ruler.y + 4), IM_COL32(180, 180, 185, 255), "Actor / Channel");
	// 줌 수준에 따른 눈금 간격
	float TickInterval = 1.0f;
	while (TickInterval * PixelsPerSecond < 65.0f)
	{
		TickInterval *= 2.0f;
	}
	while (TickInterval * PixelsPerSecond > 180.0f)
	{
		TickInterval *= 0.5f;
	}
	Draw->PushClipRect(ImVec2(TimeOriginX, Ruler.y), ImVec2(Ruler.x + Width, Ruler.y + RowHeight), true);
	const double FirstTick = std::ceil(static_cast<double>(TimeOffset) / TickInterval) * TickInterval;
	const int32 VisibleTickCount = static_cast<int32>((Width - LabelWidth) / (TickInterval * PixelsPerSecond)) + 2;
	for (int32 TickIndex = 0; TickIndex < VisibleTickCount; ++TickIndex)
	{
		const double Time = FirstTick + static_cast<double>(TickIndex) * TickInterval;
		const float X = TimeOriginX + static_cast<float>((Time - TimeOffset) * PixelsPerSecond);
		char Label[32];
		snprintf(Label, sizeof(Label), "%.2f", Time);
		Draw->AddText(ImVec2(X + 3, Ruler.y + 3), IM_COL32(180, 180, 185, 255), Label);
		Draw->AddLine(ImVec2(X, Ruler.y + RowHeight - 6), ImVec2(X, Ruler.y + RowHeight), IM_COL32(110, 110, 115, 255));
	}
	const float PlayheadX = TimeOriginX + (Playhead - TimeOffset) * PixelsPerSecond;
	if (PlayheadX >= TimeOriginX && PlayheadX <= Ruler.x + Width)
	{
		Draw->AddTriangleFilled(ImVec2(PlayheadX - 6, Ruler.y), ImVec2(PlayheadX + 6, Ruler.y), ImVec2(PlayheadX, Ruler.y + 9), IM_COL32(240, 110, 70, 255));
	}
	Draw->PopClipRect();

	const float TimelineHeight = std::max(180.0f, ImGui::GetContentRegionAvail().y - 125.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("##TimelineRows", ImVec2(0, TimelineHeight), ImGuiChildFlags_Borders);
	ImGui::PopStyleVar();
	Draw = ImGui::GetWindowDrawList();
	const ImVec2 ContentStart = ImGui::GetCursorScreenPos();
	const float RowWidth = ImGui::GetContentRegionAvail().x;
	const float RowTimeX = TimeOriginX;
	TArray<FString> Groups;
	for (const FSequenceTrack& Track : Sequence.Tracks)
	{
		if (std::find(Groups.begin(), Groups.end(), Track.TargetName) == Groups.end())
		{
			Groups.Add(Track.TargetName);
		}
	}
	for (const FString& Group : Groups)
	{
		ImGui::PushID(Group.c_str());
		bool* Collapsed = CollapsedBindings.Find(Group);
		const bool bCollapsed = Collapsed && *Collapsed;
		const FString Label = FString(bCollapsed ? "> " : "v ") + (Group.empty() ? "Screen / Camera Cuts" : Group);
		if (ImGui::Selectable(Label.c_str(), SelectedActor == Group, 0, ImVec2(RowWidth, RowHeight)))
		{
			SelectedActor = Group;
			if (ImGui::GetIO().MousePos.x < ContentStart.x + 22.0f)
			{
				CollapsedBindings.Add(Group, !bCollapsed);
			}
		}
		if (ImGui::BeginPopupContextItem("BindingMenu"))
		{
			SelectedActor = Group;
			DrawTrackMenu();
			ImGui::EndPopup();
		}
		ImGui::PopID();
		if (bCollapsed)
		{
			continue;
		}
		for (int32 TrackIndex = 0; TrackIndex < Sequence.Tracks.Num(); ++TrackIndex)
		{
			const FSequenceTrack& Track = Sequence.Tracks[TrackIndex];
			if (Track.TargetName != Group)
			{
				continue;
			}
			ImGui::PushID(TrackIndex);
			const ImVec2 Row = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##TrackRow", ImVec2(RowWidth, RowHeight));
			const bool bHovered = ImGui::IsItemHovered();
			const float MouseX = ImGui::GetIO().MousePos.x;
			int32 HoveredKey = -1;
			for (int32 KeyIndex = 0; KeyIndex < Track.Keys.Num(); ++KeyIndex)
			{
				const float X = RowTimeX + (Track.Keys[KeyIndex].Time - TimeOffset) * PixelsPerSecond;
				if (bHovered && MouseX >= RowTimeX && std::fabs(MouseX - X) <= 8.0f)
				{
					HoveredKey = KeyIndex;
				}
			}
			if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				SelectedTrack = TrackIndex;
				SelectedKey = HoveredKey;
				SelectedActor = Group;
			}
			if (bHovered && MouseX >= RowTimeX && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				OpenKeyEditor(TrackIndex, HoveredKey, SnapTime(TimeOffset + (MouseX - RowTimeX) / PixelsPerSecond));
			}
			Draw->AddRectFilled(Row, ImVec2(Row.x + RowWidth, Row.y + RowHeight), SelectedTrack == TrackIndex ? IM_COL32(57, 70, 79, 255) : IM_COL32(37, 39, 42, 255));
			Draw->PushClipRect(Row, ImVec2(RowTimeX - 4, Row.y + RowHeight), true);
			Draw->AddText(ImVec2(Row.x + 22, Row.y + 4), IM_COL32(220, 220, 225, 255), SequenceSerializer::ToString(Track.Type));
			Draw->PopClipRect();
			Draw->PushClipRect(ImVec2(RowTimeX, Row.y), ImVec2(Row.x + RowWidth, Row.y + RowHeight), true);
			const float EndX = std::min(Row.x + RowWidth, RowTimeX + (Sequence.Length - TimeOffset) * PixelsPerSecond);
			Draw->AddRectFilled(ImVec2(RowTimeX, Row.y + 7), ImVec2(std::max(RowTimeX, EndX), Row.y + RowHeight - 7), IM_COL32(45, 77, 90, 180));
			for (int32 TickIndex = 0; TickIndex < VisibleTickCount; ++TickIndex)
			{
				const double Time = FirstTick + static_cast<double>(TickIndex) * TickInterval;
				const float X = RowTimeX + static_cast<float>((Time - TimeOffset) * PixelsPerSecond);
				Draw->AddLine(ImVec2(X, Row.y), ImVec2(X, Row.y + RowHeight), IM_COL32(70, 72, 75, 255));
			}
			for (int32 KeyIndex = 0; KeyIndex < Track.Keys.Num(); ++KeyIndex)
			{
				const FSequenceKey& Key = Track.Keys[KeyIndex];
				const float X = RowTimeX + (Key.Time - TimeOffset) * PixelsPerSecond;
				if (X < RowTimeX || X > Row.x + RowWidth)
				{
					continue;
				}
				const float Y = Row.y + RowHeight * 0.5f;
				const ImU32 Color = SelectedTrack == TrackIndex && SelectedKey == KeyIndex ? IM_COL32(255, 210, 90, 255) : IM_COL32(190, 160, 205, 255);
				Draw->AddQuadFilled(ImVec2(X, Y - 6), ImVec2(X + 6, Y), ImVec2(X, Y + 6), ImVec2(X - 6, Y), Color);
			}
			if (PlayheadX >= RowTimeX && PlayheadX <= Row.x + RowWidth)
			{
				Draw->AddLine(ImVec2(PlayheadX, Row.y), ImVec2(PlayheadX, Row.y + RowHeight), IM_COL32(240, 110, 70, 255), 2.0f);
			}
			Draw->PopClipRect();
			if (HoveredKey >= 0)
			{
				const FSequenceKey& Key = Track.Keys[HoveredKey];
				if (Track.Type == ESequenceTrackType::ViewTarget)
				{
					ImGui::SetTooltip("%.3f s -> %s\nBlend: %.3f s (%s)", Key.Time, Key.TargetName.c_str(),
						Key.BlendParams.BlendTime, SequenceSerializer::ToString(Key.BlendParams.BlendFunction));
				}
				else
				{
					ImGui::SetTooltip("%.3f s = %.3f (%s)%s", Key.Time, Key.Value, SequenceSerializer::ToString(Key.Interpolation), Key.bUseInitialValue ? " [playback start value]" : "");
				}
			}
			ImGui::PopID();
		}
	}
	const bool bTimelineHovered = ImGui::IsWindowHovered();
	ImGui::EndChild();
	if (bTimelineHovered || bRulerHovered)
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if (IO.KeyCtrl && IO.MouseWheel != 0.0f)
		{
			PixelsPerSecond = std::clamp(PixelsPerSecond * std::pow(1.2f, IO.MouseWheel), 20.0f, 1000.0f);
		}
		else if (IO.KeyShift && IO.MouseWheel != 0.0f)
		{
			TimeOffset = std::clamp(TimeOffset - IO.MouseWheel * 100.0f / PixelsPerSecond, 0.0f, Sequence.Length);
		}
	}
	ImGui::BeginDisabled(SelectedTrack < 0);
	if (ImGui::Button("Delete Track"))
	{
		Sequence.Tracks.RemoveAt(SelectedTrack);
		SelectedTrack = SelectedKey = -1;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(SelectedTrack < 0 || SelectedKey < 0);
	if (ImGui::Button("Delete Key"))
	{
		Sequence.Tracks[SelectedTrack].Keys.RemoveAt(SelectedKey);
		SelectedKey = -1;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%d tracks | %.2f s", Sequence.Tracks.Num(), Sequence.Length);
}

void FSequencerPanel::OpenKeyEditor(int32 TrackIndex, int32 KeyIndex, float Time)
{
	// 기존 키 수정 또는 선택 시각의 새 키 초안
	EditingTrack = TrackIndex;
	EditingKey = KeyIndex;
	const FSequenceTrack& Track = Sequence.Tracks[TrackIndex];
	EditedKey = KeyIndex >= 0 ? Track.Keys[KeyIndex] : FSequenceKey();
	if (KeyIndex < 0)
	{
		EditedKey.Time = SnapTime(Time);
		const float CurrentValue = FSequencePlayer::GetTrackValue(Track, GetEditingWorld());
		EditedKey.Value = Track.Keys.IsEmpty() ? CurrentValue : Track.Evaluate(EditedKey.Time, CurrentValue);
	}
	strncpy_s(KeyTarget, sizeof(KeyTarget), EditedKey.TargetName.c_str(), _TRUNCATE);
	KeyError.clear();
	bOpenKeyPopup = true;
}

void FSequencerPanel::DrawKeyEditor()
{
	if (bOpenKeyPopup)
	{
		ImGui::OpenPopup("Edit Key");
		bOpenKeyPopup = false;
	}
	if (!ImGui::BeginPopupModal("Edit Key", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}
	const FSequenceTrack& Track = Sequence.Tracks[EditingTrack];
	ImGui::Text("%s / %s", Track.TargetName.empty() ? "Screen" : Track.TargetName.c_str(), SequenceSerializer::ToString(Track.Type));
	ImGui::SetNextItemWidth(240.0f);
	ImGui::InputFloat("Time##KeyTime", &EditedKey.Time, 0.0f, 0.0f, "%.3f s");
	if (Track.Type == ESequenceTrackType::ViewTarget)
	{
		ImGui::InputText("Target Name##KeyTarget", KeyTarget, sizeof(KeyTarget));
		// 키 시각부터 시작하는 카메라 전환 설정
		ImGui::SetNextItemWidth(240.0f);
		ImGui::InputFloat("Blend Time##KeyBlendTime", &EditedKey.BlendParams.BlendTime, 0.0f, 0.0f, "%.3f s");
		if (ImGui::BeginCombo("Blend Function##KeyBlendFunction", SequenceSerializer::ToString(EditedKey.BlendParams.BlendFunction)))
		{
			for (EViewTargetBlendFunction Function : { EViewTargetBlendFunction::Linear, EViewTargetBlendFunction::Cubic,
				EViewTargetBlendFunction::EaseIn, EViewTargetBlendFunction::EaseOut, EViewTargetBlendFunction::EaseInOut })
			{
				if (ImGui::Selectable(SequenceSerializer::ToString(Function), EditedKey.BlendParams.BlendFunction == Function))
				{
					EditedKey.BlendParams.BlendFunction = Function;
				}
			}
			ImGui::EndCombo();
		}
		if (EditedKey.BlendParams.BlendFunction == EViewTargetBlendFunction::EaseIn
			|| EditedKey.BlendParams.BlendFunction == EViewTargetBlendFunction::EaseOut
			|| EditedKey.BlendParams.BlendFunction == EViewTargetBlendFunction::EaseInOut)
		{
			ImGui::SetNextItemWidth(240.0f);
			ImGui::InputFloat("Blend Exp##KeyBlendExp", &EditedKey.BlendParams.BlendExp, 0.0f, 0.0f, "%.3f");
		}
		ImGui::TextDisabled("Blend starts at this key. Zero seconds makes an instant cut.");
		ImGui::TextDisabled("Finish the blend before the next ViewTarget key and Length.");
	}
	else
	{
		ImGui::Checkbox("Use playback start value", &EditedKey.bUseInitialValue);
		ImGui::BeginDisabled(EditedKey.bUseInitialValue);
		ImGui::SetNextItemWidth(240.0f);
		ImGui::InputFloat("Value##KeyValue", &EditedKey.Value, 0.0f, 0.0f, "%.3f");
		ImGui::EndDisabled();
		if (ImGui::BeginCombo("Interpolation##KeyInterpolation", SequenceSerializer::ToString(EditedKey.Interpolation)))
		{
			for (ESequenceInterpolation Mode : { ESequenceInterpolation::Constant, ESequenceInterpolation::Linear, ESequenceInterpolation::Cubic })
			{
				if (ImGui::Selectable(SequenceSerializer::ToString(Mode), EditedKey.Interpolation == Mode))
				{
					EditedKey.Interpolation = Mode;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("Interpolation applies from this key to the next key.");
	}
	if (!KeyError.empty())
	{
		ImGui::TextWrapped("%s", KeyError.c_str());
	}
	if (ImGui::Button("Apply"))
	{
		// 입력과 변경 배열 검증 후 기존 키 교체
		if (!std::isfinite(EditedKey.Time) || EditedKey.Time < 0.0f || EditedKey.Time > Sequence.Length || !std::isfinite(EditedKey.Value))
		{
			KeyError = "Enter a finite value and a time within the sequence length.";
		}
		else if (Track.Type == ESequenceTrackType::ViewTarget && KeyTarget[0] == '\0')
		{
			KeyError = "Enter an actor's actual name for this view target.";
		}
		else
		{
			FSequenceData Proposed = Sequence;
			FSequenceTrack& TargetTrack = Proposed.Tracks[EditingTrack];
			if (EditingKey >= 0)
			{
				TargetTrack.Keys.RemoveAt(EditingKey);
			}
			EditedKey.Time = SnapTime(EditedKey.Time);
			EditedKey.TargetName = KeyTarget;
			const int32 NewKeyIndex = TargetTrack.AddKey(EditedKey);
			if (Proposed.Validate(KeyError))
			{
				Sequence.Tracks[EditingTrack] = std::move(TargetTrack);
				SelectedTrack = EditingTrack;
				SelectedKey = NewKeyIndex;
				SequenceStatus = "Key updated. Play again to apply edits.";
				ImGui::CloseCurrentPopup();
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void FSequencerPanel::SeekPreview(float Time)
{
	// 에디터 씬 수정 없이 PIE에서만 시간 이동
	Playhead = Time;
	UWorld* World = SequenceWorldQuery ? SequenceWorldQuery() : nullptr;
	if (!World || (Sequence.Tracks.IsEmpty() && Sequence.Length <= 0.0f))
	{
		return;
	}
	if (PreviewGeneration == 0 || PreviewGeneration != FSequencePlayer::GetPlaybackGeneration())
	{
		if (!Sequence.Validate(SequenceStatus) || !ValidateTargets(World))
		{
			return;
		}
		FSequencePlayer::Play(Sequence, World);
		PreviewGeneration = FSequencePlayer::GetPlaybackGeneration();
	}
	FSequencePlayer::SetPaused(true);
	FSequencePlayer::Seek(Time);
}

bool FSequencerPanel::ValidateTargets(UWorld* World)
{
	// 재생 직전 실제 액터 이름 검증
	ULevel* Level = World ? World->GetPersistentLevel() : nullptr;
	if (!Level)
	{
		SequenceStatus = "No PIE level available.";
		return false;
	}
	const auto Exists = [Level](const FString& Name)
	{
		for (AActor* Actor : Level->GetActors())
		{
			if (Actor && !Actor->IsPendingDestroy() && Actor->GetFName() == FName(Name))
			{
				return true;
			}
		}
		return false;
	};
	for (const FSequenceTrack& Track : Sequence.Tracks)
	{
		if (Track.Type < ESequenceTrackType::Fade && !Exists(Track.TargetName))
		{
			SequenceStatus = "Target not found: " + Track.TargetName;
			return false;
		}
		if (Track.Type == ESequenceTrackType::ViewTarget)
		{
			for (const FSequenceKey& Key : Track.Keys)
			{
				if (!Exists(Key.TargetName))
				{
					SequenceStatus = "Camera-cut target not found: " + Key.TargetName;
					return false;
				}
			}
		}
	}
	return true;
}

bool FSequencerPanel::GetSequencePath(FString& OutPath)
{
	// 에셋 스캐너와 분리된 시퀀스 경로 제한
	const fs::path Path = fs::path(SequencePath).lexically_normal();
	if (Path.is_absolute() || Path.has_root_name() || Path.empty() || *Path.begin() != "Sequences" || Path.extension() != ".sequence")
	{
		SequenceStatus = "Use Sequences/<name>.sequence (repository-relative).";
		return false;
	}
	OutPath = Path.generic_string();
	return true;
}
