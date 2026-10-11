// Copyright Epic Games, Inc. All Rights Reserved.

#include "MirageController.h"
#include "MirageRenderSettings.h"
#include "HAL/FileManager.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "StaticMeshResources.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "InputCoreTypes.h"
#include "DynamicRHI.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogMirageController, Log, All);

AMirageController::AMirageController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AMirageController::BeginPlay()
{
	Super::BeginPlay();
	bPerformanceSampling = FParse::Param(FCommandLine::Get(), TEXT("MiragePerf"));
	if (bPerformanceSampling)
	{
		UE_LOG(LogMirageController, Display, TEXT("Performance sampling enabled (5 second warmup)"));
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("MirageDiagnostic")))
	{
		DiagnosticMode = 1;
		UE_LOG(LogMirageController, Display, TEXT("Temporary diagnostic coloring enabled by command line"));
	}
	int32 RequestedDiagnosticMode = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("MirageDiagnosticMode="), RequestedDiagnosticMode))
	{
		DiagnosticMode = FMath::Clamp(RequestedDiagnosticMode, 0, 4);
		UE_LOG(LogMirageController, Display, TEXT("Diagnostic mode %d enabled by command line"), DiagnosticMode);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("MirageDisable")))
	{
		bEnabled = false;
		UE_LOG(LogMirageController, Display, TEXT("Mirage renderer disabled by command line for baseline capture"));
	}
	const bool bPairedCapture = FParse::Param(FCommandLine::Get(), TEXT("MirageCapturePair"));
	if (bPairedCapture)
	{
		bEnabled = true;
		UE_LOG(LogMirageController, Display, TEXT("Paired ON/OFF capture requested; warming the scene with mirage enabled"));
	}
	PushSettings();
	float RequestedCameraHeightCm = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("MirageCameraHeightCm="), RequestedCameraHeightCm))
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			FVector Location = Pawn->GetActorLocation();
			Location.Z = LayerBaseWorldZ + RequestedCameraHeightCm;
			Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogMirageController, Display, TEXT("Applied camera-height test override: %.1f cm above refracting surface"), RequestedCameraHeightCm);
		}
	}
	float RequestedFOV = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("MirageFOV="), RequestedFOV))
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PlayerController->PlayerCameraManager)
			{
				PlayerController->PlayerCameraManager->SetFOV(FMath::Clamp(RequestedFOV, 5.0f, 120.0f));
				UE_LOG(LogMirageController, Display, TEXT("Applied verification camera FOV: %.2f degrees"), RequestedFOV);
			}
		}
	}
	UE_LOG(LogMirageController, Display,
		TEXT("MirageController active: %s type=%s P=%.3fkPa T0=%.2fK dT/dz=%.4fK/m scale=%.2fm layer=%.1fm trace=%.0fm"),
		bEnabled ? TEXT("enabled") : TEXT("disabled"),
		MirageType == EMirageType::Inferior ? TEXT("Inferior") : TEXT("Superior"),
		PressureKPa, BaseTemperatureK, TemperatureGradientKPerMeter, TemperatureScaleHeightMeters,
		LayerThicknessMeters, TraceDistanceMeters);

	if (FParse::Param(FCommandLine::Get(), TEXT("MirageCapture")))
	{
		FString CaptureName = GetWorld() ? GetWorld()->GetMapName() : TEXT("MirageCapture");
		FParse::Value(FCommandLine::Get(), TEXT("MirageCaptureName="), CaptureName);
		const FString CaptureDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), TEXT("MirageVerification"));
		IFileManager::Get().MakeDirectory(*CaptureDirectory, true);
		VerificationCapturePath = FPaths::Combine(CaptureDirectory,
			CaptureName + (bPairedCapture ? TEXT("_ON.png") : TEXT(".png")));
		VerificationEnabledCapturePath = FPaths::Combine(CaptureDirectory, CaptureName + TEXT("_OFF.png"));
		// Leave enough time for first-run PSO, mesh, and skylight compilation before evidence capture.
		GetWorldTimerManager().SetTimer(CaptureTimerHandle, this, &AMirageController::CaptureVerificationFrame,
			bPairedCapture ? 42.0f : 30.0f, false);
		if (bPairedCapture)
		{
			GetWorldTimerManager().SetTimer(EnableTimerHandle, this, &AMirageController::EnableMirageForPairedCapture, 45.0f, false);
			GetWorldTimerManager().SetTimer(EnabledCaptureTimerHandle, this,
				&AMirageController::CaptureEnabledVerificationFrame, 55.0f, false);
			GetWorldTimerManager().SetTimer(ExitTimerHandle, this, &AMirageController::FinishVerificationRun, 61.0f, false);
		}
		else
		{
			GetWorldTimerManager().SetTimer(ExitTimerHandle, this, &AMirageController::FinishVerificationRun, 37.0f, false);
		}
		UE_LOG(LogMirageController, Display, TEXT("Verification capture scheduled: %s"), *VerificationCapturePath);
	}
}

void AMirageController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FMirageRenderSettings Disabled;
	Disabled.bEnabled = false;
	FMirageRenderSettingsStore::Set(Disabled);
	Super::EndPlay(EndPlayReason);
}

void AMirageController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HandleRuntimeControls();
	if (bPerformanceSampling)
	{
		if (PerformanceWarmupSeconds > 0.0f)
		{
			PerformanceWarmupSeconds -= DeltaSeconds;
		}
		else
		{
			PerformanceElapsedSeconds += DeltaSeconds;
			PerformanceGpuMilliseconds += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
			++PerformanceFrameCount;
		}
	}
}

void AMirageController::SetCameraHeight(float HeightCm)
{
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		FVector Location = Pawn->GetActorLocation();
		Location.Z = LayerBaseWorldZ + HeightCm;
		Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogMirageController, Display, TEXT("Runtime camera height set to %.2f m"), HeightCm * 0.01f);
	}
}

void AMirageController::HandleRuntimeControls()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return;
	}
	bool bSettingsChanged = false;
	if (PlayerController->WasInputKeyJustPressed(EKeys::M))
	{
		bEnabled = !bEnabled;
		DiagnosticMode = 0;
		bSettingsChanged = true;
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::One)) { SetCameraHeight(50.0f); }
	if (PlayerController->WasInputKeyJustPressed(EKeys::Two)) { SetCameraHeight(150.0f); }
	if (PlayerController->WasInputKeyJustPressed(EKeys::Three)) { SetCameraHeight(300.0f); }
	if (PlayerController->WasInputKeyJustPressed(EKeys::LeftBracket))
	{
		TemperatureGradientKPerMeter -= MirageType == EMirageType::Inferior ? 5.0f : 0.2f;
		bSettingsChanged = true;
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::RightBracket))
	{
		TemperatureGradientKPerMeter += MirageType == EMirageType::Inferior ? 5.0f : 0.2f;
		bSettingsChanged = true;
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::Hyphen))
	{
		PressureKPa = FMath::Max(20.0f, PressureKPa - 1.0f);
		bSettingsChanged = true;
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::Equals))
	{
		PressureKPa = FMath::Min(120.0f, PressureKPa + 1.0f);
		bSettingsChanged = true;
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::H))
	{
		bShowRuntimeHelp = !bShowRuntimeHelp;
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::F5))
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("DesertRoad")));
		return;
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::F6))
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("ColdCoast")));
		return;
	}
	if (bSettingsChanged)
	{
		PushSettings();
	}
	if (bShowRuntimeHelp && GEngine)
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		const float CameraHeightM = Pawn ? (Pawn->GetActorLocation().Z - LayerBaseWorldZ) * 0.01f : 0.0f;
		const FString Status = FString::Printf(
			TEXT("STA-043  Mirage: %s  Height: %.2fm  dT/dz: %.2f K/m  P: %.2f kPa\nM toggle | 1/2/3 heights | [ ] gradient | - = pressure | F5 Desert | F6 Coast | H help"),
			bEnabled ? TEXT("ON") : TEXT("OFF"), CameraHeightM, TemperatureGradientKPerMeter, PressureKPa);
		GEngine->AddOnScreenDebugMessage(0x4D495241, 0.0f, FColor::Cyan, Status, false, FVector2D(1.05f, 1.05f));
	}
}

#if WITH_EDITOR
void AMirageController::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	PushSettings();
}
#endif

void AMirageController::PushSettings() const
{
	FMirageRenderSettings Settings;
	Settings.bEnabled = bEnabled;
	Settings.Type = MirageType;
	Settings.RefractionStrength = FMath::Clamp(RefractionStrength, 0.0f, 0.08f);
	Settings.Horizon = FMath::Clamp(Horizon, 0.0f, 1.0f);
	Settings.BandHeight = FMath::Clamp(BandHeight, 0.01f, 1.0f);
	Settings.TurbulenceSpeed = FMath::Max(TurbulenceSpeed, 0.0f);
	Settings.TurbulenceScale = FMath::Max(TurbulenceScale, 1.0f);
	Settings.ReflectionAmount = FMath::Clamp(ReflectionAmount, 0.0f, 1.0f);
	Settings.LoomingAmount = FMath::Clamp(LoomingAmount, 0.0f, 1.0f);
	Settings.PressureKPa = FMath::Clamp(PressureKPa, 20.0f, 120.0f);
	Settings.BaseTemperatureK = FMath::Clamp(BaseTemperatureK, 180.0f, 380.0f);
	Settings.TemperatureGradientKPerMeter = FMath::Clamp(TemperatureGradientKPerMeter, -200.0f, 200.0f);
	Settings.TemperatureScaleHeightMeters = FMath::Clamp(TemperatureScaleHeightMeters, 0.05f, 200.0f);
	Settings.LayerBaseWorldZ = LayerBaseWorldZ;
	Settings.LayerThicknessMeters = FMath::Clamp(LayerThicknessMeters, 1.0f, 500.0f);
	Settings.LayerLateralHalfWidthMeters = FMath::Clamp(LayerLateralHalfWidthMeters, 1.0f, 5000.0f);
	Settings.LayerStartWorldXMeters = FMath::Min(LayerStartWorldXMeters, LayerEndWorldXMeters);
	Settings.LayerEndWorldXMeters = FMath::Max(LayerStartWorldXMeters, LayerEndWorldXMeters);
	Settings.TraceDistanceMeters = FMath::Clamp(TraceDistanceMeters, 100.0f, 20000.0f);
	Settings.BranchSeparationRadians = FMath::Clamp(BranchSeparationRadians, 0.0f, 0.02f);
	Settings.DiagnosticMode = FMath::Clamp(DiagnosticMode, 0, 4);

	// Build a compact, explicit world-space representation for the demonstration geometry.
	// These compound AABBs are traced by every curved ray and remain resolvable when their
	// rasterized surface is hidden or outside the original camera frustum.
	auto AddProxy = [&Settings](const FVector& Min, const FVector& Max, const FLinearColor& Color,
		int32 TriangleStart = -1, int32 TriangleCount = 0)
	{
		if (Settings.GeometryMin.Num() >= MaxMirageGeometryProxies)
		{
			return;
		}
		Settings.GeometryMin.Add(FVector4f(FVector3f(Min), 0.0f));
		Settings.GeometryMax.Add(FVector4f(FVector3f(Max), 0.0f));
		Settings.GeometryColor.Add(FVector4f(Color.R, Color.G, Color.B, 1.0f));
		Settings.GeometryTriangleRange.Add(FVector4f(
			static_cast<float>(TriangleStart), static_cast<float>(TriangleCount), TriangleCount > 0 ? 1.0f : 0.0f, 0.0f));
	};

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const AStaticMeshActor* Actor = *It;
			if (!Actor->ActorHasTag(TEXT("MirageTrace")))
			{
				continue;
			}

			FVector Origin;
			FVector Extent;
			Actor->GetActorBounds(false, Origin, Extent);
			const FVector Min = Origin - Extent;
			const FVector Max = Origin + Extent;
			if (Actor->ActorHasTag(TEXT("MirageTriangleTrace")))
			{
				const UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
				const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
				const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
				if (RenderData && !RenderData->LODResources.IsEmpty())
				{
					const int32 LODIndex = Actor->ActorHasTag(TEXT("MirageCliff"))
						? RenderData->LODResources.Num() - 1 : 0;
					const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
					const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
					const FStaticMeshVertexBuffer& VertexAttributes = LOD.VertexBuffers.StaticMeshVertexBuffer;
					const FRawStaticIndexBuffer& Indices = LOD.IndexBuffer;
					if (!Indices.GetAllowCPUAccess())
					{
						UE_LOG(LogMirageController, Error,
							TEXT("Mirage trace mesh %s has no cooked CPU index data; enable Allow CPU Access on the project asset"),
							*Actor->GetName());
						continue;
					}
					const FMatrix NormalMatrix = Component->GetComponentTransform().ToInverseMatrixWithScale().GetTransposed();
					const int32 TriangleStart = Settings.GeometryTriangleVertices.Num() / 3;
					const int32 AvailableTriangles = static_cast<int32>(Indices.GetNumIndices() / 3);
					const int32 TriangleCount = FMath::Min(AvailableTriangles, 4096 - TriangleStart);
					for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
					{
						for (int32 Corner = 0; Corner < 3; ++Corner)
						{
							const uint32 VertexIndex = Indices.GetIndex(TriangleIndex * 3 + Corner);
							const FVector LocalPosition(Positions.VertexPosition(VertexIndex));
							const FVector WorldPosition = Component->GetComponentTransform().TransformPosition(LocalPosition);
							Settings.GeometryTriangleVertices.Add(FVector4f(FVector3f(WorldPosition), 1.0f));
							const FVector LocalNormal(VertexAttributes.VertexTangentZ(VertexIndex));
							const FVector WorldNormal = NormalMatrix.TransformVector(LocalNormal).GetSafeNormal();
							Settings.GeometryTriangleNormals.Add(FVector4f(FVector3f(WorldNormal), 0.0f));
							const FVector2f UV = VertexAttributes.GetNumTexCoords() > 0
								? VertexAttributes.GetVertexUV(VertexIndex, 0) : FVector2f::ZeroVector;
							Settings.GeometryTriangleUVs.Add(FVector4f(UV.X, UV.Y, 0.0f, 0.0f));
						}
					}
					FLinearColor Color(0.025f, 0.12f, 0.82f);
					if (Actor->ActorHasTag(TEXT("MirageMarker")))
					{
						Color = FLinearColor(1.0f, 0.035f, 0.01f);
					}
					else if (Actor->ActorHasTag(TEXT("MirageShip")))
					{
						Color = FLinearColor(0.045f, 0.085f, 0.14f);
					}
					else if (Actor->ActorHasTag(TEXT("MirageCliff")))
					{
						Color = FLinearColor(0.42f, 0.31f, 0.19f);
					}
					AddProxy(Min, Max, Color, TriangleStart, TriangleCount);
					static TSet<FName> LoggedTriangleActors;
					if (!LoggedTriangleActors.Contains(Actor->GetFName()))
					{
						LoggedTriangleActors.Add(Actor->GetFName());
						UE_LOG(LogMirageController, Display,
							TEXT("Actual-mesh mirage trace: %s triangles=%d/%d boundsMin=%s boundsMax=%s"),
							*Actor->GetName(), TriangleCount, AvailableTriangles, *Min.ToString(), *Max.ToString());
					}
				}
			}
			else if (Actor->ActorHasTag(TEXT("MirageVehicle")))
			{
				const FLinearColor Color = Actor->ActorHasTag(TEXT("MirageTruck"))
					? FLinearColor(0.72f, 0.12f, 0.035f) : FLinearColor(0.035f, 0.22f, 0.78f);
				const double BodyTop = FMath::Lerp(Min.Z, Max.Z, 0.58);
				AddProxy(Min, FVector(Max.X, Max.Y, BodyTop), Color);
				AddProxy(FVector(Origin.X - Extent.X * 0.35, Min.Y, BodyTop), Max, Color * 0.82f);
			}
			else if (Actor->ActorHasTag(TEXT("MirageShip")))
			{
				const FLinearColor Color(0.055f, 0.09f, 0.14f);
				const double HullTop = FMath::Lerp(Min.Z, Max.Z, 0.34);
				AddProxy(Min, FVector(Max.X, Max.Y, HullTop), Color);
				AddProxy(FVector(Origin.X - Extent.X * 0.28, Origin.Y - Extent.Y * 0.45, HullTop),
					FVector(Origin.X + Extent.X * 0.25, Origin.Y + Extent.Y * 0.45, FMath::Lerp(Min.Z, Max.Z, 0.76)),
					FLinearColor(0.72f, 0.75f, 0.78f));
				AddProxy(FVector(Origin.X - Extent.X * 0.04, Origin.Y - Extent.Y * 0.06, HullTop),
					FVector(Origin.X + Extent.X * 0.04, Origin.Y + Extent.Y * 0.06, Max.Z),
					FLinearColor(0.15f, 0.17f, 0.19f));
			}
			else if (Actor->ActorHasTag(TEXT("MirageCliff")))
			{
				const FLinearColor Color(0.42f, 0.34f, 0.25f);
				const double Third = (Max.Y - Min.Y) / 3.0;
				AddProxy(Min, FVector(Max.X, Min.Y + Third, FMath::Lerp(Min.Z, Max.Z, 0.72)), Color * 0.82f);
				AddProxy(FVector(Min.X, Min.Y + Third, Min.Z),
					FVector(Max.X, Min.Y + 2.0 * Third, Max.Z), Color);
				AddProxy(FVector(Min.X, Min.Y + 2.0 * Third, Min.Z),
					FVector(Max.X, Max.Y, FMath::Lerp(Min.Z, Max.Z, 0.81)), Color * 0.9f);
			}
			else
			{
				AddProxy(Min, Max, FLinearColor(0.8f, 0.18f, 0.04f));
			}
		}
	}
	FMirageRenderSettingsStore::Set(Settings);
}

void AMirageController::CaptureVerificationFrame()
{
	FScreenshotRequest::RequestScreenshot(VerificationCapturePath, false, false);
	UE_LOG(LogMirageController, Display, TEXT("Requested rendered verification screenshot: %s"), *VerificationCapturePath);
}

void AMirageController::EnableMirageForPairedCapture()
{
	// Keep the Tonemap callback and its warmed render target active.  Mode 4 is
	// an exact SceneColor passthrough, yielding a trustworthy pixel-aligned OFF
	// frame without the black first-frame problem seen when the pass is removed.
	DiagnosticMode = 4;
	PushSettings();
	UE_LOG(LogMirageController, Display, TEXT("Paired capture: shader switched to SceneColor passthrough after ON frame"));
}

void AMirageController::CaptureEnabledVerificationFrame()
{
	FScreenshotRequest::RequestScreenshot(VerificationEnabledCapturePath, false, false);
	UE_LOG(LogMirageController, Display, TEXT("Requested disabled verification screenshot: %s"),
		*VerificationEnabledCapturePath);
}

void AMirageController::FinishVerificationRun()
{
	if (bPerformanceSampling && PerformanceFrameCount > 0 && PerformanceElapsedSeconds > 0.0)
	{
		const double AverageFps = static_cast<double>(PerformanceFrameCount) / PerformanceElapsedSeconds;
		const double AverageFrameMs = PerformanceElapsedSeconds * 1000.0 / static_cast<double>(PerformanceFrameCount);
		const double AverageGpuMs = PerformanceGpuMilliseconds / static_cast<double>(PerformanceFrameCount);
		UE_LOG(LogMirageController, Display,
			TEXT("MIRAGE_PERF frames=%llu elapsed=%.3fs avg_fps=%.2f avg_frame_ms=%.3f avg_gpu_ms=%.3f"),
			PerformanceFrameCount, PerformanceElapsedSeconds, AverageFps, AverageFrameMs, AverageGpuMs);
	}
	UE_LOG(LogMirageController, Display, TEXT("Verification run complete; requesting clean exit"));
	FGenericPlatformMisc::RequestExit(false);
}
