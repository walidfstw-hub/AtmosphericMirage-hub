// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MirageController.generated.h"

UENUM(BlueprintType)
enum class EMirageType : uint8
{
	Inferior UMETA(DisplayName = "Inferior (hot surface)"),
	Superior UMETA(DisplayName = "Superior (cold coast)")
};

/**
 * World-facing control for the project refraction renderer. Place one in a level and tune the
 * horizon band in the viewport. The renderer itself is a C++ scene-view extension backed by HLSL.
 */
UCLASS(BlueprintType, Blueprintable)
class ATMOSPHERICMIRAGE_API AMirageController : public AActor
{
	GENERATED_BODY()

public:
	AMirageController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage")
	EMirageType MirageType = EMirageType::Inferior;

	/** Small-scale turbulence applied after physically integrating the ray. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage", meta = (ClampMin = "0.0", ClampMax = "0.08", UIMin = "0.0", UIMax = "0.03"))
	float RefractionStrength = 0.012f;

	/** Screen-space horizon; zero is the top of the image and one is the bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Horizon = 0.56f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float BandHeight = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float TurbulenceSpeed = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float TurbulenceScale = 32.0f;

	/** Amount of inverted horizon image mixed into an inferior mirage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Inferior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReflectionAmount = 0.48f;

	/** Vertical lift/compression used by a superior mirage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Superior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LoomingAmount = 0.32f;

	/** Atmospheric pressure used by n(z)=1+7.86e-4*P_kPa/T_K(z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float PressureKPa = 101.325f;

	/** Air temperature at LayerBaseWorldZ. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "180.0", ClampMax = "380.0"))
	float BaseTemperatureK = 318.0f;

	/** Surface dT/dz in kelvin per meter. It decays exponentially with height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "-200.0", ClampMax = "200.0"))
	float TemperatureGradientKPerMeter = -0.8f;

	/** Exponential e-folding height of the boundary-layer temperature gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "0.05", ClampMax = "200.0"))
	float TemperatureScaleHeightMeters = 0.5f;

	/** World Z of the road or sea surface, in Unreal centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics")
	float LayerBaseWorldZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "1.0", ClampMax = "500.0"))
	float LayerThicknessMeters = 18.0f;

	/** Half-width of the heated road or inversion volume, measured from world Y=0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "1.0", ClampMax = "5000.0"))
	float LayerLateralHalfWidthMeters = 6.0f;

	/** World-space X range of the refracting road/sea volume, in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics")
	float LayerStartWorldXMeters = -100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics")
	float LayerEndWorldXMeters = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "100.0", ClampMax = "20000.0"))
	float TraceDistanceMeters = 6500.0f;

	/** Launch-angle separation used to resolve neighboring physical image branches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Physics", meta = (ClampMin = "0.0", ClampMax = "0.02"))
	float BranchSeparationRadians = 0.0012f;

	/** 0=off, 1=shader proof, 2=triangle hits, 3=ray/object crossing, 4=exact passthrough. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirage|Diagnostics", meta = (ClampMin = "0", ClampMax = "4"))
	int32 DiagnosticMode = 0;

private:
	void PushSettings() const;
	void HandleRuntimeControls();
	void SetCameraHeight(float HeightCm);
	void CaptureVerificationFrame();
	void EnableMirageForPairedCapture();
	void CaptureEnabledVerificationFrame();
	void FinishVerificationRun();

	FString VerificationCapturePath;
	FString VerificationEnabledCapturePath;
	FTimerHandle CaptureTimerHandle;
	FTimerHandle EnableTimerHandle;
	FTimerHandle EnabledCaptureTimerHandle;
	FTimerHandle ExitTimerHandle;
	bool bShowRuntimeHelp = true;
	bool bPerformanceSampling = false;
	float PerformanceWarmupSeconds = 5.0f;
	double PerformanceElapsedSeconds = 0.0;
	double PerformanceGpuMilliseconds = 0.0;
	uint64 PerformanceFrameCount = 0;
};
