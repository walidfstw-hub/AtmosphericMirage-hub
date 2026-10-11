// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FAtmosphericMirageModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void CreateSceneViewExtension();

	TSharedPtr<class FMirageSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
	FDelegateHandle PostEngineInitHandle;
};
