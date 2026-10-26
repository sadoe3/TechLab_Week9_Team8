#pragma once

#include "BillboardComponent.h"
#include "Text/Font.h"

class UTextRenderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Text)
		PROPERTY(TextSize)
		PROPERTY(Font)
		REFLECT_END()

public:
	UTextRenderComponent();
	virtual ~UTextRenderComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual const FStaticMeshData* GetMeshData() const override;


	// 텍스트는 FTextRenderer가 따로 그리므로 렌더 패킷을 만들지 않음
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue) override {}

	const FString& GetText() const { return Text; }

	bool IsDrawOnTop() const { return bDrawOnTop; }	// 맨 앞에 그리기
	void SetDrawOnTop(bool bInDrawOnTop) { bDrawOnTop = bInDrawOnTop; }
	void SetText(const FString& InText) { Text = InText; }

	UFont* GetFont() const { return Font; }
	void SetFont(UFont* InFont) { Font = InFont; }

	float GetTextSize() const { return TextSize < 0 ? 0.1f : TextSize; }
	void SetTextSize(float InTextSize) { TextSize = InTextSize; }

private:
	bool RebuildPickingMesh() const;

	FString Text = "Text";
	UFont* Font = nullptr;
	float TextSize = 1.0f;
	bool bDrawOnTop = false;

	mutable FStaticMeshData PickingMesh;
	mutable FString CachedText;
	mutable float CachedTextSize = -1.0f;
	mutable UFont* CachedFont = nullptr;
	mutable bool bHasPickingMesh = false;
};
