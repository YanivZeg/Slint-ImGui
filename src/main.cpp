// Copyright © SixtyFPS GmbH <info@slint.dev>
// SPDX-License-Identifier: MIT

#include "scene.h"

#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <cassert>
#include <concepts>

#include <GLES3/gl3.h>
#include <GLES3/gl3platform.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#define DEFINE_SCOPED_BINDING(StructName, ParamName, BindingFn, TargetName)                        \
    struct StructName                                                                              \
    {                                                                                              \
        GLuint saved_value = {};                                                                   \
        StructName() = delete;                                                                     \
        StructName(const StructName &) = delete;                                                   \
        StructName &operator=(const StructName &) = delete;                                        \
        StructName(GLuint new_value)                                                               \
        {                                                                                          \
            glGetIntegerv(ParamName, (GLint *)&saved_value);                                       \
            BindingFn(TargetName, new_value);                                                      \
        }                                                                                          \
        ~StructName()                                                                              \
        {                                                                                          \
            BindingFn(TargetName, saved_value);                                                    \
        }                                                                                          \
    }

DEFINE_SCOPED_BINDING(ScopedTextureBinding, GL_TEXTURE_BINDING_2D, glBindTexture, GL_TEXTURE_2D);
DEFINE_SCOPED_BINDING(ScopedFrameBufferBinding, GL_DRAW_FRAMEBUFFER_BINDING, glBindFramebuffer,
                      GL_DRAW_FRAMEBUFFER);

struct DemoTexture
{
    GLuint texture;
    int width;
    int height;
    GLuint fbo;

    DemoTexture(int width, int height) : width(width), height(height)
    {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &texture);

        ScopedTextureBinding activeTexture(texture);

        GLint old_unpack_alignment;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_unpack_alignment);
        GLint old_unpack_row_length;
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &old_unpack_row_length);
        GLint old_unpack_skip_pixels;
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &old_unpack_skip_pixels);
        GLint old_unpack_skip_rows;
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &old_unpack_skip_rows);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);

        ScopedFrameBufferBinding activeFBO(fbo);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        glPixelStorei(GL_UNPACK_ALIGNMENT, old_unpack_alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, old_unpack_row_length);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, old_unpack_skip_pixels);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, old_unpack_skip_rows);
    }
    DemoTexture(const DemoTexture &) = delete;
    DemoTexture &operator=(const DemoTexture &) = delete;
    ~DemoTexture()
    {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
    }

    template<std::invocable<> Callback>
    void with_active_fbo(Callback callback)
    {
        ScopedFrameBufferBinding activeFBO(fbo);
        callback();
    }
};

class ImGuiState {
public:
    bool update(float red, float green, float blue, int width, int height)
    {
        const bool changed = (prev_red_ != red) || (prev_green_ != green) || (prev_blue_ != blue)
                || (prev_width_ != width) || (prev_height_ != height);

        if (changed) {
            prev_red_ = red;
            prev_green_ = green;
            prev_blue_ = blue;
            prev_width_ = width;
            prev_height_ = height;
        }

        return changed;
    }

private:
    float prev_red_ = -1.0f;
    float prev_green_ = -1.0f;
    float prev_blue_ = -1.0f;
    int prev_width_ = -1;
    int prev_height_ = -1;
};

class DemoRenderer
{
public:
    DemoRenderer(slint::ComponentWeakHandle<App> app) : app_weak(app) { }

    void operator()(slint::RenderingState state, slint::GraphicsAPI)
    {
        switch (state) {
        case slint::RenderingState::RenderingSetup:
            if (auto app = app_weak.lock()) {
                setup();
                auto width = (*app)->get_requested_texture_width();
                auto height = (*app)->get_requested_texture_height();
                auto texture = render(0, 0, 0, width, height);
                (*app)->set_texture(texture);
                (*app)->window().request_redraw();
            }
            break;
        case slint::RenderingState::BeforeRendering:
            updateTexture();
            break;
        case slint::RenderingState::AfterRendering:
            break;
        case slint::RenderingState::RenderingTeardown:
            teardown();
            break;
        }
    }

private:
    void setup()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplOpenGL3_Init("#version 300 es");

        displayed_texture = std::make_unique<DemoTexture>(320, 200);
        next_texture = std::make_unique<DemoTexture>(320, 200);
    }

    void updateTexture()
    {
        if (auto app = app_weak.lock()) {
            auto red = (*app)->get_selected_red();
            auto green = (*app)->get_selected_green();
            auto blue = (*app)->get_selected_blue();
            auto width = (*app)->get_requested_texture_width();
            auto height = (*app)->get_requested_texture_height();
            if (imgui_state_.update(red, green, blue, width, height)) {
                auto texture = render(red, green, blue, width, height);
                (*app)->set_texture(texture);
            }
        }
    }

    slint::Image render(float red, float green, float blue, int width, int height)
    {
        if (next_texture->width != width || next_texture->height != height) {
            auto new_texture = std::make_unique<DemoTexture>(width, height);
            std::swap(next_texture, new_texture);
        }

        next_texture->with_active_fbo([&]() {
            GLint saved_viewport[4];
            glGetIntegerv(GL_VIEWPORT, saved_viewport);

            glViewport(0, 0, next_texture->width, next_texture->height);
            glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGuiIO &io = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.DeltaTime = 1.0f / 60.0f;

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Slint + ImGui", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::Text("Rendered into texture");
                float color[3] = { red, green, blue };
                ImGui::SliderFloat("Red", &color[0], 0.0f, 1.0f);
                ImGui::SliderFloat("Green", &color[1], 0.0f, 1.0f);
                ImGui::SliderFloat("Blue", &color[2], 0.0f, 1.0f);
                ImGui::ColorEdit3("Color", color);
            }
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
        });

        auto resultTexture = slint::Image::create_from_borrowed_gl_2d_rgba_texture(
                next_texture->texture,
                { static_cast<uint32_t>(next_texture->width),
                  static_cast<uint32_t>(next_texture->height) },
                slint::Image::BorrowedOpenGLTextureOrigin::BottomLeft);

        std::swap(next_texture, displayed_texture);

        return resultTexture;
    }

    void teardown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
    }

    slint::ComponentWeakHandle<App> app_weak;
    ImGuiState imgui_state_;
    std::unique_ptr<DemoTexture> displayed_texture;
    std::unique_ptr<DemoTexture> next_texture;
};

int main()
{
    auto app = App::create();

    if (auto error = app->window().set_rendering_notifier(DemoRenderer(app))) {
        if (*error == slint::SetRenderingNotifierError::Unsupported) {
            fprintf(stderr,
                    "This example requires the use of a GL renderer. Please run with the "
                    "environment variable SLINT_BACKEND=winit-femtovg set.\n");
        } else {
            fprintf(stderr, "Unknown error calling set_rendering_notifier\n");
        }
        return EXIT_FAILURE;
    }

    app->run();
    return EXIT_SUCCESS;
}
