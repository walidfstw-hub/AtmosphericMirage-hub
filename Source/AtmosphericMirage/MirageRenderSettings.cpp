// Copyright Epic Games, Inc. All Rights Reserved.

#include "MirageRenderSettings.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace
{
	FCriticalSection GMirageSettingsLock;
	FMirageRenderSettings GMirageSettings;
}

void FMirageRenderSettingsStore::Set(const FMirageRenderSettings& InSettings)
{
	FScopeLock Lock(&GMirageSettingsLock);
	GMirageSettings = InSettings;
}

FMirageRenderSettings FMirageRenderSettingsStore::Get()
{
	FScopeLock Lock(&GMirageSettingsLock);
	return GMirageSettings;
}
