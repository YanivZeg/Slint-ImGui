// Copyright © SixtyFPS GmbH <info@slint.dev>
// SPDX-License-Identifier: MIT

#include "scene.h"

#include <cstdlib>
#include <print>
#include <stdlib.h>
#include <cassert>
#include <concepts>

#include <GLES3/gl3.h>
#include <GLES3/gl3platform.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

using std::println;

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

struct SceneTexture
{
    GLuint texture;
    int width;
    int height;
    GLuint fbo;

    SceneTexture(int width, int height) : width(width), height(height)
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
    SceneTexture(const SceneTexture &) = delete;
    SceneTexture &operator=(const SceneTexture &) = delete;
    ~SceneTexture()
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

class ImGuiRendererBase
{
public:
    ImGuiRendererBase(slint::ComponentWeakHandle<App> app) : app_weak(app) { }
    ImGuiRendererBase(ImGuiRendererBase&&) noexcept = default;
    virtual ~ImGuiRendererBase() = default;

    void operator()(slint::RenderingState state, slint::GraphicsAPI)
    {
        switch (state) {
        case slint::RenderingState::RenderingSetup:
            if (auto app = app_weak.lock()) {
                setup();
                setTexture(*app);
                (*app)->window().request_redraw();
            }
            break;
        case slint::RenderingState::BeforeRendering:
            if (auto app = app_weak.lock()) {
                updateTexture(*app);
            }
            break;
        case slint::RenderingState::AfterRendering:
            break;
        case slint::RenderingState::RenderingTeardown:
            teardown();
            break;
        }
    }
protected:

    virtual bool needsUpdate([[maybe_unused]] slint::ComponentHandle<App> &app) = 0;

    virtual void buildScene(slint::ComponentHandle<App> &app) = 0;

    void setup()
    {
        IMGUI_CHECKVERSION();
        ctx_ = ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplOpenGL3_Init("#version 300 es");

        displayed_texture_ = std::make_unique<SceneTexture>(320, 200);
        next_texture_ = std::make_unique<SceneTexture>(320, 200);
    }

    void setTexture(slint::ComponentHandle<App> &app)
    {
        auto texture = render(app);
        (app)->set_texture(texture);
    }

    void updateTexture(slint::ComponentHandle<App> &app)
    {
        if (needsUpdate(app)) {
            setTexture(app);
        }
    }

    slint::Image render(slint::ComponentHandle<App> &app)
    {
        auto width = app->get_requested_texture_width();
        auto height = app->get_requested_texture_height();

        if (next_texture_->width != width || next_texture_->height != height) {
            auto new_texture = std::make_unique<SceneTexture>(width, height);
            std::swap(next_texture_, new_texture);
        }

        next_texture_->with_active_fbo([&]() {
            GLint saved_viewport[4];
            glGetIntegerv(GL_VIEWPORT, saved_viewport);

            glViewport(0, 0, next_texture_->width, next_texture_->height);
            glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui::SetCurrentContext(ctx_);

            ImGuiIO &io = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.DeltaTime = 1.0f / 60.0f;

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            buildScene(app);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
        });

        auto resultTexture = slint::Image::create_from_borrowed_gl_2d_rgba_texture(
                next_texture_->texture,
                { static_cast<uint32_t>(next_texture_->width),
                  static_cast<uint32_t>(next_texture_->height) },
                slint::Image::BorrowedOpenGLTextureOrigin::BottomLeft);

        std::swap(next_texture_, displayed_texture_);

        return resultTexture;
    }

    virtual void teardown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext(ctx_);
        ctx_ = nullptr;
        displayed_texture_.reset();
        next_texture_.reset();
    };

private:
    slint::ComponentWeakHandle<App> app_weak;

    ImGuiContext *ctx_ = nullptr;
    std::unique_ptr<SceneTexture> displayed_texture_ = nullptr;
    std::unique_ptr<SceneTexture> next_texture_ = nullptr;
};

class DemoRenderer : public ImGuiRendererBase
{
public:
    using ImGuiRendererBase::ImGuiRendererBase;

protected:
    virtual bool needsUpdate([[maybe_unused]] slint::ComponentHandle<App> &app) override
    {
        auto new_state = State{
            .red = app->get_selected_red(),
            .green = app->get_selected_green(),
            .blue = app->get_selected_blue(),
            .width = app->get_requested_texture_width(),
            .height = app->get_requested_texture_height()
        };
        if (state_ != new_state) {
            state_ = new_state;
            return true;
        }
        return false;
    }

    virtual void buildScene([[maybe_unused]] slint::ComponentHandle<App> &app) override
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Slint + ImGui", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("Rendered into texture");
            float color[3] = { state_.red, state_.green, state_.blue };
            ImGui::SliderFloat("Red", &color[0], 0.0f, 1.0f);
            ImGui::SliderFloat("Green", &color[1], 0.0f, 1.0f);
            ImGui::SliderFloat("Blue", &color[2], 0.0f, 1.0f);
            ImGui::ColorEdit3("Color", color);
        }
        ImGui::End();
    }

private:
    struct State {
        float red = -1.0f;
        float green = -1.0f;
        float blue = -1.0f;
        int width = -1;
        int height = -1;

        bool operator==(const State &other) const
        {
            return red == other.red && green == other.green && blue == other.blue && width == other.width && height == other.height;
        }
        bool operator!=(const State &other) const
        {
            return !(*this == other);
        }
    };

    State state_;
};

int main()
{
    auto app = App::create();

    if (auto error = app->window().set_rendering_notifier(DemoRenderer(app))) {
        if (*error == slint::SetRenderingNotifierError::Unsupported) {
            println(stderr, "This example requires the use of a GL renderer. Please run with the "
                            "environment variable SLINT_BACKEND=winit-femtovg set.");
        } else {
            println(stderr, "Unknown error calling set_rendering_notifier");
        }
        return EXIT_FAILURE;
    }

    app->run();
    return EXIT_SUCCESS;
}
