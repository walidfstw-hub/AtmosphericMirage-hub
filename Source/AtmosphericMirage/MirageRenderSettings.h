// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MirageController.h"

static constexpr int32 MaxMirageGeometryProxies = 24;

struct FMirageRenderSettings
{
	bool bEnabled = false;
	EMirageType Type = EMirageType::Inferior;
	float RefractionStrength = 0.012f;
	float Horizon = 0.56f;
	float BandHeight = 0.34f;
	float TurbulenceSpeed = 1.3f;
	float TurbulenceScale = 32.0f;
	float ReflectionAmount = 0.48f;
	float LoomingAmount = 0.32f;
	float PressureKPa = 101.325f;
	float BaseTemperatureK = 318.0f;
	float TemperatureGradientKPerMeter = -0.8f;
	float TemperatureScaleHeightMeters = 0.5f;
	float LayerBaseWorldZ = 0.0f;
	float LayerThicknessMeters = 18.0f;
	float LayerLateralHalfWidthMeters = 6.0f;
	float LayerStartWorldXMeters = -100.0f;
	float LayerEndWorldXMeters = 8000.0f;
	float TraceDistanceMeters = 6500.0f;
	float BranchSeparationRadians = 0.0012f;
	int32 DiagnosticMode = 0;
	TArray<FVector4f> GeometryMin;
	TArray<FVector4f> GeometryMax;
	TArray<FVector4f> GeometryColor;
	TArray<FVector4f> GeometryTriangleRange;
	TArray<FVector4f> GeometryTriangleVertices;
	TArray<FVector4f> GeometryTriangleNormals;
	TArray<FVector4f> GeometryTriangleUVs;
};

class FMirageRenderSettingsStore
{
public:
	static void Set(const FMirageRenderSettings& InSettings);
	static FMirageRenderSettings Get();
};
