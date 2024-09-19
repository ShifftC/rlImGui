/**********************************************************************************************
*
*   raylibExtras * Utilities and Shared Components for Raylib
*
*   rlImGui * basic ImGui integration
*
*   LICENSE: ZLIB
*
*   Copyright (c) 2024 Jeffery Myers
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files (the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions:
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*
**********************************************************************************************/
// Modified - Copyright 2024 ShifftC. Licensed under the Apache License, Version 2.0.

#include "rlImGui.h"

#include <array>
#include <cmath>
#include <limits>
#include <map>

#include <imgui.h>
#include <raylib.h>
#include <rlgl.h>

#ifndef NO_FONT_AWESOME
#include "extras/FA6FreeSolidFontData.h"
#endif
#include "imgui_impl_raylib.h"

static constexpr std::array MouseCursorMap{[]() {
	std::array<MouseCursor, ImGuiMouseCursor_COUNT> TempMouseCursorMap{};
	TempMouseCursorMap[ImGuiMouseCursor_Arrow] = MOUSE_CURSOR_ARROW;
	TempMouseCursorMap[ImGuiMouseCursor_TextInput] = MOUSE_CURSOR_IBEAM;
	TempMouseCursorMap[ImGuiMouseCursor_Hand] = MOUSE_CURSOR_POINTING_HAND;
	TempMouseCursorMap[ImGuiMouseCursor_ResizeAll] = MOUSE_CURSOR_RESIZE_ALL;
	TempMouseCursorMap[ImGuiMouseCursor_ResizeEW] = MOUSE_CURSOR_RESIZE_EW;
	TempMouseCursorMap[ImGuiMouseCursor_ResizeNESW] = MOUSE_CURSOR_RESIZE_NESW;
	TempMouseCursorMap[ImGuiMouseCursor_ResizeNS] = MOUSE_CURSOR_RESIZE_NS;
	TempMouseCursorMap[ImGuiMouseCursor_ResizeNWSE] = MOUSE_CURSOR_RESIZE_NWSE;
	TempMouseCursorMap[ImGuiMouseCursor_NotAllowed] = MOUSE_CURSOR_NOT_ALLOWED;
	return TempMouseCursorMap;
}()};

static ImGuiMouseCursor CurrentMouseCursor{ImGuiMouseCursor_COUNT};
ImGuiContext* GlobalContext = nullptr;
static std::map<KeyboardKey, ImGuiKey> RaylibKeyMap;
static bool LastFrameFocused = false;
static bool LastControlPressed = false;
static bool LastShiftPressed = false;
static bool LastAltPressed = false;
static bool LastSuperPressed = false;

// internal only functions
bool rlImGuiIsControlDown() { return IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_CONTROL); }
bool rlImGuiIsShiftDown() { return IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_LEFT_SHIFT); }
bool rlImGuiIsAltDown() { return IsKeyDown(KEY_RIGHT_ALT) || IsKeyDown(KEY_LEFT_ALT); }
bool rlImGuiIsSuperDown() { return IsKeyDown(KEY_RIGHT_SUPER) || IsKeyDown(KEY_LEFT_SUPER); }

void ReloadFonts()
{
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels = nullptr;

	int width;
	int height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, nullptr);
	Image image = GenImageColor(width, height, BLANK);
	memcpy(image.data, pixels, width * height * 4);

	Texture2D* fontTexture = (Texture2D*)io.Fonts->TexID;
	if (fontTexture && fontTexture->id != 0)
	{
		UnloadTexture(*fontTexture);
		MemFree(fontTexture);
	}

	fontTexture = (Texture2D*)MemAlloc(sizeof(Texture2D));
	*fontTexture = LoadTextureFromImage(image);
	UnloadImage(image);
	io.Fonts->TexID = fontTexture;
}

static const char* GetClipTextCallback(ImGuiContext*)
{
	return GetClipboardText();
}

static void SetClipTextCallback(ImGuiContext*, const char* text)
{
	SetClipboardText(text);
}

static void ImGuiNewFrame(float deltaTime)
{
	ImGuiIO& io = ImGui::GetIO();


	Vector2 resolutionScale = GetWindowScaleDPI();

#ifndef PLATFORM_DRM
	if (IsWindowFullscreen())
	{
		int monitor = GetCurrentMonitor();
		io.DisplaySize.x = float(GetMonitorWidth(monitor));
		io.DisplaySize.y = float(GetMonitorHeight(monitor));
	}
	else
	{
		io.DisplaySize.x = float(GetScreenWidth());
		io.DisplaySize.y = float(GetScreenHeight());
	}

#if !defined(__APPLE__)
	if (!IsWindowState(FLAG_WINDOW_HIGHDPI))
		resolutionScale = Vector2{1, 1};
#endif
#else
    io.DisplaySize.x = float(GetScreenWidth());
    io.DisplaySize.y = float(GetScreenHeight());
#endif

	io.DisplayFramebufferScale = ImVec2(resolutionScale.x, resolutionScale.y);

	io.DeltaTime = deltaTime;

	if (io.WantSetMousePos)
	{
		SetMousePosition((int)io.MousePos.x, (int)io.MousePos.y);
	}
	else
	{
		io.AddMousePosEvent((float)GetMouseX(), (float)GetMouseY());
	}

	auto setMouseEvent = [&io](int rayMouse, int imGuiMouse) {
		if (IsMouseButtonPressed(rayMouse))
			io.AddMouseButtonEvent(imGuiMouse, true);
		else if (IsMouseButtonReleased(rayMouse))
			io.AddMouseButtonEvent(imGuiMouse, false);
	};

	setMouseEvent(MOUSE_BUTTON_LEFT, ImGuiMouseButton_Left);
	setMouseEvent(MOUSE_BUTTON_RIGHT, ImGuiMouseButton_Right);
	setMouseEvent(MOUSE_BUTTON_MIDDLE, ImGuiMouseButton_Middle);
	setMouseEvent(MOUSE_BUTTON_FORWARD, ImGuiMouseButton_Middle + 1);
	setMouseEvent(MOUSE_BUTTON_BACK, ImGuiMouseButton_Middle + 2);

	{
		Vector2 mouseWheel = GetMouseWheelMoveV();
		io.AddMouseWheelEvent(mouseWheel.x, mouseWheel.y);
	}

	if (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_HasMouseCursors)
	{
		if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0)
		{
			ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
			if (imgui_cursor != CurrentMouseCursor || io.MouseDrawCursor)
			{
				CurrentMouseCursor = imgui_cursor;
				if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None)
				{
					HideCursor();
				}
				else
				{
					ShowCursor();

					if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange))
					{
						SetMouseCursor((imgui_cursor > -1 && imgui_cursor < ImGuiMouseCursor_COUNT)
							               ? MouseCursorMap[imgui_cursor] : MOUSE_CURSOR_DEFAULT);
					}
				}
			}
		}
	}
}

static void ImGuiTriangleVert(ImDrawVert& idx_vert)
{
	Color* c;
	c = (Color*)&idx_vert.col;
	rlColor4ub(c->r, c->g, c->b, c->a);
	rlTexCoord2f(idx_vert.uv.x, idx_vert.uv.y);
	rlVertex2f(idx_vert.pos.x, idx_vert.pos.y);
}

static void ImGuiRenderTriangles(unsigned int count,
                                 int indexStart,
                                 const ImVector<ImDrawIdx>& indexBuffer,
                                 const ImVector<ImDrawVert>& vertBuffer,
                                 void* texturePtr)
{
	if (count < 3)
		return;

	Texture* texture = (Texture*)texturePtr;

	unsigned int textureId = (texture == nullptr) ? 0 : texture->id;

	rlBegin(RL_TRIANGLES);
	rlSetTexture(textureId);

	for (unsigned int i = 0; i <= (count - 3); i += 3)
	{
		ImDrawIdx indexA = indexBuffer[indexStart + i];
		ImDrawIdx indexB = indexBuffer[indexStart + i + 1];
		ImDrawIdx indexC = indexBuffer[indexStart + i + 2];

		ImDrawVert vertexA = vertBuffer[indexA];
		ImDrawVert vertexB = vertBuffer[indexB];
		ImDrawVert vertexC = vertBuffer[indexC];

		ImGuiTriangleVert(vertexA);
		ImGuiTriangleVert(vertexB);
		ImGuiTriangleVert(vertexC);
	}
	rlEnd();
}

static void EnableScissor(float x, float y, float width, float height)
{
	rlEnableScissorTest();
	ImGuiIO& io = ImGui::GetIO();

	ImVec2 scale = io.DisplayFramebufferScale;
#if !defined(__APPLE__)
	if (!IsWindowState(FLAG_WINDOW_HIGHDPI))
	{
		scale.x = 1;
		scale.y = 1;
	}
#endif

	rlScissor((int)(x * scale.x),
	          int((io.DisplaySize.y - (int)(y + height)) * scale.y),
	          (int)(width * scale.x),
	          (int)(height * scale.y));
}

void SetupFontAwesome()
{
#ifndef NO_FONT_AWESOME
	static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.FontDataOwnedByAtlas = false;

	icons_config.GlyphMaxAdvanceX = std::numeric_limits<float>::max();
	icons_config.RasterizerMultiply = 1.0f;
	icons_config.OversampleH = 2;
	icons_config.OversampleV = 1;

	icons_config.GlyphRanges = icons_ranges;

	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontFromMemoryCompressedTTF((void*)fa_solid_900_compressed_data, fa_solid_900_compressed_size,
	                                         FONT_AWESOME_ICON_SIZE, &icons_config, icons_ranges);
#endif
}

void SetupBackend()
{
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = "imgui_impl_raylib";
	io.BackendFlags |= ImGuiBackendFlags_HasGamepad | ImGuiBackendFlags_HasSetMousePos;

#ifndef PLATFORM_DRM
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
#endif

	io.MousePos = ImVec2(0, 0);

	ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

	platformIO.Platform_SetClipboardTextFn = SetClipTextCallback;
	platformIO.Platform_GetClipboardTextFn = GetClipTextCallback;

	platformIO.Platform_ClipboardUserData = nullptr;
}

void rlImGuiEndInitImGui()
{
	ImGui::SetCurrentContext(GlobalContext);

	SetupFontAwesome();

	SetupBackend();

	ReloadFonts();
}

static void SetupKeymap()
{
	if (!RaylibKeyMap.empty())
		return;

	// build up a map of raylib keys to ImGuiKeys
	RaylibKeyMap[KEY_APOSTROPHE] = ImGuiKey_Apostrophe;
	RaylibKeyMap[KEY_COMMA] = ImGuiKey_Comma;
	RaylibKeyMap[KEY_MINUS] = ImGuiKey_Minus;
	RaylibKeyMap[KEY_PERIOD] = ImGuiKey_Period;
	RaylibKeyMap[KEY_SLASH] = ImGuiKey_Slash;
	RaylibKeyMap[KEY_ZERO] = ImGuiKey_0;
	RaylibKeyMap[KEY_ONE] = ImGuiKey_1;
	RaylibKeyMap[KEY_TWO] = ImGuiKey_2;
	RaylibKeyMap[KEY_THREE] = ImGuiKey_3;
	RaylibKeyMap[KEY_FOUR] = ImGuiKey_4;
	RaylibKeyMap[KEY_FIVE] = ImGuiKey_5;
	RaylibKeyMap[KEY_SIX] = ImGuiKey_6;
	RaylibKeyMap[KEY_SEVEN] = ImGuiKey_7;
	RaylibKeyMap[KEY_EIGHT] = ImGuiKey_8;
	RaylibKeyMap[KEY_NINE] = ImGuiKey_9;
	RaylibKeyMap[KEY_SEMICOLON] = ImGuiKey_Semicolon;
	RaylibKeyMap[KEY_EQUAL] = ImGuiKey_Equal;
	RaylibKeyMap[KEY_A] = ImGuiKey_A;
	RaylibKeyMap[KEY_B] = ImGuiKey_B;
	RaylibKeyMap[KEY_C] = ImGuiKey_C;
	RaylibKeyMap[KEY_D] = ImGuiKey_D;
	RaylibKeyMap[KEY_E] = ImGuiKey_E;
	RaylibKeyMap[KEY_F] = ImGuiKey_F;
	RaylibKeyMap[KEY_G] = ImGuiKey_G;
	RaylibKeyMap[KEY_H] = ImGuiKey_H;
	RaylibKeyMap[KEY_I] = ImGuiKey_I;
	RaylibKeyMap[KEY_J] = ImGuiKey_J;
	RaylibKeyMap[KEY_K] = ImGuiKey_K;
	RaylibKeyMap[KEY_L] = ImGuiKey_L;
	RaylibKeyMap[KEY_M] = ImGuiKey_M;
	RaylibKeyMap[KEY_N] = ImGuiKey_N;
	RaylibKeyMap[KEY_O] = ImGuiKey_O;
	RaylibKeyMap[KEY_P] = ImGuiKey_P;
	RaylibKeyMap[KEY_Q] = ImGuiKey_Q;
	RaylibKeyMap[KEY_R] = ImGuiKey_R;
	RaylibKeyMap[KEY_S] = ImGuiKey_S;
	RaylibKeyMap[KEY_T] = ImGuiKey_T;
	RaylibKeyMap[KEY_U] = ImGuiKey_U;
	RaylibKeyMap[KEY_V] = ImGuiKey_V;
	RaylibKeyMap[KEY_W] = ImGuiKey_W;
	RaylibKeyMap[KEY_X] = ImGuiKey_X;
	RaylibKeyMap[KEY_Y] = ImGuiKey_Y;
	RaylibKeyMap[KEY_Z] = ImGuiKey_Z;
	RaylibKeyMap[KEY_SPACE] = ImGuiKey_Space;
	RaylibKeyMap[KEY_ESCAPE] = ImGuiKey_Escape;
	RaylibKeyMap[KEY_ENTER] = ImGuiKey_Enter;
	RaylibKeyMap[KEY_TAB] = ImGuiKey_Tab;
	RaylibKeyMap[KEY_BACKSPACE] = ImGuiKey_Backspace;
	RaylibKeyMap[KEY_INSERT] = ImGuiKey_Insert;
	RaylibKeyMap[KEY_DELETE] = ImGuiKey_Delete;
	RaylibKeyMap[KEY_RIGHT] = ImGuiKey_RightArrow;
	RaylibKeyMap[KEY_LEFT] = ImGuiKey_LeftArrow;
	RaylibKeyMap[KEY_DOWN] = ImGuiKey_DownArrow;
	RaylibKeyMap[KEY_UP] = ImGuiKey_UpArrow;
	RaylibKeyMap[KEY_PAGE_UP] = ImGuiKey_PageUp;
	RaylibKeyMap[KEY_PAGE_DOWN] = ImGuiKey_PageDown;
	RaylibKeyMap[KEY_HOME] = ImGuiKey_Home;
	RaylibKeyMap[KEY_END] = ImGuiKey_End;
	RaylibKeyMap[KEY_CAPS_LOCK] = ImGuiKey_CapsLock;
	RaylibKeyMap[KEY_SCROLL_LOCK] = ImGuiKey_ScrollLock;
	RaylibKeyMap[KEY_NUM_LOCK] = ImGuiKey_NumLock;
	RaylibKeyMap[KEY_PRINT_SCREEN] = ImGuiKey_PrintScreen;
	RaylibKeyMap[KEY_PAUSE] = ImGuiKey_Pause;
	RaylibKeyMap[KEY_F1] = ImGuiKey_F1;
	RaylibKeyMap[KEY_F2] = ImGuiKey_F2;
	RaylibKeyMap[KEY_F3] = ImGuiKey_F3;
	RaylibKeyMap[KEY_F4] = ImGuiKey_F4;
	RaylibKeyMap[KEY_F5] = ImGuiKey_F5;
	RaylibKeyMap[KEY_F6] = ImGuiKey_F6;
	RaylibKeyMap[KEY_F7] = ImGuiKey_F7;
	RaylibKeyMap[KEY_F8] = ImGuiKey_F8;
	RaylibKeyMap[KEY_F9] = ImGuiKey_F9;
	RaylibKeyMap[KEY_F10] = ImGuiKey_F10;
	RaylibKeyMap[KEY_F11] = ImGuiKey_F11;
	RaylibKeyMap[KEY_F12] = ImGuiKey_F12;
	RaylibKeyMap[KEY_LEFT_SHIFT] = ImGuiKey_LeftShift;
	RaylibKeyMap[KEY_LEFT_CONTROL] = ImGuiKey_LeftCtrl;
	RaylibKeyMap[KEY_LEFT_ALT] = ImGuiKey_LeftAlt;
	RaylibKeyMap[KEY_LEFT_SUPER] = ImGuiKey_LeftSuper;
	RaylibKeyMap[KEY_RIGHT_SHIFT] = ImGuiKey_RightShift;
	RaylibKeyMap[KEY_RIGHT_CONTROL] = ImGuiKey_RightCtrl;
	RaylibKeyMap[KEY_RIGHT_ALT] = ImGuiKey_RightAlt;
	RaylibKeyMap[KEY_RIGHT_SUPER] = ImGuiKey_RightSuper;
	RaylibKeyMap[KEY_KB_MENU] = ImGuiKey_Menu;
	RaylibKeyMap[KEY_LEFT_BRACKET] = ImGuiKey_LeftBracket;
	RaylibKeyMap[KEY_BACKSLASH] = ImGuiKey_Backslash;
	RaylibKeyMap[KEY_RIGHT_BRACKET] = ImGuiKey_RightBracket;
	RaylibKeyMap[KEY_GRAVE] = ImGuiKey_GraveAccent;
	RaylibKeyMap[KEY_KP_0] = ImGuiKey_Keypad0;
	RaylibKeyMap[KEY_KP_1] = ImGuiKey_Keypad1;
	RaylibKeyMap[KEY_KP_2] = ImGuiKey_Keypad2;
	RaylibKeyMap[KEY_KP_3] = ImGuiKey_Keypad3;
	RaylibKeyMap[KEY_KP_4] = ImGuiKey_Keypad4;
	RaylibKeyMap[KEY_KP_5] = ImGuiKey_Keypad5;
	RaylibKeyMap[KEY_KP_6] = ImGuiKey_Keypad6;
	RaylibKeyMap[KEY_KP_7] = ImGuiKey_Keypad7;
	RaylibKeyMap[KEY_KP_8] = ImGuiKey_Keypad8;
	RaylibKeyMap[KEY_KP_9] = ImGuiKey_Keypad9;
	RaylibKeyMap[KEY_KP_DECIMAL] = ImGuiKey_KeypadDecimal;
	RaylibKeyMap[KEY_KP_DIVIDE] = ImGuiKey_KeypadDivide;
	RaylibKeyMap[KEY_KP_MULTIPLY] = ImGuiKey_KeypadMultiply;
	RaylibKeyMap[KEY_KP_SUBTRACT] = ImGuiKey_KeypadSubtract;
	RaylibKeyMap[KEY_KP_ADD] = ImGuiKey_KeypadAdd;
	RaylibKeyMap[KEY_KP_ENTER] = ImGuiKey_KeypadEnter;
	RaylibKeyMap[KEY_KP_EQUAL] = ImGuiKey_KeypadEqual;
}

static void SetupGlobals()
{
	LastFrameFocused = IsWindowFocused();
	LastControlPressed = false;
	LastShiftPressed = false;
	LastAltPressed = false;
	LastSuperPressed = false;
}

void rlImGuiBeginInitImGui()
{
	SetupGlobals();
	if (GlobalContext == nullptr)
		GlobalContext = ImGui::CreateContext(nullptr);
	SetupKeymap();

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();
}

void rlImGuiSetup(bool dark)
{
	rlImGuiBeginInitImGui();

	if (dark)
		ImGui::StyleColorsDark();
	else
		ImGui::StyleColorsLight();

	rlImGuiEndInitImGui();
}

void rlImGuiReloadFonts()
{
	ImGui::SetCurrentContext(GlobalContext);

	ReloadFonts();
}

void rlImGuiBegin()
{
	ImGui::SetCurrentContext(GlobalContext);
	rlImGuiBeginDelta(GetFrameTime());
}

void rlImGuiBeginDelta(float deltaTime)
{
	ImGui::SetCurrentContext(GlobalContext);
	ImGuiNewFrame(deltaTime);
	ImGui_ImplRaylib_ProcessEvents();
	ImGui::NewFrame();
}

void rlImGuiEnd()
{
	ImGui::SetCurrentContext(GlobalContext);
	ImGui::Render();
	ImGui_ImplRaylib_RenderDrawData(ImGui::GetDrawData());
}

void rlImGuiShutdown()
{
	if (GlobalContext == nullptr)
		return;

	ImGui::SetCurrentContext(GlobalContext);
	ImGui_ImplRaylib_Shutdown();

	ImGui::DestroyContext(GlobalContext);
	GlobalContext = nullptr;
}

void rlImGuiImage(const Texture* image)
{
	if (!image)
		return;

	if (GlobalContext)
		ImGui::SetCurrentContext(GlobalContext);

	ImGui::Image((ImTextureID)image, ImVec2(float(image->width), float(image->height)));
}

bool rlImGuiImageButton(const char* name, const Texture* image)
{
	if (!image)
		return false;

	if (GlobalContext)
		ImGui::SetCurrentContext(GlobalContext);

	return ImGui::ImageButton(name, (ImTextureID)image, ImVec2(float(image->width), float(image->height)));
}

bool rlImGuiImageButtonSize(const char* name, const Texture* image, ImVec2 size)
{
	if (!image)
		return false;

	if (GlobalContext)
		ImGui::SetCurrentContext(GlobalContext);

	return ImGui::ImageButton(name, (ImTextureID)image, size);
}

void rlImGuiImageSize(const Texture* image, int width, int height)
{
	if (!image)
		return;

	if (GlobalContext)
		ImGui::SetCurrentContext(GlobalContext);

	ImGui::Image((ImTextureID)image, ImVec2(float(width), float(height)));
}

void rlImGuiImageSizeV(const Texture* image, Vector2 size)
{
	if (!image)
		return;

	if (GlobalContext)
		ImGui::SetCurrentContext(GlobalContext);

	ImGui::Image((ImTextureID)image, ImVec2(size.x, size.y));
}

void rlImGuiImageRect(const Texture* image, int destWidth, int destHeight, Rectangle sourceRect)
{
	if (!image)
		return;

	if (GlobalContext)
		ImGui::SetCurrentContext(GlobalContext);

	ImVec2 uv0;
	ImVec2 uv1;

	if (sourceRect.width < 0)
	{
		uv0.x = -((float)sourceRect.x / image->width);
		uv1.x = (uv0.x - (float)(fabs(sourceRect.width) / image->width));
	}
	else
	{
		uv0.x = (float)sourceRect.x / image->width;
		uv1.x = uv0.x + (float)(sourceRect.width / image->width);
	}

	if (sourceRect.height < 0)
	{
		uv0.y = -((float)sourceRect.y / image->height);
		uv1.y = (uv0.y - (float)(fabs(sourceRect.height) / image->height));
	}
	else
	{
		uv0.y = (float)sourceRect.y / image->height;
		uv1.y = uv0.y + (float)(sourceRect.height / image->height);
	}

	ImGui::Image((ImTextureID)image, ImVec2(float(destWidth), float(destHeight)), uv0, uv1);
}

void rlImGuiImageRenderTexture(const RenderTexture* Image)
{
	if (Image == nullptr)
	{
		return;
	}
	if (GlobalContext != nullptr)
	{
		ImGui::SetCurrentContext(GlobalContext);
	}

	rlImGuiImageRect(&Image->texture, Image->texture.width, Image->texture.height,
	                 Rectangle{0, 0, float(Image->texture.width), -float(Image->texture.height)});
}

void rlImGuiImageRenderTextureFit(const RenderTexture* Image, const bool bCenter)
{
	if (Image == nullptr)
	{
		return;
	}
	if (GlobalContext != nullptr)
	{
		ImGui::SetCurrentContext(GlobalContext);
	}

	const ImVec2 ContentArea{ImGui::GetContentRegionAvail()};
	float ContentScale{ContentArea.x / static_cast<float>(Image->texture.width)};

	const float Y{static_cast<float>(Image->texture.height) * ContentScale};
	if (Y > ContentArea.y)
	{
		ContentScale = ContentArea.y / static_cast<float>(Image->texture.height);
	}

	const float SizeX{static_cast<float>(Image->texture.width) * ContentScale};
	const float SizeY{static_cast<float>(Image->texture.height) * ContentScale};

	if (bCenter)
	{
		ImGui::SetCursorPosX(0);
		ImGui::SetCursorPosX(ContentArea.x / 2.0f - SizeX / 2.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ContentArea.y / 2.0f - SizeY / 2.0f));
	}

	rlImGuiImageRect(&Image->texture, static_cast<int>(SizeX), static_cast<int>(SizeY),
	                 Rectangle{0, 0,
	                           static_cast<float>(Image->texture.width),
	                           -static_cast<float>(Image->texture.height)});
}

// raw ImGui backend API
bool ImGui_ImplRaylib_Init()
{
	SetupGlobals();
	SetupKeymap();
	SetupBackend();
	return true;
}

void Imgui_ImplRaylib_BuildFontAtlas()
{
	ReloadFonts();
}

void ImGui_ImplRaylib_Shutdown()
{
	ImGuiIO& io = ImGui::GetIO();
	Texture2D* fontTexture = (Texture2D*)io.Fonts->TexID;

	if (fontTexture)
	{
		UnloadTexture(*fontTexture);
		MemFree(fontTexture);
	}

	io.Fonts->TexID = 0;
}

void ImGui_ImplRaylib_NewFrame()
{
	ImGuiNewFrame(GetFrameTime());
}

void ImGui_ImplRaylib_RenderDrawData(ImDrawData* draw_data)
{
	rlDrawRenderBatchActive();
	rlDisableBackfaceCulling();

	for (int l = 0; l < draw_data->CmdListsCount; ++l)
	{
		const ImDrawList* commandList = draw_data->CmdLists[l];

		for (const auto& cmd : commandList->CmdBuffer)
		{
			EnableScissor(cmd.ClipRect.x - draw_data->DisplayPos.x, cmd.ClipRect.y - draw_data->DisplayPos.y,
			              cmd.ClipRect.z - (cmd.ClipRect.x - draw_data->DisplayPos.x),
			              cmd.ClipRect.w - (cmd.ClipRect.y - draw_data->DisplayPos.y));
			if (cmd.UserCallback != nullptr)
			{
				cmd.UserCallback(commandList, &cmd);

				continue;
			}

			ImGuiRenderTriangles(cmd.ElemCount, cmd.IdxOffset, commandList->IdxBuffer, commandList->VtxBuffer,
			                     cmd.TextureId);
			rlDrawRenderBatchActive();
		}
	}

	rlSetTexture(0);
	rlDisableScissorTest();
	rlEnableBackfaceCulling();
}

void HandleGamepadButtonEvent(ImGuiIO& io, GamepadButton button, ImGuiKey key)
{
	if (IsGamepadButtonPressed(0, button))
		io.AddKeyEvent(key, true);
	else if (IsGamepadButtonReleased(0, button))
		io.AddKeyEvent(key, false);
}

void HandleGamepadStickEvent(ImGuiIO& io, GamepadAxis axis, ImGuiKey negKey, ImGuiKey posKey)
{
	constexpr float deadZone = 0.20f;

	float axisValue = GetGamepadAxisMovement(0, axis);

	io.AddKeyAnalogEvent(negKey, axisValue < -deadZone, axisValue < -deadZone ? -axisValue : 0);
	io.AddKeyAnalogEvent(posKey, axisValue > deadZone, axisValue > deadZone ? axisValue : 0);
}

bool ImGui_ImplRaylib_ProcessEvents()
{
	ImGuiIO& io = ImGui::GetIO();

	bool focused = IsWindowFocused();
	if (focused != LastFrameFocused)
		io.AddFocusEvent(focused);
	LastFrameFocused = focused;

	// handle the modifyer key events so that shortcuts work
	bool ctrlDown = rlImGuiIsControlDown();
	if (ctrlDown != LastControlPressed)
		io.AddKeyEvent(ImGuiMod_Ctrl, ctrlDown);
	LastControlPressed = ctrlDown;

	bool shiftDown = rlImGuiIsShiftDown();
	if (shiftDown != LastShiftPressed)
		io.AddKeyEvent(ImGuiMod_Shift, shiftDown);
	LastShiftPressed = shiftDown;

	bool altDown = rlImGuiIsAltDown();
	if (altDown != LastAltPressed)
		io.AddKeyEvent(ImGuiMod_Alt, altDown);
	LastAltPressed = altDown;

	bool superDown = rlImGuiIsSuperDown();
	if (superDown != LastSuperPressed)
		io.AddKeyEvent(ImGuiMod_Super, superDown);
	LastSuperPressed = superDown;

	// get the pressed keys, just walk the keys so we don
	for (int keyId = KEY_NULL; keyId < KeyboardKey::KEY_KP_EQUAL; keyId++)
	{
		if (!IsKeyPressed(keyId))
			continue;
		auto keyItr = RaylibKeyMap.find(KeyboardKey(keyId));
		if (keyItr != RaylibKeyMap.end())
			io.AddKeyEvent(keyItr->second, true);
	}

	// look for any keys that were down last frame and see if they were down and are released
	for (const auto keyItr : RaylibKeyMap)
	{
		if (IsKeyReleased(keyItr.first))
			io.AddKeyEvent(keyItr.second, false);
	}

	if (io.WantCaptureKeyboard)
	{
		// add the text input in order
		unsigned int pressed = GetCharPressed();
		while (pressed != 0)
		{
			io.AddInputCharacter(pressed);
			pressed = GetCharPressed();
		}
	}

	if (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad && IsGamepadAvailable(0))
	{
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_LEFT_FACE_UP, ImGuiKey_GamepadDpadUp);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_LEFT_FACE_RIGHT, ImGuiKey_GamepadDpadRight);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_LEFT_FACE_DOWN, ImGuiKey_GamepadDpadDown);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_LEFT_FACE_LEFT, ImGuiKey_GamepadDpadLeft);

		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_RIGHT_FACE_UP, ImGuiKey_GamepadFaceUp);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, ImGuiKey_GamepadFaceLeft);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, ImGuiKey_GamepadFaceDown);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_RIGHT_FACE_LEFT, ImGuiKey_GamepadFaceRight);

		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_LEFT_TRIGGER_1, ImGuiKey_GamepadL1);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_LEFT_TRIGGER_2, ImGuiKey_GamepadL2);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_RIGHT_TRIGGER_1, ImGuiKey_GamepadR1);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_RIGHT_TRIGGER_2, ImGuiKey_GamepadR2);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_LEFT_THUMB, ImGuiKey_GamepadL3);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_RIGHT_THUMB, ImGuiKey_GamepadR3);

		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_MIDDLE_LEFT, ImGuiKey_GamepadStart);
		HandleGamepadButtonEvent(io, GAMEPAD_BUTTON_MIDDLE_RIGHT, ImGuiKey_GamepadBack);

		// left stick
		HandleGamepadStickEvent(io, GAMEPAD_AXIS_LEFT_X, ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight);
		HandleGamepadStickEvent(io, GAMEPAD_AXIS_LEFT_Y, ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown);

		// right stick
		HandleGamepadStickEvent(io, GAMEPAD_AXIS_RIGHT_X, ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight);
		HandleGamepadStickEvent(io, GAMEPAD_AXIS_RIGHT_Y, ImGuiKey_GamepadRStickUp, ImGuiKey_GamepadRStickDown);
	}

	return true;
}
