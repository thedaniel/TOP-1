#include "../comp-config.h"

#include <nanocanvas/NanoCanvas.h>
#define NANOVG_GLES2_IMPLEMENTATION
#include <GLES2/gl2.h>
#include <nanovg/nanovg_gl.h>
#include <nanovg/nanovg_gl_utils.h>

#include "base.h"
#include "mainui.h"
#include "utils.h"
#include "../globals.h"
#include "../util/configfile.h"

#include <GL/glx.h>
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
  XVisualInfo          *vInfo;
  XSetWindowAttributes  swa;
  GLXFBConfig          *fbConfigs;
  GLXContext            context;
  GLXWindow             glxWin;
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
};

int singleBufferAttributess[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE,   GLX_RGBA_BIT,
  GLX_RED_SIZE,      1,   /* Request a single buffered color buffer */
  GLX_GREEN_SIZE,    1,   /* with the maximum number of color bits  */
  GLX_BLUE_SIZE,     1,   /* for each component                     */
  GLX_STENCIL_SIZE,  1,
  None
};

int doubleBufferAttributes[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE,   GLX_RGBA_BIT,
  GLX_DOUBLEBUFFER,  True,  /* Request a double-buffered color buffer with */
  GLX_RED_SIZE,      1,     /* the maximum number of bits per component    */
  GLX_GREEN_SIZE,    1,
  GLX_BLUE_SIZE,     1,
  GLX_STENCIL_SIZE,  1,
  None
};

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
      printf( "Unable to open a connection to the X server\n" );
      exit();
  }

  /* Request a suitable framebuffer configuration - try for a double 
  ** buffered configuration first */
  data.fbConfigs = glXChooseFBConfig( data.dpy, DefaultScreen(data.dpy),
                                  doubleBufferAttributes, &data.numReturned );

  if ( data.fbConfigs == NULL ) {  /* no double buffered configs available */
    data.fbConfigs = glXChooseFBConfig(data.dpy, DefaultScreen(data.dpy),
                                    singleBufferAttributess, &data.numReturned );
    data.swapFlag = False;
  }

  data.vInfo = glXGetVisualFromFBConfig( data.dpy, data.fbConfigs[0] );

  data.swa.border_pixel = 0;
  data.swa.event_mask = StructureNotifyMask;
  data.swa.colormap = XCreateColormap( data.dpy, RootWindow(data.dpy, data.vInfo->screen),
   data.vInfo->visual, AllocNone );

  data.swaMask = CWBorderPixel | CWColormap | CWEventMask;

  data.width = drawing::WIDTH;
  data.height = drawing::HEIGHT;

  data.xWin = XCreateWindow( data.dpy, RootWindow(data.dpy, data.vInfo->screen),
    0, 0, data.width, data.height,
    0, data.vInfo->depth, InputOutput, data.vInfo->visual, data.swaMask, &data.swa );

  /* Create a GLX context for OpenGL rendering */
  data.context = glXCreateNewContext( data.dpy, data.fbConfigs[0], GLX_RGBA_TYPE,
        NULL, True );

  /* Create a GLX window to associate the frame buffer configuration
  ** with the created X window */
  data.glxWin = glXCreateWindow( data.dpy, data.fbConfigs[0], data.xWin, NULL );

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

  glXMakeContextCurrent( data.dpy, data.glxWin, data.glxWin, data.context );

	data.vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
	if (data.vg == NULL) {
		printf("Could not init nanovg.\n");
		return;
	}

  drawing::Canvas canvas(data.vg, drawing::WIDTH, drawing::HEIGHT);
  drawing::initUtils(canvas);

  std::thread eventThread  = std::thread([&](){event_routine(data);});

  using std::chrono::milliseconds;
  using std::chrono::seconds;
  using std::chrono::duration;
  using clock = std::chrono::high_resolution_clock;

  auto waitTime = milliseconds(int(1000.f / data.config["FPS"].get<float>()));
  auto t0 = clock::now();

  bool showFps = data.config["Debug"].get<bool>();

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
      canvas.fillText(fmt::format("{:.2f} FPS", data.fps), {0, drawing::HEIGHT});
    }
    canvas.endFrame();

    if (data.swapFlag)
      glXSwapBuffers(data.dpy, data.glxWin);

    glFlush();

    data.lastFrameTime = clock::now() - t0;
    std::this_thread::sleep_for(waitTime - data.lastFrameTime);

    data.fps = 1 / (std::chrono::duration_cast<milliseconds>(clock::now() - t0).count() / 1000.f);
	}

	nvgDeleteGLES2(data.vg);

  GLOB.exit();
}
