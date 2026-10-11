// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../../MirageController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMirageControllerDefaultsTest,
	"AtmosphericMirage.Controller.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMirageControllerDefaultsTest::RunTest(const FString& Parameters)
{
	const AMirageController* Controller = GetDefault<AMirageController>();
	TestNotNull(TEXT("Mirage controller class has a default object"), Controller);
	if (Controller)
	{
		TestTrue(TEXT("Mirage is enabled by default"), Controller->bEnabled);
		TestEqual(TEXT("Default type is inferior"), Controller->MirageType, EMirageType::Inferior);
		TestTrue(TEXT("Refraction strength is positive"), Controller->RefractionStrength > 0.0f);
		TestTrue(TEXT("Horizon is normalized"), Controller->Horizon >= 0.0f && Controller->Horizon <= 1.0f);
		TestTrue(TEXT("Band height is positive"), Controller->BandHeight > 0.0f);
	}
	return true;
}

#endif
