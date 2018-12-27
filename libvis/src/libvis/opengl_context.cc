// Copyright 2018 ETH Zürich, Thomas Schöps
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "libvis/opengl_context.h"

#include <GL/glew.h>

#if defined(WIN32) || defined(_Windows) || defined(_WINDOWS) || \
    defined(_WIN32) || defined(__WIN32__)

#   include <windows.h>
//#   include <strsafe.h>
#else //linux

#   include <GL/glx.h>
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#endif //_WIN32 & linux

#include <glog/logging.h>

namespace vis {
//#if 0
#if defined(WIN32) || defined(_Windows) || defined(_WINDOWS) || \
    defined(_WIN32) || defined(__WIN32__)

struct OpenGLContextImpl {
//   Display* display;
//   GLXDrawable drawable;
//   GLXContext context;
  HDC displayHdc; //display & drawable to one HDC
  HGLRC context;
  
  bool needs_glew_initialization;
};

//DEPRECATED temporarialy:
//int XErrorHandler(Display* dsp, XErrorEvent* error) {

void CheckLastError() {
    //https://docs.microsoft.com/zh-cn/windows/desktop/Debug/retrieving-the-last-error-code
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);
    LOG(FATAL) << "LastError:\n" << lpMsgBuf;

}

#else //linux

struct OpenGLContextImpl {
  Display* display;
  GLXDrawable drawable;
  GLXContext context;
  bool needs_glew_initialization;
};


int XErrorHandler(Display* dsp, XErrorEvent* error) {
  constexpr int kBufferSize = 512;
  char error_string[kBufferSize];
  XGetErrorText(dsp, error->error_code, error_string, kBufferSize);

  LOG(FATAL) << "X Error:\n" << error_string;
  return 0;
}
#endif //_WIN32 & linux


OpenGLContext::OpenGLContext() { }

OpenGLContext::OpenGLContext(OpenGLContext&& other) { impl.swap(other.impl); }

OpenGLContext& OpenGLContext::operator=(OpenGLContext&& other) {
  impl.swap(other.impl);
  return *this;
}

OpenGLContext::~OpenGLContext() {
  Deinitialize();
}

#if defined(WIN32) || defined(_Windows) || defined(_WINDOWS) || \
    defined(_WIN32) || defined(__WIN32__)

bool OpenGLContext::InitializeWindowless(OpenGLContext* sharing_context) {
  CHECK(!impl);
  impl.reset(new OpenGLContextImpl());

  PIXELFORMATDESCRIPTOR pfd;
  ZeroMemory(&pfd, sizeof(pfd)); // set the pixel format for the DC
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 24;
  pfd.iLayerType = PFD_MAIN_PLANE;

  //HWND hwin = GetActiveWindow();
  //HDC hdc_active = GetDC(hwin);
  HDC hdc_active = wglGetCurrentDC();
  int format = ChoosePixelFormat(hdc_active, &pfd);
  //If the function fails, the return value is zero.To get extended error information, call GetLastError.
  if (!format) {
      LOG(FATAL) << "ChoosePixelFormat() No appropriate visual found.";
      CheckLastError();
  }
  bool res = SetPixelFormat(hdc_active, format, &pfd);
  if (!res) {
      LOG(FATAL) << "SetPixelFormat() failed.";
      CheckLastError();
  }

  HGLRC hrc = wglCreateContext(hdc_active);
  if (nullptr == hrc) {
      LOG(FATAL) << "wglCreateContext() failed.";
      CheckLastError();
  }

  impl->displayHdc = hdc_active;
  impl->context = hrc;
  impl->needs_glew_initialization = true;
  return true;
}//InitializeWindowless

void OpenGLContext::Deinitialize() {
  if (!impl || !impl->context) {
    return;
  }

  wglMakeCurrent(nullptr, nullptr);
  wglDeleteContext(impl->context);

  impl->displayHdc = nullptr;
  impl->context = nullptr;

  impl.reset();
}//Deinitialize

void OpenGLContext::AttachToCurrent() {
  impl.reset(new OpenGLContextImpl());
  
  //impl->display = glXGetCurrentDisplay();
  //impl->drawable = glXGetCurrentDrawable();
  //impl->context = glXGetCurrentContext();
  impl->displayHdc = wglGetCurrentDC();
  impl->context = wglGetCurrentContext();

  impl->needs_glew_initialization = false;  // TODO: This is not clear.
}//AttachToCurrent

void OpenGLContext::Detach() {
  impl.reset();
}//Detach

OpenGLContext SwitchOpenGLContext(const OpenGLContext& context) {
  OpenGLContext current_context;
  current_context.AttachToCurrent();
  if (!current_context.impl->displayHdc) {
    // We need a display, otherwise glXMakeCurrent() will segfault.
    current_context.impl->displayHdc = context.impl->displayHdc;
  }

  bool res = wglMakeCurrent(context.impl->displayHdc, context.impl->context);
  if (res == GL_FALSE) {
      LOG(FATAL) << "wglMakeCurrent() failed.";
  }

  if (context.impl->needs_glew_initialization) {
    // Initialize GLEW on first switch to a context.
    glewExperimental = GL_TRUE;
    GLenum glew_init_result = glewInit();
    CHECK_EQ(static_cast<int>(glew_init_result), GLEW_OK);
    glGetError();  // Ignore GL_INVALID_ENUM​ error caused by glew
    context.impl->needs_glew_initialization = false;
  }
  
  return current_context;
}//SwitchOpenGLContext

#else //linux

bool OpenGLContext::InitializeWindowless(OpenGLContext* sharing_context) {
  CHECK(!impl);
  impl.reset(new OpenGLContextImpl());
  
  GLint attributes[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, None};

  int (*old_error_handler)(Display*, XErrorEvent*) =
      XSetErrorHandler(XErrorHandler);

  Display* display = XOpenDisplay(NULL);
  if (!display) {
    LOG(FATAL) << "Cannot connect to X server.";
  }

  Window root_window = DefaultRootWindow(display);
  XVisualInfo* visual = glXChooseVisual(display, 0, attributes);
  if (!visual) {
    LOG(FATAL) << "No appropriate visual found.";
  }

  GLXContext glx_context =
      glXCreateContext(display, visual, sharing_context ? sharing_context->impl->context : nullptr, GL_TRUE);
  if (!glx_context) {
    LOG(FATAL) << "Cannot create GLX context.";
  }
  XFree(visual);

  impl->display = display;
  impl->drawable = root_window;
  impl->context = glx_context;
  impl->needs_glew_initialization = true;

  XSetErrorHandler(old_error_handler);
  return true;
}

void OpenGLContext::Deinitialize() {
  if (!impl || !impl->context) {
    return;
  }
  
  glXDestroyContext(impl->display, impl->context);
  XCloseDisplay(impl->display);

  impl->drawable = None;
  impl->context = nullptr;
  
  impl.reset();
}

void OpenGLContext::AttachToCurrent() {
  impl.reset(new OpenGLContextImpl());
  
  impl->display = glXGetCurrentDisplay();
  impl->drawable = glXGetCurrentDrawable();
  impl->context = glXGetCurrentContext();
  impl->needs_glew_initialization = false;  // TODO: This is not clear.
}

void OpenGLContext::Detach() {
  impl.reset();
}


OpenGLContext SwitchOpenGLContext(const OpenGLContext& context) {
  int (*old_error_handler)(Display*, XErrorEvent*) =
      XSetErrorHandler(XErrorHandler);
  
  OpenGLContext current_context;
  current_context.AttachToCurrent();
  
  if (!current_context.impl->display) {
    // We need a display, otherwise glXMakeCurrent() will segfault.
    current_context.impl->display = context.impl->display;
  }
  
  if (glXMakeCurrent(context.impl->display, context.impl->drawable,
                     context.impl->context) == GL_FALSE) {
    LOG(FATAL) << "Cannot make GLX context current.";
  }
  
  if (context.impl->needs_glew_initialization) {
    // Initialize GLEW on first switch to a context.
    glewExperimental = GL_TRUE;
    GLenum glew_init_result = glewInit();
    CHECK_EQ(static_cast<int>(glew_init_result), GLEW_OK);
    glGetError();  // Ignore GL_INVALID_ENUM​ error caused by glew
    context.impl->needs_glew_initialization = false;
  }
  
  XSetErrorHandler(old_error_handler);
  return current_context;
}

#endif //_WIN32 & linux

}
