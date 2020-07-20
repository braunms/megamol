/*
 * OpenGL_GLFW_Service.cpp
 *
 * Copyright (C) 2019 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "OpenGL_GLFW_RAPI.hpp"

#include <array>
#include <chrono>
#include <vector>

#include "glad.h"
#ifdef _WIN32
#    include <Windows.h>
#    undef min
#    undef max
#    include "glad_wgl.h"
#else
#    include "glad_glx.h"
#endif

#include <GLFW/glfw3.h>
#ifdef _WIN32
#    ifndef USE_EGL
#        define GLFW_EXPOSE_NATIVE_WGL
#        define GLFW_EXPOSE_NATIVE_WIN32
#        include <GLFW/glfw3native.h>
#    endif
#endif

#include "vislib/graphics/FpsCounter.h"
#include "vislib/sys/Log.h"

#include <functional>
#include <iostream>

static std::string get_message_id_name(GLuint id) {
    if (id == 0x0500) {
        return "GL_INVALID_ENUM";
    }
    if (id == 0x0501) {
        return "GL_INVALID_VALUE";
    }
    if (id == 0x0502) {
        return "GL_INVALID_OPERATION";
    }
    if (id == 0x0503) {
        return "GL_STACK_OVERFLOW";
    }
    if (id == 0x0504) {
        return "GL_STACK_UNDERFLOW";
    }
    if (id == 0x0505) {
        return "GL_OUT_OF_MEMORY";
    }
    if (id == 0x0506) {
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    }
    if (id == 0x0507) {
        return "GL_CONTEXT_LOST";
    }
    if (id == 0x8031) {
        return "GL_TABLE_TOO_LARGE";
    }

	return std::to_string(id);
}


static void APIENTRY opengl_debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam) {
    /* Message Sources
        Source enum                      Generated by
        GL_DEBUG_SOURCE_API              Calls to the OpenGL API
        GL_DEBUG_SOURCE_WINDOW_SYSTEM    Calls to a window - system API
        GL_DEBUG_SOURCE_SHADER_COMPILER  A compiler for a shading language
        GL_DEBUG_SOURCE_THIRD_PARTY      An application associated with OpenGL
        GL_DEBUG_SOURCE_APPLICATION      Generated by the user of this application
        GL_DEBUG_SOURCE_OTHER            Some source that isn't one of these
    */
    /* Message Types
        Type enum                          Meaning
        GL_DEBUG_TYPE_ERROR                An error, typically from the API
        GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR  Some behavior marked deprecated has been used
        GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR   Something has invoked undefined behavior
        GL_DEBUG_TYPE_PORTABILITY          Some functionality the user relies upon is not portable
        GL_DEBUG_TYPE_PERFORMANCE          Code has triggered possible performance issues
        GL_DEBUG_TYPE_MARKER               Command stream annotation
        GL_DEBUG_TYPE_PUSH_GROUP           Group pushing
        GL_DEBUG_TYPE_POP_GROUP            foo
        GL_DEBUG_TYPE_OTHER                Some type that isn't one of these
    */
    /* Message Severity
        Severity enum                    Meaning
        GL_DEBUG_SEVERITY_HIGH           All OpenGL Errors, shader compilation / linking errors, or highly
                                         - dangerous undefined behavior 
        GL_DEBUG_SEVERITY_MEDIUM         Major performance warnings, shader compilation / linking
                                         warnings, or the use of deprecated functionality 
        GL_DEBUG_SEVERITY_LOW            Redundant state change
                                         performance warning, or unimportant undefined behavior 
        GL_DEBUG_SEVERITY_NOTIFICATION   Anything that isn't an
                                         error or performance issue.
    */
    if (source == GL_DEBUG_SOURCE_API || source == GL_DEBUG_SOURCE_SHADER_COMPILER)
        if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR ||
            type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
            if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
                std::cout << "OpenGL Error: " << message << " (" << get_message_id_name(id) << ")" << std::endl;
}

namespace megamol {
namespace frontend {

namespace {
// the idea is that we only borrow, but _do not_ manipulate shared data somebody else gave us
struct SharedData {
    GLFWwindow* borrowed_glfwContextWindowPtr{nullptr}; //
    // The SharedData idea is a bit broken here: on one hand, each window needs its own handle and GL context, on the
    // other hand a GL context can share its resources with others when its glfwWindow* handle gets passed to creation
    // of another Window/OpenGL context. So this handle is private, but can be used by other RAPI instances too.
};

void initSharedContext(SharedData& context) {
    context.borrowed_glfwContextWindowPtr = nullptr; // stays null since nobody shared his GL context with us
}

// indirection to not spam header file with GLFW inclucde
#define that static_cast<OpenGL_GLFW_Service*>(::glfwGetWindowUserPointer(wnd))

// keyboard events
void outer_glfw_onKey_func(GLFWwindow* wnd, int key, int scancode, int action, int mods) {
    that->glfw_onKey_func(key, scancode, action, mods);
}
void outer_glfw_onChar_func(GLFWwindow* wnd, unsigned int codepoint) { that->glfw_onChar_func(codepoint); }

// mouse events
void outer_glfw_onMouseButton_func(GLFWwindow* wnd, int button, int action, int mods) {
    that->glfw_onMouseButton_func(button, action, mods);
}
void outer_glfw_onMouseCursorPosition_func(GLFWwindow* wnd, double xpos, double ypos) {
	// cursor (x,y) position in screen coordinates relative to upper-left corner
    that->glfw_onMouseCursorPosition_func(xpos, ypos);
}
void outer_glfw_onMouseCursorEnter_func(GLFWwindow* wnd, int entered) {
    that->glfw_onMouseCursorEnter_func(entered == GLFW_TRUE);
}
void outer_glfw_onMouseScroll_func(GLFWwindow* wnd, double xoffset, double yoffset) {
    that->glfw_onMouseScroll_func(xoffset, yoffset);
}

// window events
void outer_glfw_onWindowSize_func(GLFWwindow* wnd, int width /* in screen coordinates of the window */, int height) {
    that->glfw_onWindowSize_func(width, height);
}
void outer_glfw_onWindowFocus_func(GLFWwindow* wnd, int focused) { that->glfw_onWindowFocus_func(focused == GLFW_TRUE); }
void outer_glfw_onWindowShouldClose_func(GLFWwindow* wnd) { that->glfw_onWindowShouldClose_func(true); }
void outer_glfw_onWindowIconified_func(GLFWwindow* wnd, int iconified) { that->glfw_onWindowIconified_func(iconified == GLFW_TRUE); }
void outer_glfw_onWindowContentScale_func(GLFWwindow* wnd, float xscale, float yscale) {
    that->glfw_onWindowContentScale_func(xscale, yscale);
}
// void outer_glfw_WindowPosition_func(GLFWwindow* wnd, int xpos, int ypos) { that->glfw_WindowPosition_func(xpos, ypos); }
void outer_glfw_onPathDrop_func(GLFWwindow* wnd, int path_count, const char* paths[]) {
    that->glfw_onPathDrop_func(path_count, paths);
}

// framebuffer events
void outer_glfw_onFramebufferSize_func(GLFWwindow* wnd, int widthpx, int heightpx) {
    that->glfw_onFramebufferSize_func(widthpx, heightpx);
}

} // namespace

// helpers to simplify data access
#define m_data (*m_pimpl)
#define m_sharedData ((m_data.sharedDataPtr) ? (*m_data.sharedDataPtr) : (m_data.sharedData))
#define m_glfwWindowPtr (m_data.glfwContextWindowPtr)

void OpenGL_GLFW_Service::OpenGL_Context::activate() const {
    if (!ptr) return;

	glfwMakeContextCurrent(static_cast<GLFWwindow*>(ptr));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);
}

void OpenGL_GLFW_Service::OpenGL_Context::close() const {
    if (!ptr) return;

	glfwMakeContextCurrent(nullptr);
}


struct OpenGL_GLFW_Service::PimplData {
    SharedData sharedData;              // personal data we share with other RAPIs
    SharedData* sharedDataPtr{nullptr}; // if we get shared data from another OGL RAPI object, we access it using this
                                        // ptr and leave our own shared data un-initialized

    GLFWwindow* glfwContextWindowPtr{nullptr}; // _my own_ gl context!
    OpenGL_GLFW_Service::Config initialConfig;    // keep copy of user-provided config

    std::string fullWindowTitle;

    int currentWidth = 0, currentHeight = 0;

    // TODO: move into 'FrameStatisticsCalculator'
    vislib::graphics::FpsCounter fpsCntr;
    float fps = 0.0f;
    std::array<float, 20> fpsList = {0.0f};
    bool showFpsInTitle = true;
    std::chrono::system_clock::time_point fpsSyncTime;
    GLuint fragmentQuery;
    GLuint primsQuery;
    bool showFragmentsInTitle;
    bool showPrimsInTitle;
    // glGenQueries(1, &m_data.fragmentQuery);
    // glGenQueries(1, &m_data.primsQuery);
    // m_data.fpsSyncTime = std::chrono::system_clock::now();
};

OpenGL_GLFW_Service::~OpenGL_GLFW_Service() {
    if (m_pimpl) this->close(); // cleans up pimpl
}

bool OpenGL_GLFW_Service::init(void* configPtr) {
    if (configPtr == nullptr) return false;

    return init(*static_cast<Config*>(configPtr));
}

bool OpenGL_GLFW_Service::init(const Config& config) {
    if (m_pimpl) return false; // this API object is already initialized

    // TODO: check config for sanity?
    // if(!sane(config))
    //	return false

    m_pimpl = std::unique_ptr<PimplData, std::function<void(PimplData*)>>(new PimplData, [](PimplData* ptr) { delete ptr; });
    m_data.initialConfig = config;
    // from here on, access pimpl data using "m_data.member", as in m_pimpl->member minus the  void-ptr casting

    // init (shared) context data for this object or use provided
    if (config.sharedContextPtr) {
        m_data.sharedDataPtr = reinterpret_cast<SharedData*>(config.sharedContextPtr);
    } else {
        initSharedContext(m_data.sharedData);
    }
    // from here on, use m_sharedData to access reference to SharedData for RAPI objects; the owner will clean it up
    // correctly
    if (m_data.sharedDataPtr) {
        // glfw already initialized by other render api
    } else {
        const bool success_glfw = glfwInit();
        if (!success_glfw)
			return false; // glfw had error on init; abort
    }

    // init glfw window and OpenGL Context
    ::glfwWindowHint(GLFW_ALPHA_BITS, 8);
    ::glfwWindowHint(GLFW_DECORATED,
        (config.windowPlacement.fullScreen) ? (GL_FALSE) : (config.windowPlacement.noDec ? GL_FALSE : GL_TRUE));
    ::glfwWindowHint(GLFW_VISIBLE, GL_FALSE); // initially invisible

    int monCnt = 0;
    GLFWmonitor** monitors = ::glfwGetMonitors(&monCnt); // primary monitor is first in list
    if (!monitors) return false;                         // no monitor found; abort

    // in fullscreen, use last available monitor as to not block primary monitor, where the user may have important
    // stuff he wants to look at
    int monitorNr =
        (config.windowPlacement.fullScreen)
            ? std::max<int>(0, std::min<int>(monCnt - 1,
                                   config.windowPlacement.mon)) // if fullscreen, use last or user-provided monitor
            : (0);                                              // if windowed, use primary monitor
    GLFWmonitor* selectedMonitor = monitors[monitorNr];
    if (!selectedMonitor) return false; // selected monitor not valid for some reason; abort

    const GLFWvidmode* mode = ::glfwGetVideoMode(selectedMonitor);
    if (!mode) return false; // error while receiving monitor mode; abort

    // window size for windowed mode
    if (!config.windowPlacement.fullScreen) {
        if (config.windowPlacement.size && (config.windowPlacement.w > 0) && (config.windowPlacement.h > 0)) {
            m_data.currentWidth = config.windowPlacement.w;
            m_data.currentHeight = config.windowPlacement.h;
        } else {
            vislib::sys::Log::DefaultLog.WriteWarn("No useful window size given. Making one up");
            // no useful window size given, derive one from monitor resolution
            m_data.currentWidth = mode->width * 3 / 4;
            m_data.currentHeight = mode->height * 3 / 4;
        }
    }

    // options for fullscreen mode
    if (config.windowPlacement.fullScreen) {
        if (config.windowPlacement.pos)
            vislib::sys::Log::DefaultLog.WriteWarn("Ignoring window placement position when requesting fullscreen.");

        if (config.windowPlacement.size &&
            ((config.windowPlacement.w != mode->width) || (config.windowPlacement.h != mode->height)))
            vislib::sys::Log::DefaultLog.WriteWarn("Changing screen resolution is currently not supported.");

        if (config.windowPlacement.noDec)
            vislib::sys::Log::DefaultLog.WriteWarn("Ignoring no-decorations setting when requesting fullscreen.");

        /* note we do not use a real fullscrene mode, since then we would have focus-iconify problems */
        m_data.currentWidth = mode->width;
        m_data.currentHeight = mode->height;

        ::glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        ::glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        ::glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        ::glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        // this only works since we are NOT setting a monitor
        ::glfwWindowHint(GLFW_FLOATING, GL_TRUE); // floating above other windows / top most

        // will place 'fullscreen' window at origin of monitor
        int mon_x, mon_y;
        ::glfwGetMonitorPos(selectedMonitor, &mon_x, &mon_y);
        m_data.initialConfig.windowPlacement.x = mon_x;
        m_data.initialConfig.windowPlacement.y = mon_y;
    }

    // TODO: OpenGL context hints? version? core profile?
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_data.initialConfig.versionMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_data.initialConfig.versionMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    m_glfwWindowPtr = ::glfwCreateWindow(m_data.currentWidth, m_data.currentHeight,
        m_data.initialConfig.windowTitlePrefix.c_str(), nullptr, m_sharedData.borrowed_glfwContextWindowPtr);
    m_opengl_context_impl.ptr = m_glfwWindowPtr;
    m_opengl_context = &m_opengl_context_impl;

    if (!m_glfwWindowPtr) {
        vislib::sys::Log::DefaultLog.WriteInfo("OpenGL_GLFW_Service: Failed to create GLFW window.");
        return false;
    }
    vislib::sys::Log::DefaultLog.WriteInfo(
        "OpenGL_GLFW_Service: Create window with size w: %d, h: %d\n", m_data.currentWidth, m_data.currentHeight);

    ::glfwMakeContextCurrent(m_glfwWindowPtr);

    if (config.windowPlacement.pos ||
        config.windowPlacement
            .fullScreen) // note the m_data window position got overwritten with monitor position for fullscreen mode
        ::glfwSetWindowPos(
            m_glfwWindowPtr, m_data.initialConfig.windowPlacement.x, m_data.initialConfig.windowPlacement.y);

    // TODO: when do we need this?
    // if (config.windowPlacement.fullScreen ||
    //     config.windowPlacement.noDec) {
    //     ::glfwSetInputMode(m_glfwWindowPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // }

    if(gladLoadGL() == 0)
		vislib::sys::Log::DefaultLog.WriteInfo("OpenGL_GLFW_Service: failed to load GL via glad\n");
#ifdef _WIN32
    gladLoadWGL(wglGetCurrentDC());
#else
    Display* display = XOpenDisplay(NULL);
    gladLoadGLX(display, DefaultScreen(display));
    XCloseDisplay(display);
#endif

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(opengl_debug_message_callback, nullptr);

    ::glfwSetWindowUserPointer(m_glfwWindowPtr, this); // this is ok, as long as no one derives from this RAPI

    // set callbacks
    ::glfwSetKeyCallback(m_glfwWindowPtr, &outer_glfw_onKey_func);
    ::glfwSetCharCallback(m_glfwWindowPtr, &outer_glfw_onChar_func);
    // this->m_keyboardEvents; // ignore because no interaction happened yet

    // set callbacks
    ::glfwSetMouseButtonCallback(m_glfwWindowPtr, &outer_glfw_onMouseButton_func);
    ::glfwSetCursorPosCallback(m_glfwWindowPtr, &outer_glfw_onMouseCursorPosition_func);
    ::glfwSetCursorEnterCallback(m_glfwWindowPtr, &outer_glfw_onMouseCursorEnter_func);
    ::glfwSetScrollCallback(m_glfwWindowPtr, &outer_glfw_onMouseScroll_func);
    // set current state for mouse events
    // this->m_mouseEvents.previous_state.buttons; // ignore because no interaction yet
    this->m_mouseEvents.previous_state.entered = glfwGetWindowAttrib(m_glfwWindowPtr, GLFW_HOVERED);
    ::glfwGetCursorPos(m_glfwWindowPtr, &this->m_mouseEvents.previous_state.x_cursor_position,
        &this->m_mouseEvents.previous_state.y_cursor_position);
    this->m_mouseEvents.previous_state.x_scroll = 0.0;
    this->m_mouseEvents.previous_state.y_scroll = 0.0;

    // set callbacks
    ::glfwSetWindowSizeCallback(m_glfwWindowPtr, &outer_glfw_onWindowSize_func);
    ::glfwSetWindowFocusCallback(m_glfwWindowPtr, &outer_glfw_onWindowFocus_func);
    ::glfwSetWindowCloseCallback(m_glfwWindowPtr, &outer_glfw_onWindowShouldClose_func);
    ::glfwSetWindowIconifyCallback(m_glfwWindowPtr, &outer_glfw_onWindowIconified_func);
    ::glfwSetWindowContentScaleCallback(m_glfwWindowPtr, &outer_glfw_onWindowContentScale_func);
    ::glfwSetDropCallback(m_glfwWindowPtr, &outer_glfw_onPathDrop_func);
    // set current window state
    glfwGetWindowSize(
        m_glfwWindowPtr, &this->m_windowEvents.previous_state.width, &this->m_windowEvents.previous_state.height);
    this->m_windowEvents.previous_state.is_focused = (GLFW_TRUE == glfwGetWindowAttrib(m_glfwWindowPtr, GLFW_FOCUSED));
    this->m_windowEvents.previous_state.is_iconified =
        (GLFW_TRUE == glfwGetWindowAttrib(m_glfwWindowPtr, GLFW_ICONIFIED));
    this->m_windowEvents.previous_state.should_close = (GLFW_TRUE == glfwWindowShouldClose(m_glfwWindowPtr));
    glfwGetWindowContentScale(m_glfwWindowPtr, &this->m_windowEvents.previous_state.x_contentscale,
        &this->m_windowEvents.previous_state.y_contentscale);

    // set callbacks
    ::glfwSetFramebufferSizeCallback(m_glfwWindowPtr, &outer_glfw_onFramebufferSize_func);
    // set current framebuffer state
    glfwGetFramebufferSize(m_glfwWindowPtr, &this->m_framebufferEvents.previous_state.width,
        &this->m_framebufferEvents.previous_state.height);

    // TODO: implement OpenGL Debug
    // if (config.enableKHRDebug)
    //    megamol::core::utility::KHR::startDebug();

    if (config.enableVsync) ::glfwSwapInterval(0);

    ::glfwShowWindow(m_glfwWindowPtr);
    ::glfwMakeContextCurrent(nullptr);

	// make the events and resources managed/provided by this RAPI available to the outside world
	m_renderResourceReferences = {
		{"KeyboardEvents", m_keyboardEvents},
		{"MouseEvents", m_mouseEvents},
		{"WindowEvents", m_windowEvents},
		{"FramebufferEvents", m_framebufferEvents},
		{"IOpenGL_Context", *m_opengl_context}
	};

    return true;
}

void OpenGL_GLFW_Service::close() {
    if (!m_pimpl) // this API object is not initialized
        return;

    const bool close_glfw = (m_data.sharedDataPtr == nullptr);

    ::glfwMakeContextCurrent(m_glfwWindowPtr);

    // GL context and destruction of all other things happens in destructors of pimpl data members
    if (m_data.glfwContextWindowPtr) ::glfwDestroyWindow(m_data.glfwContextWindowPtr);
    m_data.sharedDataPtr = nullptr;
    m_data.glfwContextWindowPtr = nullptr;
    this->m_pimpl.release();

    ::glfwMakeContextCurrent(nullptr);

    if (close_glfw) {
        glfwTerminate();
    }
}
	
void OpenGL_GLFW_Service::updateResources() {
    if (m_glfwWindowPtr == nullptr) return;

	this->clearResources();

    // poll events for all GLFW windows shared by this context. this also issues the callbacks.
    // note at this point there is no GL context active.
	// event struct get filled via GLFW callbacks when new input events come in during glfwPollEvents()
    if (m_data.sharedDataPtr == nullptr) // nobody shared context with us, so we must be primary context provider
		::glfwPollEvents(); // may only be called from main thread
    // from GLFW Docs:
    // Do not assume that callbacks will only be called through glfwPollEvents().
    // While it is necessary to process events in the event queue,
    // some window systems will send some events directly to the application,
    // which in turn causes callbacks to be called outside of regular event processing.
}

void OpenGL_GLFW_Service::digestChangedResources() {
    if (m_glfwWindowPtr == nullptr) return;

    if (this->m_windowEvents.should_close_events.size() && this->m_windowEvents.should_close_events.back())
        this->setShutdown(true); // cleanup of this RAPI and dependent GL stuff is triggered via this shutdown hint
}

void OpenGL_GLFW_Service::preGraphRender() {
    if (m_glfwWindowPtr == nullptr) return;

    // start frame timer

    // rendering via MegaMol View is called after this function finishes
    // in the end this calls the equivalent of ::mmcRenderView(hView, &renderContext)
    // which leads to view.Render()
}

void OpenGL_GLFW_Service::postGraphRender() {
    if (m_glfwWindowPtr == nullptr) return;

    // end frame timer
    // update window name

	// TODO: kill UI layers or make sure they make sense
    // m_data.uiLayers.OnDraw();

    ::glfwSwapBuffers(m_glfwWindowPtr);
    ::glfwMakeContextCurrent(nullptr);
}

void OpenGL_GLFW_Service::clearResources() {
	m_keyboardEvents.clear();
	m_mouseEvents.clear();
	m_windowEvents.clear();
	m_framebufferEvents.clear();
}

const std::vector<ModuleResource>& OpenGL_GLFW_Service::getModuleResources() const {
	return m_renderResourceReferences;
}

const void* OpenGL_GLFW_Service::getSharedDataPtr() const { return &m_sharedData; }

void OpenGL_GLFW_Service::updateWindowTitle() {}

void OpenGL_GLFW_Service::glfw_onKey_func(const int key, const int scancode, const int action, const int mods) {

    input_events::Key key_ = static_cast<input_events::Key>(key);
    input_events::KeyAction action_(input_events::KeyAction::RELEASE);
    switch (action) {
    case GLFW_PRESS:
        action_ = input_events::KeyAction::PRESS;
        break;
    case GLFW_REPEAT:
        action_ = input_events::KeyAction::REPEAT;
        break;
    case GLFW_RELEASE:
        action_ = input_events::KeyAction::RELEASE;
        break;
    }

    input_events::Modifiers mods_;
    if ((mods & GLFW_MOD_SHIFT) == GLFW_MOD_SHIFT) mods_ |= input_events::Modifier::SHIFT;
    if ((mods & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL) mods_ |= input_events::Modifier::CTRL;
    if ((mods & GLFW_MOD_ALT) == GLFW_MOD_ALT) mods_ |= input_events::Modifier::ALT;

    this->m_keyboardEvents.key_events.emplace_back(std::make_tuple(key_, action_, mods_));
    // m_data.uiLayers.OnKey(key, action, mods);
    //this->ui_events.onKey_list.emplace_back(std::make_tuple(key_, action_, mods_));
}

void OpenGL_GLFW_Service::glfw_onChar_func(const unsigned int codepoint) {
    //::glfwMakeContextCurrent(m_glfwWindowPtr);
    // m_data.uiLayers.OnChar(charcode);
    //this->ui_events.onChar_list.emplace_back(charcode);
    this->m_keyboardEvents.codepoint_events.emplace_back(codepoint);
}

void OpenGL_GLFW_Service::glfw_onMouseCursorPosition_func(const double xpos, const double ypos) {

    this->m_mouseEvents.position_events.emplace_back(std::make_tuple(xpos, ypos));
    //::glfwMakeContextCurrent(m_glfwWindowPtr);
    // if (m_data.mouseCapture) {
    //    m_data.mouseCapture->OnMouseMove(x, y);
    //} else {
    //    m_data.uiLayers.OnMouseMove(x, y);
    //}
    //this->ui_events.onMouseMove_list.emplace_back(std::make_tuple(xpos, ypos));
}

void OpenGL_GLFW_Service::glfw_onMouseButton_func(const int button, const int action, const int mods) {
    input_events::MouseButton btn = static_cast<input_events::MouseButton>(button);
    input_events::MouseButtonAction btnaction =
        (action == GLFW_PRESS) ? input_events::MouseButtonAction::PRESS : input_events::MouseButtonAction::RELEASE;

    input_events::Modifiers btnmods;
    if ((mods & GLFW_MOD_SHIFT) == GLFW_MOD_SHIFT) btnmods |= input_events::Modifier::SHIFT;
    if ((mods & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL) btnmods |= input_events::Modifier::CTRL;
    if ((mods & GLFW_MOD_ALT) == GLFW_MOD_ALT) btnmods |= input_events::Modifier::ALT;

    this->m_mouseEvents.buttons_events.emplace_back(std::make_tuple(btn, btnaction, btnmods));

    //this->ui_events.onMouseButton_list.emplace_back(std::make_tuple(btn, btnaction, mods));

    // if (m_data.mouseCapture) {
    //    m_data.mouseCapture->OnMouseButton(btn, action, mods);
    //} else {
    //    if (m_data.uiLayers.OnMouseButton(btn, action, mods))
    //        if (action == frontend::MouseButtonAction::PRESS)
    //            m_data.mouseCapture = m_data.uiLayers.lastEventCaptureUILayer();
    //}

    // if (m_data.mouseCapture) {
    //    bool anyPressed = false;
    //    for (int mbi = GLFW_MOUSE_BUTTON_1; mbi <= GLFW_MOUSE_BUTTON_LAST; ++mbi) {
    //        if (::glfwGetMouseButton(m_glfwWindowPtr, mbi) == GLFW_PRESS) {
    //            anyPressed = true;
    //            break;
    //        }
    //    }
    //    if (!anyPressed) {
    //        m_data.mouseCapture.reset();
    //        double x, y;
    //        ::glfwGetCursorPos(m_glfwWindowPtr, &x, &y);
    //        glfw_onMouseMove_func(x, y); // to inform all of the new location
    //    }
    //}
}

void OpenGL_GLFW_Service::glfw_onMouseScroll_func(const double xoffset, const double yoffset) {

	this->m_mouseEvents.scroll_events.emplace_back(std::make_tuple(xoffset, yoffset));

    //this->ui_events.onMouseWheel_list.emplace_back(std::make_tuple(xoffset, yoffset));
    // if (m_data.mouseCapture) {
    //    m_data.mouseCapture->OnMouseScroll(xoffset, yoffset);
    //} else {
    //    m_data.uiLayers.OnMouseScroll(xoffset, yoffset);
    //}
}

void OpenGL_GLFW_Service::glfw_onMouseCursorEnter_func(const bool entered) {
	this->m_mouseEvents.enter_events.emplace_back(entered);
}

void OpenGL_GLFW_Service::glfw_onFramebufferSize_func(const int widthpx, const int heightpx) {
    this->m_framebufferEvents.size_events.emplace_back(input_events::FramebufferState{widthpx, heightpx});
}

void OpenGL_GLFW_Service::glfw_onWindowSize_func(const int width, const int height) { // in screen coordinates, of the window
    this->m_windowEvents.size_events.emplace_back(std::tuple(width, height));
}

void OpenGL_GLFW_Service::glfw_onWindowFocus_func(const bool focused) {
	this->m_windowEvents.is_focused_events.emplace_back(focused);
}

void OpenGL_GLFW_Service::glfw_onWindowShouldClose_func(const bool shouldclose) {
    this->m_windowEvents.should_close_events.emplace_back(shouldclose);
}

void OpenGL_GLFW_Service::glfw_onWindowIconified_func(const bool iconified) {
    this->m_windowEvents.is_iconified_events.emplace_back(iconified);
}

void OpenGL_GLFW_Service::glfw_onWindowContentScale_func(const float xscale, const float yscale) {
    this->m_windowEvents.content_scale_events.emplace_back(std::tuple(xscale, yscale));
}

void OpenGL_GLFW_Service::glfw_onPathDrop_func(const int path_count, const char* paths[]) {
    std::vector<std::string> paths_;
    paths_.reserve(path_count);

    for (int i = 0; i < path_count; i++)
		paths_.emplace_back(std::string(paths[i]));

    this->m_windowEvents.dropped_path_events.push_back(paths_);
}

} // namespace frontend
} // namespace megamol
