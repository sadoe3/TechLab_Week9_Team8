#pragma once

#include "Engine.h"

class UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine, UEngine)

public:
	FEngineConfig GetConfig() const override;

	virtual bool Init() override;
	virtual void PreTick(float DeltaTime) override;
	virtual void Tick(float DeltaTime) override;
	virtual void PostTick(float DeltaTime) override;
	virtual void PreExit() override;

private:
	// 실행 World의 비소유 참조
	UWorld* GameWorld = nullptr;
};