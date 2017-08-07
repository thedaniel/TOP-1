#include "../comp-config.h"

#ifdef UI_FRAMEWORK_X11

#include "base.h"
#include "mainui.h"
#include "utils.h"
#include "../globals.h"
#include "../util/configfile.h"

#include <nanocanvas/NanoCanvas.h>
#define NANOVG_GLES2_IMPLEMENTATION
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <nanovg/nanovg_gl.h>
#include <nanovg/nanovg_gl_utils.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <chrono>
#include <thread>


static top1::json defaultConfig() {
  return {
    {"Key repeat", false},
    {"FPS", 60.f},
    {"Debug", false}
  };
};

// Adapted from this - check for reference:
// https://www.khronos.org/opengl/wiki/Programming_OpenGL_in_Linux:_GLX_and_Xlib

struct X11Data {
  MainUI               &ui;
  top1::json            config;
  Display              *dpy;
  Window                xWin;
  XSetWindowAttributes  swa;
  Screen*               screen;
  EGLDisplay            eglDisplay;
  EGLConfig             eglConfig;
  EGLContext            eglContext;
  EGLSurface            eglSurface;
  NativeWindowType      window;
  EGLint                numConfig;
  int                   swaMask;
  int                   numReturned;
  int                   swapFlag = True;

  uint width;
  uint height;

  XSizeHints *sizeH;
  NVGcontext* vg;
  Atom deleteWindow;

  bool keyRepeat;

  std::chrono::duration<double> lastFrameTime;
  float fps = 0;

  X11Data(MainUI &ui) : ui (ui) {}
};


const EGLint configAttribs[] = { 
  EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,    
  EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
  EGL_BLUE_SIZE,8, EGL_GREEN_SIZE,8, EGL_RED_SIZE,8, EGL_ALPHA_SIZE,8,
  EGL_STENCIL_SIZE,8, EGL_NONE };
const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION,2, EGL_NONE };
const EGLint surfaceAttribs[] = {EGL_RENDER_BUFFER,EGL_BACK_BUFFER, EGL_NONE };

static ui::Key keyboardKey(int xKey, int mods) {
  using namespace ui;
  switch (xKey) {

    // Rotaries
  case XK_q:
    if (mods & ControlMask) return K_BLUE_CLICK;
    return K_BLUE_UP;
  case XK_a:
    if (mods & ControlMask) return K_BLUE_CLICK;
    return K_BLUE_DOWN;
  case XK_w:
    if (mods & ControlMask) return K_GREEN_CLICK;
    return K_GREEN_UP;
  case XK_s:
    if (mods & ControlMask) return K_GREEN_CLICK;
    return K_GREEN_DOWN;
  case XK_e:
    if (mods & ControlMask) return K_WHITE_CLICK;
    return K_WHITE_UP;
  case XK_d:
    if (mods & ControlMask) return K_WHITE_CLICK;
    return K_WHITE_DOWN;
  case XK_r:
    if (mods & ControlMask) return K_RED_CLICK;
    return K_RED_UP;
  case XK_f:
    if (mods & ControlMask) return K_RED_CLICK;
    return K_RED_DOWN;

  case XK_Left:  return K_LEFT;
  case XK_Right: return K_RIGHT;

    // Tapedeck
  case XK_space: return K_PLAY;
  case XK_z:     return K_REC;
  case XK_F1:    return K_TRACK_1;
  case XK_F2:    return K_TRACK_2;
  case XK_F3:    return K_TRACK_3;
  case XK_F4:    return K_TRACK_4;

    // Numbers
  case XK_1:     return K_1;
  case XK_2:     return K_2;
  case XK_3:     return K_3;
  case XK_4:     return K_4;
  case XK_5:     return K_5;
  case XK_6:     return K_6;
  case XK_7:     return K_7;
  case XK_8:     return K_8;
  case XK_9:     return K_9;
  case XK_0:     return K_0;

  case XK_t:     if (mods & ControlMask) return K_TAPE; else break;
  case XK_y:     if (mods & ControlMask) return K_MIXER; else break;
  case XK_u:     if (mods & ControlMask) return K_SYNTH; else break;
  case XK_g:     if (mods & ControlMask) return K_METRONOME; else break;
  case XK_h:     if (mods & ControlMask) return K_SAMPLER; else break;
  case XK_j:     if (mods & ControlMask) return K_LOOPER; else break;

  case XK_l:     return K_LOOP;
  case XK_i:     return K_LOOP_IN;
  case XK_o:     return K_LOOP_OUT;

  case XK_x:     return K_CUT;
  case XK_c:     if (mods & ControlMask) return K_LIFT; else break;
  case XK_v:     if (mods & ControlMask) return K_DROP;

  case XK_Shift_R:
  case XK_Shift_L:
    return K_SHIFT;

  default:             return K_NONE;
  }
  return K_NONE;
}


static void event_routine(X11Data& data) {

  XEvent e;
  while(GLOB.running()) {
    XNextEvent(data.dpy, &e);

    ui::Key key;
    int keysym;
    switch (e.type) {
    case KeyPress:
      keysym = XkbKeycodeToKeysym(
        data.dpy,
        e.xkey.keycode,
        0,
        e.xkey.state & ShiftMask ? 1 : 0);

      key = keyboardKey(keysym, e.xkey.state);
      if (key) data.ui.keypress(key);
      break;
    case ClientMessage:
      if((Atom) e.xclient.data.l[0] == data.deleteWindow) {
        GLOB.exit();
      }
      break;
    case ConfigureNotify: // resize or move event
      data.width = e.xconfigure.width;
      data.height = e.xconfigure.height;
      break;
    case KeyRelease:
      keysym = XkbKeycodeToKeysym(
        data.dpy,
        e.xkey.keycode,
        0,
        e.xkey.state & ShiftMask ? 1 : 0);

      key = keyboardKey(keysym, e.xkey.state);
      switch (key) {
      case 0: break;
      case ui::K_RED_UP:
      case ui::K_RED_DOWN:
      case ui::K_BLUE_UP:
      case ui::K_BLUE_DOWN:
      case ui::K_WHITE_UP:
      case ui::K_WHITE_DOWN:
      case ui::K_GREEN_UP:
      case ui::K_GREEN_DOWN:
      case ui::K_LEFT:
      case ui::K_RIGHT:
        data.ui.keyrelease(key);
        break;
      default:
        // skip keyrepeat
        if (XEventsQueued(data.dpy, QueuedAfterReading)) {
          XEvent nev;
          XPeekEvent(data.dpy, &nev);

          if (nev.type == KeyPress && nev.xkey.time == e.xkey.time &&
            nev.xkey.keycode == e.xkey.keycode) {
            XNextEvent(data.dpy, &e);
            break;
          }
        }
        data.ui.keyrelease(key);
      }
      break;
    }

  }
}

void MainUI::mainRoutine() {
  X11Data data {*this};

  top1::ConfigFile configFile ("data/x11-config.json", defaultConfig());
  configFile.read();
  data.config = configFile.data;

  /* Open a connection to the X server */
  data.dpy = XOpenDisplay( NULL );
  if ( data.dpy == NULL ) {
    LOGF << ( "Unable to open a connection to the X server\n" );
    GLOB.exit();
    return;
  }

  data.screen = DefaultScreenOfDisplay(data.dpy);

  data.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  /* initialize the EGL display connection */
  eglInitialize(data.eglDisplay, NULL, NULL);
  /* get an appropriate EGL frame buffer configuration */
  eglChooseConfig(data.eglDisplay, configAttribs, &data.eglConfig, 1, &data.numConfig);
  /* create an EGL rendering context */
  data.eglContext = eglCreateContext(data.eglDisplay, data.eglConfig, EGL_NO_CONTEXT, contextAttribs);

  data.width = drawing::WIDTH;
  data.height = drawing::HEIGHT;

  data.xWin = XCreateWindow(data.dpy, RootWindowOfScreen(data.screen),
    0, 0, data.width, data.height, 0, DefaultDepthOfScreen(data.screen),
    InputOutput, DefaultVisualOfScreen(data.screen), data.swaMask, &data.swa);

  data.sizeH = XAllocSizeHints();
  data.sizeH->flags = PSize | PMinSize | PMaxSize | PAspect;
  data.sizeH->min_width = drawing::WIDTH;
  data.sizeH->min_height = drawing::HEIGHT;
  data.sizeH->max_width = 4000;
  data.sizeH->max_height = 3000;
  data.sizeH->max_aspect = data.sizeH->min_aspect = {4, 3};
  XSetWMNormalHints(data.dpy, data.xWin, data.sizeH);
  XSelectInput(data.dpy, data.xWin,
    KeyPressMask | StructureNotifyMask | KeyReleaseMask);

  XMapWindow( data.dpy, data.xWin );

  data.deleteWindow = XInternAtom(data.dpy, "WM_DELETE_WINDOW", false);
  XSetWMProtocols(data.dpy, data.xWin, &data.deleteWindow, 1);

  /* create an EGL window surface */
  data.eglSurface = eglCreateWindowSurface(data.eglDisplay, data.eglConfig, data.xWin, surfaceAttribs);
  /* connect the context to the surface */
  eglMakeCurrent(data.eglDisplay, data.eglSurface, data.eglSurface, data.eglContext);

	data.vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
	if (data.vg == NULL) {
		LOGF << ("Could not init nanovg.\n");
    GLOB.exit();
    return;
	}

  drawing::Canvas canvas(data.vg, drawing::WIDTH, drawing::HEIGHT);
  drawing::initUtils(canvas);

  std::thread eventThread  = std::thread([&](){event_routine(data);});

  using std::chrono::milliseconds;
  using std::chrono::seconds;
  using std::chrono::duration;
  using clock = std::chrono::high_resolution_clock;

  float desiredFPS = data.config["FPS"].get<float>();
  auto waitTime = milliseconds(int(1000.f / desiredFPS));
  auto t0 = clock::now();

  bool showFps = data.config["Debug"];

	while (GLOB.running()) {
    t0 = clock::now();

    float scale = std::min(data.width/float(drawing::WIDTH), data.height/float(drawing::HEIGHT));

		// Update and render
    canvas.setSize(data.width, data.height);
		glViewport(0, 0, data.width, data.height);

    glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);
    glEnable(GL_STENCIL_TEST);

    canvas.clearColor(drawing::Colours::Black);
    canvas.begineFrame(data.width, data.height);

    canvas.scale(scale, scale);
    draw(canvas);

    if (showFps) {
      canvas.beginPath();
      canvas.font(15);
      canvas.font(drawing::FONT_NORM);
      canvas.fillStyle(drawing::Colours::White);
      canvas.textAlign(drawing::TextAlign::Left, drawing::TextAlign::Baseline);
      canvas.fillText(fmt::format("{:.2f} FPS ({:.2f})", data.fps, desiredFPS), {0, drawing::HEIGHT});
    }
    canvas.endFrame();

    if (data.swapFlag)
      eglSwapBuffers(data.eglDisplay, data.eglSurface);

    glFlush();

    data.lastFrameTime = clock::now() - t0;
    std::this_thread::sleep_for(waitTime - data.lastFrameTime);

    data.fps = 1 / (std::chrono::duration_cast<milliseconds>(clock::now() - t0).count() / 1000.f);
	}

	nvgDeleteGLES2(data.vg);

  GLOB.exit();
}

#endif
