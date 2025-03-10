/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "Combiner.h"
#include "Game/GlobalUnsynced.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Rendering/Shaders/Shader.h"
#include "Map/ReadMap.h"
#include "System/Exceptions.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

#include "System/Misc/TracyDefs.h"


CONFIG(bool, HighResInfoTexture).defaultValue(true).description("Use full heightmap resolution info texture (true), or half resolution (false)");


CInfoTextureCombiner::CInfoTextureCombiner()
: CPboInfoTexture("info")
, disabled(true)
{
	texSize = (configHandler->GetBool("HighResInfoTexture")) ? int2(mapDims.pwr2mapx, mapDims.pwr2mapy) : int2(mapDims.pwr2mapx >> 1, mapDims.pwr2mapy >> 1);
	//texSize = (configHandler->GetBool("HighResInfoTexture")) ? int2(512, 512) : int2(256, 256);
	texChannels = 4;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// precision hint:
	// GL_RGBA8 hasn't enough precision for subtle blending operations,
	// cause we update the texture slowly with alpha values of 0.05, so
	// per update the change is too subtle. With just 8bits per channel
	// the update is too less to change the texture value at all. The
	// texture keeps unchanged then and never reaches the wanted final
	// state, keeping `shadow` images of the old state.
	// Testing showed that 10bits per channel are already good enough to
	// counter this, GL_RGBA16 would be another solution, but needs twice
	// as much texture memory + bandwidth.
	// Also GL3.x enforces that GL_RGB10_A2 must be renderable.
	RecoilTexStorage2D(GL_TEXTURE_2D, -1, GL_RGB10_A2, texSize.x, texSize.y);

	if (FBO::IsSupported()) {
		fbo.Bind();
		fbo.AttachTexture(texture);
		/*bool status =*/ fbo.CheckStatus("CInfoTextureCombiner");
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
		if (fbo.IsValid()) glClear(GL_COLOR_BUFFER_BIT);
		FBO::Unbind();

		// create mipmaps
		glBindTexture(GL_TEXTURE_2D, texture);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	shader = shaderHandler->CreateProgramObject("[CInfoTextureCombiner]", "CInfoTextureCombiner");

	if (!fbo.IsValid() /*|| !shader->IsValid()*/) { // don't check shader (it gets created/switched at runtime)
		throw opengl_error("");
	}
}

CInfoTextureCombiner::~CInfoTextureCombiner()
{
	RECOIL_DETAILED_TRACY_ZONE;
	shaderHandler->ReleaseProgramObject("[CInfoTextureCombiner]", "CInfoTextureCombiner");
}


void CInfoTextureCombiner::SwitchMode(const std::string& name)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (name.empty()) {
		disabled = true;

		CreateShader(curMode = "", true);
		return;
	}

	// WTF? fully reloaded from disk on every switch?
	// TODO: allow "info:myluainfotex"
	switch (hashString(name.c_str())) {
		case hashString("los"   ): { disabled = !CreateShader("shaders/GLSL/infoLOS.lua"   , true, float4(0.5f, 0.5f, 0.5f, 1.0f)); } break;
		case hashString("metal" ): { disabled = !CreateShader("shaders/GLSL/infoMetal.lua" , true, float4(0.0f, 0.0f, 0.0f, 1.0f)); } break;
		case hashString("height"): { disabled = !CreateShader("shaders/GLSL/infoHeight.lua"                                      ); } break;
		case hashString("path"  ): { disabled = !CreateShader("shaders/GLSL/infoPath.lua"                                        ); } break;
		default                  : { disabled = !CreateShader(name                                                               ); } break;
	}

	curMode = (disabled) ? "" : name;
}


bool CInfoTextureCombiner::CreateShader(const std::string& filename, const bool clear, const float4 clearColor)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (clear) {
		// clear
		fbo.Bind();
		glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
		if (fbo.IsValid()) glClear(GL_COLOR_BUFFER_BIT);
		FBO::Unbind();

		// create mipmaps
		glBindTexture(GL_TEXTURE_2D, texture);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	if (filename.empty())
		return false;

	shader->Release();
	return shader->LoadFromLua(filename);
}


void CInfoTextureCombiner::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	shader->Enable();
	fbo.Bind();
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glViewport(0,0, texSize.x, texSize.y);
	glEnable(GL_BLEND);

	shader->BindTextures();
	shader->SetUniform("time", gu->gameTime);

	const float isx = 2.0f * (mapDims.mapx / float(mapDims.pwr2mapx)) - 1.0f;
	const float isy = 2.0f * (mapDims.mapy / float(mapDims.pwr2mapy)) - 1.0f;

	glBegin(GL_QUADS);
		glTexCoord2f(0.f, 0.f); glVertex2f(-1.f, -1.f);
		glTexCoord2f(0.f, 1.f); glVertex2f(-1.f, +isy);
		glTexCoord2f(1.f, 1.f); glVertex2f(+isx, +isy);
		glTexCoord2f(1.f, 0.f); glVertex2f(+isx, -1.f);
	glEnd();

	globalRendering->LoadViewport();
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	FBO::Unbind();
	shader->Disable();

	// create mipmaps
	glBindTexture(GL_TEXTURE_2D, texture);
	glGenerateMipmap(GL_TEXTURE_2D);
}
