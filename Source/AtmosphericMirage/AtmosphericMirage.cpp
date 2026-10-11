// Copyright Epic Games, Inc. All Rights Reserved.

#include "AtmosphericMirage.h"
#include "MirageSceneViewExtension.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SceneViewExtension.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogAtmosphericMirage, Log, All);

void FAtmosphericMirageModule::StartupModule()
{
	const FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/AtmosphericMirage"), ShaderDirectory);
	UE_LOG(LogAtmosphericMirage, Display, TEXT("Mapped /AtmosphericMirage shaders to %s"), *ShaderDirectory);

	if (GEngine)
	{
		CreateSceneViewExtension();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAtmosphericMirageModule::CreateSceneViewExtension);
	}
}

void FAtmosphericMirageModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
	SceneViewExtension.Reset();
}

void FAtmosphericMirageModule::CreateSceneViewExtension()
{
	if (!SceneViewExtension.IsValid())
	{
		SceneViewExtension = FSceneViewExtensions::NewExtension<FMirageSceneViewExtension>();
		UE_LOG(LogAtmosphericMirage, Display, TEXT("Mirage scene-view extension instantiated"));
	}
}

IMPLEMENT_PRIMARY_GAME_MODULE(FAtmosphericMirageModule, AtmosphericMirage, "AtmosphericMirage");
