/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <bit>

#include "DynWater.h"
#include "WaterRendering.h"

#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/GameHelper.h"
#include "Game/GlobalUnsynced.h"
// #include "Game/UI/MouseHandler.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/GL/VertexArray.h"
#include "Rendering/Textures/Bitmap.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "System/Log/ILog.h"
#include "System/Exceptions.h"

#include "System/Misc/TracyDefs.h"

#define LOG_SECTION_DYN_WATER "DynWater"
LOG_REGISTER_SECTION_GLOBAL(LOG_SECTION_DYN_WATER)

// use the specific section for all LOG*() calls in this source file
#ifdef LOG_SECTION_CURRENT
	#undef LOG_SECTION_CURRENT
#endif
#define LOG_SECTION_CURRENT LOG_SECTION_DYN_WATER


#define W_SIZE 5
#define WF_SIZE 5120
#define WH_SIZE 2560
/*
#define W_SIZE 4
#define WF_SIZE 4096
#define WH_SIZE 2048
*/
void CDynWater::InitResources(bool loadShader)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!FBO::IsSupported())
		throw content_error("DynWater Error: missing FBO support");

	camPosBig = float3(2048, 0, 2048);
	refractSize = (globalRendering->viewSizeY >= 1024) ? 1024 : 512;

	glGenTextures(1, &reflectTexture);
	glBindTexture(GL_TEXTURE_2D, reflectTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glGenTextures(1, &refractTexture);
	glBindTexture(GL_TEXTURE_2D, refractTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, refractSize, refractSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glGenTextures(1, &detailNormalTex);
	glBindTexture(GL_TEXTURE_2D, detailNormalTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, 256, 256, 0, GL_RGBA, GL_FLOAT, 0);
	glGenerateMipmapEXT(GL_TEXTURE_2D);

	float* temp = new float[1024 * 1024 * 4];

	for (int y = 0; y < 64; ++y) {
		for (int x = 0; x < 64; ++x) {
			temp[(y*64 + x)*4 + 0] = std::sin(x*math::TWOPI/64.0f) + ((x < 32) ? -1 : 1)*0.3f;
			temp[(y*64 + x)*4 + 1] = temp[(y*64 + x)*4 + 0];
			temp[(y*64 + x)*4 + 2] = std::cos(x*math::TWOPI/64.0f) + ((x < 32) ? (16 - x) : (x - 48))/16.0f*0.3f;
			temp[(y*64 + x)*4 + 3] = 0;
		}
	}
	glGenTextures(3, rawBumpTexture);
	glBindTexture(GL_TEXTURE_2D, rawBumpTexture[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, 64, 64, 0, GL_RGBA, GL_FLOAT, temp);


	CBitmap foam;
	// NB: assumes the image is not dxt-compressed but IL will still load it (!)
	if (!foam.LoadGrayscale(waterRendering->foamTexture))
		LOG_L(L_WARNING, "[%s] could not load grayscale foam-texture %s\n", __func__, waterRendering->foamTexture.c_str());

	if (!std::has_single_bit <uint32_t> (foam.xsize) || !std::has_single_bit <uint32_t> (foam.ysize))
		foam.CreateRescaled(std::bit_ceil <uint32_t> (foam.xsize), std::bit_ceil <uint32_t> (foam.ysize));

	foamTex = foam.CreateMipMapTexture(0.0f, 0.0f, 0);


	if (ProgramStringIsNative(GL_VERTEX_PROGRAM_ARB, "ARB/waterDyn.vp")) {
		waterVP = LoadVertexProgram("ARB/waterDyn.vp");
	} else {
		waterVP = LoadVertexProgram("ARB/waterDynNT.vp");
	}

	waterFP          = LoadFragmentProgram("ARB/waterDyn.fp");
	waveFP           = LoadFragmentProgram("ARB/waterDynWave.fp");
	waveVP           = LoadVertexProgram(  "ARB/waterDynWave.vp");
	waveFP2          = LoadFragmentProgram("ARB/waterDynWave2.fp");
	waveVP2          = LoadVertexProgram(  "ARB/waterDynWave2.vp");
	waveNormalFP     = LoadFragmentProgram("ARB/waterDynNormal.fp");
	waveNormalVP     = LoadVertexProgram(  "ARB/waterDynNormal.vp");
	waveCopyHeightFP = LoadFragmentProgram("ARB/waterDynWave3.fp");
	waveCopyHeightVP = LoadVertexProgram(  "ARB/waterDynWave3.vp");
	dwDetailNormalFP = LoadFragmentProgram("ARB/dwDetailNormal.fp");
	dwDetailNormalVP = LoadVertexProgram(  "ARB/dwDetailNormal.vp");
	dwAddSplashFP    = LoadFragmentProgram("ARB/dwAddSplash.fp");
	dwAddSplashVP    = LoadVertexProgram(  "ARB/dwAddSplash.vp");

	waterSurfaceColor = waterRendering->surfaceColor;

	for (int y = 0; y < 1024; ++y) {
		for (int x = 0; x < 1024; ++x) {
			temp[(y*1024 + x)*4 + 0] = 0;
			temp[(y*1024 + x)*4 + 1] = 0;
			temp[(y*1024 + x)*4 + 2] = 0;
			temp[(y*1024 + x)*4 + 3] = 0;
		}
	}
	glGenTextures(1, &waveHeight32);
	glBindTexture(GL_TEXTURE_2D, waveHeight32);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, 256, 256, 0, GL_RGBA, GL_FLOAT, temp);

	glGenTextures(1, &waveTex1);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, 1024, 1024, 0, GL_RGBA, GL_FLOAT, temp);

	for (int y = 0; y < 1024; ++y) {
		for (int x = 0; x < 1024; ++x) {
			//const float dist = (x - 500)*(x - 500)+(y - 450)*(y - 450);
			temp[(y*1024 + x)*4 + 0] = 0;//max(0.0f,15-std::sqrt(dist));//std::sin(y*PI*2.0f/64.0f)*0.5f+0.5f;
			temp[(y*1024 + x)*4 + 1] = 0;
			temp[(y*1024 + x)*4 + 2] = 0;
			temp[(y*1024 + x)*4 + 3] = 0;
		}
	}
	glGenTextures(1, &waveTex2);
	glBindTexture(GL_TEXTURE_2D, waveTex2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, 1024, 1024, 0, GL_RGBA, GL_FLOAT, temp);

	for (int y = 0; y < 1024; ++y) {
		for (int x = 0; x < 1024; ++x) {
			temp[(y*1024 + x)*4 + 0] = 0;
			temp[(y*1024 + x)*4 + 1] = 1;
			temp[(y*1024 + x)*4 + 2] = 0;
			temp[(y*1024 + x)*4 + 3] = 0;
		}
	}
	glGenTextures(1, &waveTex3);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA16F_ARB, 1024, 1024, 0, GL_RGBA, GL_FLOAT, temp);

	for (int y = 0; y < 64; ++y) {
		const float dy = y - 31.5f;
		for (int x = 0; x < 64; ++x) {
			const float dx = x-31.5f;
			const float dist = std::sqrt(dx*dx + dy*dy);
			temp[(y*64 + x)*4 + 0] = std::max(0.0f, 1 - dist/30.f) * std::max(0.0f, 1 - dist/30.f);
			temp[(y*64 + x)*4 + 1] = std::max(0.0f, 1 - dist/30.f);
			temp[(y*64 + x)*4 + 2] = std::max(0.0f, 1 - dist/30.f) * std::max(0.0f, 1 - dist/30.f);
			temp[(y*64 + x)*4 + 3] = 0;
		}
	}

	glGenTextures(1, &splashTex);
	glBindTexture(GL_TEXTURE_2D, splashTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR/*_MIPMAP_NEAREST*/);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
	//gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA32F_ARB, 64, 64, GL_RGBA, GL_FLOAT, temp);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, 64, 64, 0, GL_RGBA, GL_FLOAT, temp);

	unsigned char temp2[] = {0, 0, 0, 0};
	glGenTextures(1, &zeroTex);
	glBindTexture(GL_TEXTURE_2D, zeroTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR/*_MIPMAP_NEAREST*/);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp2);

	unsigned char temp3[] = {0, 255, 0, 0};
	glGenTextures(1, &fixedUpTex);
	glBindTexture(GL_TEXTURE_2D, fixedUpTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR/*_MIPMAP_NEAREST*/);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp3);

	CBitmap bm;
	if (!bm.Load("bitmaps/boatshape.bmp"))
		throw content_error("Could not load boatshape from file bitmaps/boatshape.bmp");

	for (int a = 0; a < (bm.xsize * bm.ysize); ++a) {
		bm.GetRawMem()[a * 4 + 2] = bm.GetRawMem()[a * 4];
		bm.GetRawMem()[a * 4 + 3] = bm.GetRawMem()[a * 4];
	}
	boatShape = bm.CreateTexture();

	CBitmap bm2;
	if (!bm.Load("bitmaps/hovershape.bmp"))
		throw content_error("Could not load hovershape from file bitmaps/hovershape.bmp");

	for (int a = 0; a < (bm2.xsize * bm2.ysize); ++a) {
		bm2.GetRawMem()[a * 4 + 2] = bm2.GetRawMem()[a * 4];
		bm2.GetRawMem()[a * 4 + 3] = bm2.GetRawMem()[a * 4];
	}
	hoverShape = bm2.CreateTexture();

	delete[] temp;
	temp = nullptr;

	glGenFramebuffersEXT(1, &frameBuffer);

	GLenum depthFormat = static_cast<GLenum>(CGlobalRendering::DepthBitsToFormat(globalRendering->supportDepthBufferBitDepth));

	reflectFBO.Bind();
	reflectFBO.AttachTexture(reflectTexture, GL_TEXTURE_2D, GL_COLOR_ATTACHMENT0_EXT);
	reflectFBO.CreateRenderBuffer(GL_DEPTH_ATTACHMENT_EXT, depthFormat, 512, 512);
	refractFBO.Bind();
	refractFBO.AttachTexture(refractTexture, GL_TEXTURE_2D, GL_COLOR_ATTACHMENT0_EXT);
	refractFBO.CreateRenderBuffer(GL_DEPTH_ATTACHMENT_EXT, depthFormat, refractSize, refractSize);
	FBO::Unbind();

	if (!reflectFBO.IsValid() || !refractFBO.IsValid()) {
		throw content_error("DynWater Error: Invalid FBO");
	}
}

void CDynWater::FreeResources()
{
	RECOIL_DETAILED_TRACY_ZONE;
	const auto DeleteTexture = [](GLuint& texID) { if (texID > 0) { glDeleteTextures(1, &texID); texID = 0; } };
	const auto DeleteProgram = [](GLuint& proID) { if (proID > 0) { glSafeDeleteProgram(proID); proID = 0; } };

	DeleteTexture(reflectTexture);
	DeleteTexture(refractTexture);
	for (auto& rbt : rawBumpTexture)
		DeleteTexture(rbt);

	DeleteTexture(detailNormalTex);
	DeleteTexture(waveTex1);
	DeleteTexture(waveTex2);
	DeleteTexture(waveTex3);
	DeleteTexture(waveHeight32);
	DeleteTexture(splashTex);
	DeleteTexture(foamTex);
	DeleteTexture(boatShape);
	DeleteTexture(hoverShape);
	DeleteTexture(zeroTex);
	DeleteTexture(fixedUpTex);

	DeleteProgram(waterFP);
	DeleteProgram(waterVP);
	DeleteProgram(waveFP);
	DeleteProgram(waveVP);
	DeleteProgram(waveFP2);
	DeleteProgram(waveVP2);
	DeleteProgram(waveNormalFP);
	DeleteProgram(waveNormalVP);
	DeleteProgram(waveCopyHeightFP);
	DeleteProgram(waveCopyHeightVP);
	DeleteProgram(dwDetailNormalVP);
	DeleteProgram(dwDetailNormalFP);
	DeleteProgram(dwAddSplashVP);
	DeleteProgram(dwAddSplashFP);

	if (frameBuffer) {
		glDeleteFramebuffersEXT(1, &frameBuffer);
		frameBuffer = 0;
	}
}

void CDynWater::Draw()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!waterRendering->forceRendering && !readMap->HasVisibleWater())
		return;

	glPushAttrib(GL_ENABLE_BIT);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_FOG);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_2D, reflectTexture);
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, waveHeight32);
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, refractTexture);
	glActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_2D, readMap->GetShadingTexture());
	glActiveTextureARB(GL_TEXTURE5_ARB);
	glBindTexture(GL_TEXTURE_2D, foamTex);
	glActiveTextureARB(GL_TEXTURE6_ARB);
	glBindTexture(GL_TEXTURE_2D, detailNormalTex);

	shadowHandler.SetupShadowTexSampler(GL_TEXTURE7);
	glActiveTextureARB(GL_TEXTURE0_ARB);

	glColor4f(1, 1, 1, 0.5f);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, waterFP);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, waterVP);
	glEnable(GL_VERTEX_PROGRAM_ARB);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE * wireFrameMode + GL_FILL * (1 - wireFrameMode));

	const float dx = float(globalRendering->viewSizeX) / globalRendering->viewSizeY * camera->GetTanHalfFov();
	const float dy = float(globalRendering->viewSizeY) / globalRendering->viewSizeY * camera->GetTanHalfFov();
	const auto& sky = ISky::GetSky();
	const float3& L = sky->GetLight()->GetLightDir();


	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 10, 1.0f/(W_SIZE*256), 1.0f/(W_SIZE*256), 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 11, -camPosX/256.0f + 0.5f, -camPosZ/256.0f + 0.5f, 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 12, 1.0f/WF_SIZE, 1.0f/WF_SIZE, 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 13, -(camPosBig.x - WH_SIZE)/WF_SIZE, -(camPosBig.z - WH_SIZE)/WF_SIZE, 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 14, 1.0f/(mapDims.mapxp1 * SQUARE_SIZE), 1.0f/(mapDims.mapyp1 * SQUARE_SIZE), 0, 0);
	//glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 1.0f/4096.0f, 1.0f/4096.0f, 0, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, camera->GetPos().x, camera->GetPos().y, camera->GetPos().z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 2, reflectRight.x, reflectRight.y, reflectRight.z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 3, reflectUp.x, reflectUp.y, reflectUp.z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 4, 0.5f/dx, 0.5f/dy, 1, 1);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 5, reflectForward.x, reflectForward.y, reflectForward.z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 6, 0.05f, 1 - 0.05f, 0, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 7, 0.2f, 0, 0, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 8, 0.5f, 0.6f, 0.8f, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 9, L.x, L.y, L.z, 0.0f);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 10, sunLighting->groundDiffuseColor.x, sunLighting->groundDiffuseColor.y, sunLighting->groundDiffuseColor.z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 11, sunLighting->groundAmbientColor.x, sunLighting->groundAmbientColor.y, sunLighting->groundAmbientColor.z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 12, refractRight.x, refractRight.y, refractRight.z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 13, refractUp.x,refractUp.y, refractUp.z, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 14, 0.5f/dx, 0.5f/dy, 1, 1);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 15, refractForward.x, refractForward.y, refractForward.z, 0);


	DrawWaterSurface();

	glBindTexture(GL_TEXTURE_2D, fixedUpTex);

	DrawOuterSurface();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	glDisable(GL_VERTEX_PROGRAM_ARB);
	glPopAttrib();
/*
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE5_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE6_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE7_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	*/
}

void CDynWater::UpdateWater(const CGame* game)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!waterRendering->forceRendering && !readMap->HasVisibleWater())
		return;

	glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_FOG_BIT);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	DrawHeightTex();

	if (firstDraw) {
		firstDraw = false;
		DrawDetailNormalTex();
	}
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);

	DrawRefraction(game);
	DrawReflection(game);
	FBO::Unbind();
	glPopAttrib();
}

void CDynWater::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!waterRendering->forceRendering && !readMap->HasVisibleWater())
		return;

	oldCamPosBig = camPosBig;

	camPosBig.x = std::floor(std::max((float)WH_SIZE, std::min((float)mapDims.mapx*SQUARE_SIZE-WH_SIZE, (float)camera->GetPos().x))/(W_SIZE*16))*(W_SIZE*16);
	camPosBig.z = std::floor(std::max((float)WH_SIZE, std::min((float)mapDims.mapy*SQUARE_SIZE-WH_SIZE, (float)camera->GetPos().z))/(W_SIZE*16))*(W_SIZE*16);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	/* if(mouse->buttons[0].pressed) {
		float3 pos = camera->GetPos() + mouse->dir * (-camera->GetPos().y / mouse->dir.y);
		AddSplash(pos, 20, 1);
	}*/
	AddShipWakes();
	AddExplosions();
	DrawDetailNormalTex();
	DrawWaves();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
}

void CDynWater::DrawReflection(const CGame* game)
{
	RECOIL_DETAILED_TRACY_ZONE;
	reflectFBO.Bind();

	const auto& sky = ISky::GetSky();
	glClearColor(sky->fogColor.x, sky->fogColor.y, sky->fogColor.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	const double clipPlaneEqs[2 * 4] = {
		0.0, 1.0, 0.0, 5.0, // ground; use d>0 to hide shoreline cracks
		0.0, 1.0, 0.0, 0.0, // models
	};

	CCamera* prvCam = CCameraHandler::GetSetActiveCamera(CCamera::CAMTYPE_UWREFL);
	CCamera* curCam = CCameraHandler::GetActiveCamera();

	{
		curCam->CopyStateReflect(prvCam);
		curCam->UpdateLoadViewport(0, 0, 512, 512);

		reflectRight   = curCam->GetRight();
		reflectUp      = curCam->GetUp();
		reflectForward = curCam->GetDir();

		DrawReflections(&clipPlaneEqs[0], true, true);
	}

	CCameraHandler::SetActiveCamera(prvCam->GetCamType());

	prvCam->Update();
	prvCam->LoadViewport();
}

void CDynWater::DrawRefraction(const CGame* game)
{
	RECOIL_DETAILED_TRACY_ZONE;
	camera->Update();

	refractRight = camera->GetRight();
	refractUp = camera->GetUp();
	refractForward = camera->GetDir();


	refractFBO.Bind();
	glViewport(0, 0, refractSize, refractSize);

	const auto& sky = ISky::GetSky();
	glClearColor(sky->fogColor.x, sky->fogColor.y, sky->fogColor.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	const double clipPlaneEqs[2 * 4] = {
		0.0, -1.0, 0.0, 5.0, // ground
		0.0, -1.0, 0.0, 0.0, // models
	};

	const float3 oldsun = sunLighting->modelDiffuseColor;
	const float3 oldambient = sunLighting->modelAmbientColor;

	sunLighting->modelDiffuseColor *= float3(0.5f, 0.7f, 0.9f);
	sunLighting->modelAmbientColor *= float3(0.6f, 0.8f, 1.0f);

	DrawRefractions(&clipPlaneEqs[0], true, true);

	globalRendering->LoadViewport();
	glClearColor(sky->fogColor.x, sky->fogColor.y, sky->fogColor.z, 1);

	sunLighting->modelDiffuseColor = oldsun;
	sunLighting->modelAmbientColor = oldambient;
}

void CDynWater::DrawWaves()
{
	RECOIL_DETAILED_TRACY_ZONE;
	float dx = camPosBig.x - oldCamPosBig.x;
	float dy = camPosBig.z - oldCamPosBig.z;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	/*glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE5_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);*/


	float start = 0.1f / 1024;
	float end = 1023.9f / 1024;

	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 8,  -1.0f/1024, 1.0f/1024, 0,0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 9,  0,          1.0f/1024, 0,0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 10, 1.0f/1024,  1.0f/1024, 0,0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 11, 1.0f/1024,  0, 0,0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 12, float(WF_SIZE)/(mapDims.mapxp1*SQUARE_SIZE), float(WF_SIZE)/(mapDims.mapyp1*SQUARE_SIZE), 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 13, (camPosBig.x-WH_SIZE)/(mapDims.pwr2mapx*SQUARE_SIZE), (camPosBig.z-WH_SIZE)/(mapDims.pwr2mapy*SQUARE_SIZE), 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 14, dx/WF_SIZE, dy/WF_SIZE, 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 15, (camPosBig.x-WH_SIZE)/WF_SIZE*4, (camPosBig.x-WH_SIZE)/WF_SIZE*4, 0, 0);

	//////////////////////////////////////

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, waveTex3, 0);

	glViewport(0, 0, 1024, 1024);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_L(L_WARNING, "[DynWater::%s][1] FBO not ready", __func__);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glActiveTextureARB(GL_TEXTURE5_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glActiveTextureARB(GL_TEXTURE6_ARB);
	glBindTexture(GL_TEXTURE_2D, readMap->GetShadingTexture ());
	glActiveTextureARB(GL_TEXTURE0_ARB);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, waveFP2);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, waveVP2);
	glEnable(GL_VERTEX_PROGRAM_ARB);

	//update flows pass
	int resetTexs[] = { 0, 1, 2, 3, 4, 5, -1 };
	DrawUpdateSquare(dx, dy, resetTexs);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glFlush();


	///////////////////////////////////////

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, waveTex2, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_L(L_WARNING, "[DynWater::%s][2] FBO not ready", __func__);


	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glActiveTextureARB(GL_TEXTURE5_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex3);
	glActiveTextureARB(GL_TEXTURE6_ARB);
	glBindTexture(GL_TEXTURE_2D, detailNormalTex);
	glActiveTextureARB(GL_TEXTURE0_ARB);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, waveFP);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, waveVP);

	// update height pass
	int resetTexs2[] = {0, -1};
	DrawUpdateSquare(dx, dy, resetTexs2);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glFlush();

	////////////////////////////////

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex2);
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex2);
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex2);
	glActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE5_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE6_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, waveTex1, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_L(L_WARNING, "[DynWater::%s][3] FBO not ready", __func__);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, waveNormalFP);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, waveNormalVP);

	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0, 0, 0, W_SIZE*2, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,1, W_SIZE*2, 0, 0, 0);

	//update normals pass
	glBegin(GL_QUADS);
	glTexCoord2f(start, start); glVertexf3(ZeroVector);
	glTexCoord2f(start, end);   glVertexf3(  UpVector);
	glTexCoord2f(end,   end);   glVertexf3(  XYVector);
	glTexCoord2f(end,   start); glVertexf3( RgtVector);
	glEnd();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glFlush();

	glDisable(GL_VERTEX_PROGRAM_ARB);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);

	unsigned int temp = waveTex1;
	waveTex1 = waveTex2;
	waveTex2 = waveTex3;
	waveTex3 = temp;
}

void CDynWater::DrawHeightTex()
{
	RECOIL_DETAILED_TRACY_ZONE;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, waveHeight32, 0);

	glViewport(0, 0, 256, 256);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_L(L_WARNING, "[DynWater::%s] FBO not ready", __func__);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, waveCopyHeightFP);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, waveCopyHeightVP);
	glEnable(GL_VERTEX_PROGRAM_ARB);

	camPosX = int(camera->GetPos().x / W_SIZE);
	camPosZ = int(camera->GetPos().z / W_SIZE);

	float startx = (camPosX - 120)/1024.0f - (camPosBig.x - WH_SIZE)/WF_SIZE;
	float startz = (camPosZ - 120)/1024.0f - (camPosBig.z - WH_SIZE)/WF_SIZE;
	float endx   = (camPosX + 120)/1024.0f - (camPosBig.x - WH_SIZE)/WF_SIZE;
	float endz   = (camPosZ + 120)/1024.0f - (camPosBig.z - WH_SIZE)/WF_SIZE;
	float startv = 8.0f / 256;
	float endv   = 248.0f / 256;
	//update 32 bit height map
	glBegin(GL_QUADS);
	glTexCoord2f(startx, startz); glVertex3f(startv, startv, 0);
	glTexCoord2f(startx, endz);   glVertex3f(startv, endv,   0);
	glTexCoord2f(endx,   endz);   glVertex3f(endv,   endv,   0);
	glTexCoord2f(endx,   startz); glVertex3f(endv,   startv, 0);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, waveTex1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	glDisable(GL_VERTEX_PROGRAM_ARB);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glFlush();
}


// ((40*2*2)*(2<<5))>>1 == WF_SIZE
#define LOD_SIZE_FACT (40*2*2)
#define WSQUARE_SIZE W_SIZE

static CVertexArray* va;

static inline void DrawVertexAQ(int x, int y)
{
	va->AddVertexQ0(x*WSQUARE_SIZE, 0, y*WSQUARE_SIZE);
}

void CDynWater::DrawWaterSurface()
{
	RECOIL_DETAILED_TRACY_ZONE;
	bool inStrip = false;

	va = GetVertexArray();
	va->Initialize();

	CCamera* cam = CCameraHandler::GetActiveCamera();
	cam->CalcFrustumLines(readMap->GetCurrMinHeight() - 100.0f, readMap->GetCurrMaxHeight() + 100.0f, SQUARE_SIZE);

	camPosBig2.x = std::floor(std::max((float)WH_SIZE, std::min((float)mapDims.mapx*SQUARE_SIZE - WH_SIZE, cam->GetPos().x))/(W_SIZE*16))*(W_SIZE*16);
	camPosBig2.z = std::floor(std::max((float)WH_SIZE, std::min((float)mapDims.mapy*SQUARE_SIZE - WH_SIZE, cam->GetPos().z))/(W_SIZE*16))*(W_SIZE*16);

	const CCamera::FrustumLine* negLines = cam->GetNegFrustumLines();
	const CCamera::FrustumLine* posLines = cam->GetPosFrustumLines();

	for (int lod = 1; lod < (2 << 5); lod *= 2) {
		int cx = (int)(cam->GetPos().x / WSQUARE_SIZE);
		int cy = (int)(cam->GetPos().z / WSQUARE_SIZE);

		cx = (cx / lod) * lod;
		cy = (cy / lod) * lod;

		const int hlod = lod >> 1;
		const int ysquaremod = (cy % (2 * lod)) / lod;
		const int xsquaremod = (cx % (2 * lod)) / lod;

		const int minty = int(camPosBig2.z / WSQUARE_SIZE - 512);
		const int maxty = int(camPosBig2.z / WSQUARE_SIZE + 512);
		const int mintx = int(camPosBig2.x / WSQUARE_SIZE - 512);
		const int maxtx = int(camPosBig2.x / WSQUARE_SIZE + 512);

		const int minly = cy + (-LOD_SIZE_FACT + 2 - ysquaremod) * lod;
		const int maxly = cy + ( LOD_SIZE_FACT     - ysquaremod) * lod;
		const int minlx = cx + (-LOD_SIZE_FACT + 2 - xsquaremod) * lod;
		const int maxlx = cx + ( LOD_SIZE_FACT     - xsquaremod) * lod;

		const int xstart = std::max(minlx, mintx);
		const int xend   = std::min(maxlx, maxtx);
		const int ystart = std::max(minly, minty);
		const int yend   = std::min(maxly, maxty);

		const int vrhlod = LOD_SIZE_FACT * hlod;

		// TODO: split drawing from (duplicated) GridVisibility code below
		for (int y = ystart; y < yend; y += lod) {
			int xs = xstart;
			int xe = xend;
			int xtest,xtest2;

			for (int idx = 0, cnt = negLines[4].sign * 0; idx < cnt; idx++) {
				const CCamera::FrustumLine& fl = negLines[idx];
				const float xtf = fl.base / WSQUARE_SIZE + fl.dir * y;

				xtest  = ((int) xtf                ) / lod * lod - lod;
				xtest2 = ((int)(xtf + fl.dir * lod)) / lod * lod - lod;

				xtest = std::max(xtest, xtest2);
				xs = std::max(xs, xtest);
			}
			for (int idx = 0, cnt = posLines[4].sign * 0; idx < cnt; idx++) {
				const CCamera::FrustumLine& fl = posLines[idx];
				const float xtf = fl.base / WSQUARE_SIZE + fl.dir * y;

				xtest  = ((int) xtf                ) / lod * lod - lod;
				xtest2 = ((int)(xtf + fl.dir * lod)) / lod * lod - lod;

				xtest = std::min(xtest, xtest2);
				xe = std::min(xe, xtest);
			}

			const int ylod = y + lod;
			const int yhlod = y + hlod;

			const int nloop = (xe - xs) / lod + 1;
			va->EnlargeArrays(nloop*13);
			for (int x = xs; x < xe; x += lod) {
				const int xlod = x + lod;
				const int xhlod = x + hlod;

				if ((lod == 1) || (x > (cx + vrhlod)) || (x < (cx - vrhlod)) || (y > (cy + vrhlod)) || (y < (cy - vrhlod))) { // normal terrain
					if (!inStrip) {
						DrawVertexAQ(x, y);
						DrawVertexAQ(x, ylod);
						inStrip = true;
					}
					DrawVertexAQ(xlod, y);
					DrawVertexAQ(xlod, ylod);
				} else { // inre begr?sning mot f?eg?nde lod FIXME
					if (x >= (cx + vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(x,y);
						DrawVertexAQ(x,yhlod);
						DrawVertexAQ(xhlod,y);
						DrawVertexAQ(xhlod,yhlod);
						va->EndStrip();
						DrawVertexAQ(x,yhlod);
						DrawVertexAQ(x,ylod);
						DrawVertexAQ(xhlod,yhlod);
						DrawVertexAQ(xhlod,ylod);
						va->EndStrip();
						DrawVertexAQ(xhlod,ylod);
						DrawVertexAQ(xlod,ylod);
						DrawVertexAQ(xhlod,yhlod);
						DrawVertexAQ(xlod,y);
						DrawVertexAQ(xhlod,y);
						va->EndStrip();
					} else if (x <= (cx - vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xlod,  y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xhlod, y);
						va->EndStrip();
						DrawVertexAQ(xlod,  ylod);
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xhlod, ylod);
						DrawVertexAQ(xhlod, yhlod);
						va->EndStrip();
						DrawVertexAQ(xhlod, y);
						DrawVertexAQ(x,     y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(x,     ylod);
						DrawVertexAQ(xhlod, ylod);
						va->EndStrip();
					} else if (y >= (cy + vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(x,     y);
						DrawVertexAQ(x,     yhlod);
						DrawVertexAQ(xhlod, y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xlod,  y);
						DrawVertexAQ(xlod,  yhlod);
						va->EndStrip();
						DrawVertexAQ(x,     yhlod);
						DrawVertexAQ(x,     ylod);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xlod,  ylod);
						DrawVertexAQ(xlod,  yhlod);
						va->EndStrip();
					} else if (y <= (cy - vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(x,     yhlod);
						DrawVertexAQ(x,     ylod);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xhlod, ylod);
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xlod,  ylod);
						va->EndStrip();
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xlod,  y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(x,     y);
						DrawVertexAQ(x,     yhlod);
						va->EndStrip();
					}
				}
			}
			if (inStrip) {
				va->EndStrip();
				inStrip = false;
			}
		}
	}
	va->DrawArray0(GL_TRIANGLE_STRIP);
}

void CDynWater::DrawDetailNormalTex()
{
	RECOIL_DETAILED_TRACY_ZONE;
	for (int a = 0; a < 8; ++a) {
		glActiveTextureARB(GL_TEXTURE0_ARB + a);
		glBindTexture(GL_TEXTURE_2D, rawBumpTexture[0]);
	}
	glActiveTextureARB(GL_TEXTURE0_ARB);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, detailNormalTex, 0);

	glViewport(0, 0, 256, 256);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_L(L_WARNING, "[DynWater::%s] FBO not ready", __func__);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, dwDetailNormalFP);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, dwDetailNormalVP);
	glEnable(GL_VERTEX_PROGRAM_ARB);

	float swh = 0.05f; // height of detail normal waves
	float lwh = 1.0f;  // height of larger ambient waves

	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 9, gs->frameNum, 0, 0, 0);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 10, 5, 0, 0, 1.0f/120);			//controls the position and speed of the waves
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 11, 14, 0, 0, 1.0f/90);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 12, 29, 0, 0, 1.0f/55);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 13, 9, 4, 0, 1.0f/100);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 14, -5, 14, 0, 1.0f/90);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 15, 27, 27, 0, 1.0f/75);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 16, -3, -5, 0, 1.0f/100);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 17, -10, 24, 0, 1.0f/60);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0, 0.2f*swh, 0.0f*swh, 0.7f*lwh, 0);		//controls the height of the waves
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,1, 0.2f*swh, 0.0f*swh, 0.7f*lwh, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,2, 0.2f*swh, 0.0f*swh, 0.7f*lwh, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,3, 0.2f*swh, 0.01f*swh, 0.4f*lwh, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,4, 0.07f*swh, 0.2f*swh, 0.7f*lwh, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,5, 0.2f*swh, 0.2f*swh, 0.7f*lwh, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,6, 0.12f*swh, 0.2f*swh, 0.7f*lwh, 0);
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,7, 0.08f*swh, 0.2f*swh, 0.7f*lwh, 0);

	//update detail normals
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertexf3(ZeroVector);
	glTexCoord2f(0, 1); glVertexf3(  UpVector);
	glTexCoord2f(1, 1); glVertexf3(  XYVector);
	glTexCoord2f(1, 0); glVertexf3( RgtVector);
	glEnd();


	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	glFlush();

	glBindTexture(GL_TEXTURE_2D,detailNormalTex);
	glGenerateMipmapEXT(GL_TEXTURE_2D);

}

void CDynWater::AddShipWakes()
{
	RECOIL_DETAILED_TRACY_ZONE;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, waveTex1, 0);

	glViewport(0, 0, 1024, 1024);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_L(L_WARNING, "[DynWater::%s] FBO not ready", __func__);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, dwAddSplashFP);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, dwAddSplashVP);
	glEnable(GL_VERTEX_PROGRAM_ARB);
	glDisable(GL_CULL_FACE);

	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 10, 1.0f/WF_SIZE, 1.0f/WF_SIZE, 0, 1);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 11, -(oldCamPosBig.x - WH_SIZE)/WF_SIZE, -(oldCamPosBig.z - WH_SIZE)/WF_SIZE, 0, 0);

	CVertexArray* va = GetVertexArray();
	va->Initialize();
	CVertexArray* va2 = GetVertexArray(); // never try to get more than 2 at once
	va2->Initialize();

	{
		const auto& units = CUnitDrawer::GetUnsortedUnits();
		const int nadd = units.size() * 4;

		va->EnlargeArrays(nadd, 0, VA_SIZE_TN);
		va2->EnlargeArrays(nadd, 0, VA_SIZE_TN);

		for (const CUnit* unit: units) {
			const MoveDef* moveDef = unit->moveDef;

			if (moveDef == NULL)
				continue;

			if (moveDef->speedModClass == MoveDef::Hover) {
				// hovercraft
				const float3& pos = unit->pos;

				if ((std::fabs(pos.x - camPosBig.x) > (WH_SIZE - 50)) ||
					(std::fabs(pos.z - camPosBig.z) > (WH_SIZE - 50)))
				{
					continue;
				}
				if (!(unit->losStatus[gu->myAllyTeam] & LOS_INLOS) && !gu->spectatingFullView)
					continue;

				if ((pos.y > -4.0f) && (pos.y < 4.0f)) {
					const float3 frontAdd = unit->frontdir * unit->radius * 0.75f;
					const float3 sideAdd = unit->rightdir * unit->radius * 0.75f;
					const float depth = std::sqrt(std::sqrt(unit->mass)) * 0.4f;
					const float3 n(depth, 0.05f * depth, depth);

					va2->AddVertexQTN(pos + frontAdd + sideAdd, 0, 0, n);
					va2->AddVertexQTN(pos + frontAdd - sideAdd, 1, 0, n);
					va2->AddVertexQTN(pos - frontAdd - sideAdd, 1, 1, n);
					va2->AddVertexQTN(pos - frontAdd + sideAdd, 0, 1, n);
				}
			} else if (moveDef->speedModClass == MoveDef::Ship) {
				// surface ship
				const float3& pos = unit->pos;

				if ((std::fabs(pos.x - camPosBig.x) > (WH_SIZE - 50)) ||
					(std::fabs(pos.z - camPosBig.z) > (WH_SIZE - 50)))
				{
					continue;
				}
				if (!(unit->losStatus[gu->myAllyTeam] & LOS_INLOS) && !gu->spectatingFullView)
					continue;

				// skip submarines (which have deep waterlines)
				if (unit->IsUnderWater() || !unit->IsInWater())
					continue;

				const float3 frontAdd = unit->frontdir * unit->radius * 0.75f;
				const float3 sideAdd = unit->rightdir * unit->radius * 0.18f;
				const float depth = std::sqrt(std::sqrt(unit->mass));
				const float3 n(depth, 0.04f * unit->speed.Length2D() * depth, depth);

				va->AddVertexQTN(pos + frontAdd + sideAdd, 0, 0, n);
				va->AddVertexQTN(pos + frontAdd - sideAdd, 1, 0, n);
				va->AddVertexQTN(pos - frontAdd - sideAdd, 1, 1, n);
				va->AddVertexQTN(pos - frontAdd + sideAdd, 0, 1, n);
			}
		}
	}

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, boatShape);

	va->DrawArrayTN(GL_QUADS);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, hoverShape);

	va2->DrawArrayTN(GL_QUADS);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	glFlush();
}

void CDynWater::AddExplosions()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (explosions.empty()) {
		return;
	}

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, waveTex1, 0);

	glViewport(0, 0, 1024, 1024);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, splashTex);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
		LOG_L(L_WARNING, "[DynWater::%s][1] FBO not ready", __func__);

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, dwAddSplashFP);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, dwAddSplashVP);
	glEnable(GL_VERTEX_PROGRAM_ARB);
	glDisable(GL_CULL_FACE);

	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 10, 1.0f/WF_SIZE, 1.0f/WF_SIZE, 0, 1);
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 11, -(oldCamPosBig.x - WH_SIZE)/WF_SIZE, -(oldCamPosBig.z - WH_SIZE)/WF_SIZE, 0, 0);

	CVertexArray* va = GetVertexArray();
	va->Initialize();

	const int nadd = explosions.size()*4;
	va->EnlargeArrays(nadd, 0, VA_SIZE_TN);

	for (std::vector<Explosion>::iterator ei = explosions.begin(); ei != explosions.end(); ++ei) {
		Explosion& explo = *ei;
		float3 pos = explo.pos;
		if ((std::fabs(pos.x - camPosBig.x) > (WH_SIZE - 50))
				|| (std::fabs(pos.z - camPosBig.z) > (WH_SIZE - 50)))
		{
			continue;
		}
		float inv = 1.01f;
		if (pos.y < 0) {
			if (pos.y < -explo.radius*0.5f) {
				inv = 0.99f;
			}
			pos.y = pos.y*-0.5f;
		}

		float size = explo.radius - pos.y;
		if (size < 8) {
			continue;
		}
		float strength = explo.strength * (size / explo.radius)*0.5f;

		const float3 n(strength, strength*0.005f, strength*inv);

		va->AddVertexQTN(pos + (float3(1.0f,  0.0f, 1.0f) * size),  0, 0, n);
		va->AddVertexQTN(pos + (float3(-1.0f, 0.0f, 1.0f) * size),  1, 0, n);
		va->AddVertexQTN(pos + (float3(-1.0f, 0.0f, -1.0f) * size), 1, 1, n);
		va->AddVertexQTN(pos + (float3(1.0f,  0.0f, -1.0f) * size), 0, 1, n);
	}
	explosions.clear();

	va->DrawArrayTN(GL_QUADS);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	glFlush();
}

void CDynWater::AddExplosion(const float3& pos, float strength, float size)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if ((pos.y > size) || (size < 8)) {
		return;
	}

	explosions.push_back(Explosion(pos, std::min(size*20, strength), size));
}

void CDynWater::DrawUpdateSquare(float dx, float dy, int* resetTexs)
{
	RECOIL_DETAILED_TRACY_ZONE;
	float startx = std::max(0.f, -dx/WF_SIZE);
	float starty = std::max(0.f, -dy/WF_SIZE);
	float endx   = std::min(1.f, 1 - dx/WF_SIZE);
	float endy   = std::min(1.f, 1 - dy/WF_SIZE);

	DrawSingleUpdateSquare(startx, starty, endx, endy);

	int a = 0;
	while (resetTexs[a] >= 0) {
		glActiveTextureARB(GL_TEXTURE0_ARB + resetTexs[a]);
		glBindTexture(GL_TEXTURE_2D, zeroTex);
		++a;
	}
	glActiveTextureARB(GL_TEXTURE0_ARB);

	if (startx > 0) {
		DrawSingleUpdateSquare(0, 0, startx, 1);
	} else if(endx < 1) {
		DrawSingleUpdateSquare(endx, 0, 1, 1);
	}

	if (starty > 0) {
		DrawSingleUpdateSquare(startx, 0, endx, starty);
	} else if (endy < 1) {
		DrawSingleUpdateSquare(startx, endy, endx, 1);
	}
}

void CDynWater::DrawSingleUpdateSquare(float startx, float starty, float endx, float endy)
{
	RECOIL_DETAILED_TRACY_ZONE;
	float texstart = 0.1f / 1024;
	float texend = 1023.9f / 1024;
	float texdif = texend - texstart;

	CVertexArray* va = GetVertexArray();
	va->Initialize();
	va->CheckInitSize(4 * VA_SIZE_T);

	va->AddVertexQT(float3(startx, starty, 0), texstart + startx*texdif, texstart + starty*texdif);
	va->AddVertexQT(float3(startx, endy,   0), texstart + startx*texdif, texstart + endy*texdif);
	va->AddVertexQT(float3(endx,   endy,   0), texstart + endx*texdif,   texstart + endy*texdif);
	va->AddVertexQT(float3(endx,   starty, 0), texstart + endx*texdif,   texstart + starty*texdif);

	va->DrawArrayT(GL_QUADS);
}

void CDynWater::DrawOuterSurface()
{
	RECOIL_DETAILED_TRACY_ZONE;
	CVertexArray* va = GetVertexArray();
	va->Initialize();

	va->EnlargeArrays(3*3*16*16*4);
	float posx = camPosBig2.x - WH_SIZE - WF_SIZE;
	float posy = camPosBig2.z - WH_SIZE - WF_SIZE;

	float ys = posy;
	for (int y = -1; y <= 1; ++y, ys += WF_SIZE) { //! CAUTION: loop count must match EnlargeArrays above
		float xs = posx;
		for (int x = -1; x <= 1; ++x, xs += WF_SIZE) {
			if ((x == 0) && (y == 0)) {
				continue;
			}
			float pys = ys;
			for (int y2 = 0; y2 < 16; ++y2) { //! CAUTION: loop count must match EnlargeArrays above
				float pxs = xs;
				const float pys1 = pys + WF_SIZE/16;
				for (int x2 = 0; x2 < 16; ++x2) {
					const float pxs1 = pxs + WF_SIZE/16;
					va->AddVertexQ0(pxs,  0, pys);
					va->AddVertexQ0(pxs1, 0, pys);
					va->AddVertexQ0(pxs1, 0, pys1);
					va->AddVertexQ0(pxs,  0, pys1);
					pxs = pxs1;
				}
				pys = pys1;
			}
		}
	}

	va->DrawArray0(GL_QUADS);
}
