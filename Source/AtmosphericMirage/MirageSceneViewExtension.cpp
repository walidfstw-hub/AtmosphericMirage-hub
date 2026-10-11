// Copyright Epic Games, Inc. All Rights Reserved.

#include "MirageSceneViewExtension.h"
#include "MirageRenderSettings.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneTexturesConfig.h"
#include "ScreenPass.h"
#include "ShaderParameterStruct.h"

DEFINE_LOG_CATEGORY_STATIC(LogMirageRenderer, Log, All);

class FMirageRefractionPS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMirageRefractionPS);
	SHADER_USE_PARAMETER_STRUCT(FMirageRefractionPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FVector4f, MirageParameters0)
		SHADER_PARAMETER(FVector4f, MirageParameters1)
		SHADER_PARAMETER(FVector4f, AtmosphereParameters0)
		SHADER_PARAMETER(FVector4f, AtmosphereParameters1)
		SHADER_PARAMETER(FVector4f, AtmosphereParameters2)
		SHADER_PARAMETER(uint32, MirageGeometryCount)
		SHADER_PARAMETER_ARRAY(FVector4f, MirageGeometryMin, [MaxMirageGeometryProxies])
		SHADER_PARAMETER_ARRAY(FVector4f, MirageGeometryMax, [MaxMirageGeometryProxies])
		SHADER_PARAMETER_ARRAY(FVector4f, MirageGeometryColor, [MaxMirageGeometryProxies])
		SHADER_PARAMETER_ARRAY(FVector4f, MirageGeometryTriangleRange, [MaxMirageGeometryProxies])
		SHADER_PARAMETER(uint32, MirageTriangleVertexCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, MirageTriangleVertices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, MirageTriangleNormals)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, MirageTriangleUVs)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMirageRefractionPS, "/AtmosphericMirage/MirageRefraction.usf", "MainPS", SF_Pixel);

FMirageSceneViewExtension::FMirageSceneViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	UE_LOG(LogMirageRenderer, Display, TEXT("Mirage scene-view extension constructed"));
}

void FMirageSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::Tonemap && bIsPassEnabled)
	{
		static bool bLoggedSubscription = false;
		if (!bLoggedSubscription)
		{
			bLoggedSubscription = true;
			UE_LOG(LogMirageRenderer, Display, TEXT("Subscribed to Tonemap for an active view"));
		}
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FMirageSceneViewExtension::RenderMirage_RenderThread));
	}
}

FScreenPassTexture FMirageSceneViewExtension::RenderMirage_RenderThread(FRDGBuilder& GraphBuilder,
	const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	const FMirageRenderSettings Settings = FMirageRenderSettingsStore::Get();
	const FScreenPassTextureSlice SceneColorSlice = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
	if (!Settings.bEnabled || !SceneColorSlice.IsValid())
	{
		return FScreenPassTexture(SceneColorSlice);
	}

	const FScreenPassTexture SceneColor(SceneColorSlice);
	static bool bLoggedDispatch = false;
	if (!bLoggedDispatch)
	{
		bLoggedDispatch = true;
		UE_LOG(LogMirageRenderer, Display,
			TEXT("Dispatching MirageRefractionPS: type=%s strength=%.5f P=%.3f T0=%.2f gradient=%.4f geometry=%d diagnostic=%d"),
			Settings.Type == EMirageType::Inferior ? TEXT("Inferior") : TEXT("Superior"),
			Settings.RefractionStrength, Settings.PressureKPa, Settings.BaseTemperatureK,
			Settings.TemperatureGradientKPerMeter, Settings.GeometryMin.Num(), Settings.DiagnosticMode);
		UE_LOG(LogMirageRenderer, Display, TEXT("Mirage render view location: %s"), *View.ViewLocation.ToString());
	}
	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(
			GraphBuilder, SceneColor, View.GetOverwriteLoadAction(), TEXT("MirageRefractionOutput"));
	}

	FMirageRefractionPS::FParameters* Parameters = GraphBuilder.AllocParameters<FMirageRefractionPS::FParameters>();
	Parameters->ViewUniformBuffer = View.ViewUniformBuffer;
	Parameters->SceneTexturesStruct = Inputs.SceneTextures.SceneTextures;
	Parameters->SceneColorTexture = SceneColor.Texture;
	Parameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->MirageParameters0 = FVector4f(
		Settings.RefractionStrength, Settings.Horizon, Settings.BandHeight,
		Settings.Type == EMirageType::Superior ? 1.0f : 0.0f);
	Parameters->MirageParameters1 = FVector4f(
		Settings.TurbulenceSpeed, Settings.TurbulenceScale,
		Settings.ReflectionAmount, Settings.LoomingAmount);
	Parameters->AtmosphereParameters0 = FVector4f(
		Settings.PressureKPa, Settings.BaseTemperatureK,
		Settings.TemperatureGradientKPerMeter, Settings.LayerBaseWorldZ);
	Parameters->AtmosphereParameters1 = FVector4f(
		Settings.LayerThicknessMeters, Settings.TraceDistanceMeters,
		Settings.BranchSeparationRadians, static_cast<float>(Settings.DiagnosticMode));
	Parameters->AtmosphereParameters2 = FVector4f(
		Settings.TemperatureScaleHeightMeters, Settings.LayerLateralHalfWidthMeters,
		Settings.LayerStartWorldXMeters, Settings.LayerEndWorldXMeters);
	Parameters->MirageGeometryCount = static_cast<uint32>(FMath::Min(
		FMath::Min(Settings.GeometryMin.Num(), Settings.GeometryMax.Num()),
		FMath::Min(FMath::Min(Settings.GeometryColor.Num(), Settings.GeometryTriangleRange.Num()), MaxMirageGeometryProxies)));
	for (int32 Index = 0; Index < MaxMirageGeometryProxies; ++Index)
	{
		const bool bValid = Index < static_cast<int32>(Parameters->MirageGeometryCount);
		Parameters->MirageGeometryMin[Index] = bValid ? Settings.GeometryMin[Index] : FVector4f(0, 0, 0, 0);
		Parameters->MirageGeometryMax[Index] = bValid ? Settings.GeometryMax[Index] : FVector4f(0, 0, 0, 0);
		Parameters->MirageGeometryColor[Index] = bValid ? Settings.GeometryColor[Index] : FVector4f(0, 0, 0, 0);
		Parameters->MirageGeometryTriangleRange[Index] = bValid ? Settings.GeometryTriangleRange[Index] : FVector4f(-1, 0, 0, 0);
	}
	FRDGBufferRef TriangleBuffer = CreateStructuredBuffer(
		GraphBuilder, TEXT("MirageTriangleVertices"), Settings.GeometryTriangleVertices);
	FRDGBufferRef TriangleNormalBuffer = CreateStructuredBuffer(
		GraphBuilder, TEXT("MirageTriangleNormals"), Settings.GeometryTriangleNormals);
	FRDGBufferRef TriangleUVBuffer = CreateStructuredBuffer(
		GraphBuilder, TEXT("MirageTriangleUVs"), Settings.GeometryTriangleUVs);
	Parameters->MirageTriangleVertices = GraphBuilder.CreateSRV(TriangleBuffer);
	Parameters->MirageTriangleNormals = GraphBuilder.CreateSRV(TriangleNormalBuffer);
	Parameters->MirageTriangleUVs = GraphBuilder.CreateSRV(TriangleUVBuffer);
	Parameters->MirageTriangleVertexCount = static_cast<uint32>(Settings.GeometryTriangleVertices.Num());
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	TShaderMapRef<FMirageRefractionPS> PixelShader(GetGlobalShaderMap(View.FeatureLevel));
	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("AtmosphericMirage::Refraction"),
		FScreenPassViewInfo(View),
		FScreenPassTextureViewport(Output),
		FScreenPassTextureViewport(SceneColor),
		PixelShader,
		Parameters);

	return MoveTemp(Output);
}
