#include "../comp-config.h"
#ifdef UIFRAMEWORK_RPI

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

static top1::json defaultConfig() {
  return {
  };
};


int singleBufferAttributess[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE,   GLX_RGBA_BIT,
  GLX_RED_SIZE,      1,   /* Request a single buffered color buffer */
  GLX_GREEN_SIZE,    1,   /* with the maximum number of color bits  */
  GLX_BLUE_SIZE,     1,   /* for each component                     */
  None
};

int doubleBufferAttributes[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE,   GLX_RGBA_BIT,
  GLX_DOUBLEBUFFER,  True,  /* Request a double-buffered color buffer with */
  GLX_RED_SIZE,      1,     /* the maximum number of bits per component    */
  GLX_GREEN_SIZE,    1, 
  GLX_BLUE_SIZE,     1,
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


static void event_routine(MainUI& self, Display *display) {

  XEvent e;
  Atom wm_del = XInternAtom(display, "WM_DELETE_WINDOW", false);
  while(GLOB.running()) {
    XNextEvent(display, &e);

    ui::Key key;
    int keysym;
    switch (e.type) {
    case KeyPress:
      keysym = XkbKeycodeToKeysym(
        display,
        e.xkey.keycode,
        0,
        e.xkey.state & ShiftMask ? 1 : 0);

      key = keyboardKey(keysym, e.xkey.state);
      if (key) self.keypress(key);
      break;
    case ClientMessage:
      if((Atom) e.xclient.data.l[0] == wm_del) {
        GLOB.exit();
      }
      break;
    case KeyRelease:
      // if (XEventsQueued(display, QueuedAfterReading)) {
      //   XEvent nev;
      //   XPeekEvent(display, &nev);

      //   if (nev.type == KeyPress && nev.xkey.time == e.xkey.time &&
      //   nev.xkey.keycode == e.xkey.keycode) {
      //     XNextEvent(display, &e);
      //     break;
      //   }
      // }
      // keysym = XkbKeycodeToKeysym(
      //   display,
      //   e.xkey.keycode,
      //   0,
      //   e.xkey.state & ShiftMask ? 1 : 0);

      // key = keyboardKey(keysym);
      if (key) self.keyrelease(key);
      break;
    }

  }
}

static Bool WaitForNotify( Display *dpy, XEvent *event, XPointer arg ) {
  return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
}

void MainUI::mainRoutine() {
  top1::ConfigFile configFile ("data/rpi.json", defaultConfig());
  configFile.read();
  top1::json config = configFile.data;

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

  /* Open a connection to the X server */
  dpy = XOpenDisplay( NULL );
  if ( dpy == NULL ) {
      printf( "Unable to open a connection to the X server\n" );
      exit();
  }

  /* Request a suitable framebuffer configuration - try for a double 
  ** buffered configuration first */
  fbConfigs = glXChooseFBConfig( dpy, DefaultScreen(dpy),
                                  doubleBufferAttributes, &numReturned );

  if ( fbConfigs == NULL ) {  /* no double buffered configs available */
    fbConfigs = glXChooseFBConfig( dpy, DefaultScreen(dpy),
                                    singleBufferAttributess, &numReturned );
    swapFlag = False;
  }

  vInfo = glXGetVisualFromFBConfig( dpy, fbConfigs[0] );

  swa.border_pixel = 0;
  swa.event_mask = StructureNotifyMask;
  swa.colormap = XCreateColormap( dpy, RootWindow(dpy, vInfo->screen),
                                  vInfo->visual, AllocNone );

  swaMask = CWBorderPixel | CWColormap | CWEventMask;

  uint width = 320;
  uint height = 240;

  xWin = XCreateWindow( dpy, RootWindow(dpy, vInfo->screen), 0, 0, width, height,
                        0, vInfo->depth, InputOutput, vInfo->visual,
                        swaMask, &swa );

  /* Create a GLX context for OpenGL rendering */
  context = glXCreateNewContext( dpy, fbConfigs[0], GLX_RGBA_TYPE,
        NULL, True );

  /* Create a GLX window to associate the frame buffer configuration
  ** with the created X window */
  glxWin = glXCreateWindow( dpy, fbConfigs[0], xWin, NULL );

  XSizeHints *sizeH = XAllocSizeHints();
  sizeH->flags = PSize | PMinSize | PMaxSize;
  sizeH->min_width = drawing::WIDTH;
  sizeH->min_height = drawing::HEIGHT;
  sizeH->max_width = 4000;
  sizeH->max_height = 3000;
  XSetWMNormalHints(dpy, xWin, sizeH);
  XSelectInput(dpy, xWin, KeyPressMask);

  XMapWindow( dpy, xWin );

  Atom WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", false);
  XSetWMProtocols(dpy, xWin, &WM_DELETE_WINDOW, 1);

  glXMakeContextCurrent( dpy, glxWin, glxWin, context );

	NVGcontext* vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
	if (vg == NULL) {
		printf("Could not init nanovg.\n");
		return;
	}

  drawing::Canvas canvas(vg, drawing::WIDTH, drawing::HEIGHT);
  drawing::initUtils(canvas);

  float scale = std::min(width/float(drawing::WIDTH), height/float(drawing::HEIGHT));

  canvas.setSize(width, height);

  std::thread eventThread  = std::thread([&](){event_routine(*this, dpy);});

	while (GLOB.running())
	{
    Window* root;
		// Update and render
		glViewport(0, 0, width, height);

    glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

    canvas.clearColor(drawing::Colours::Black);
    canvas.begineFrame(width, height);

    canvas.scale(scale, scale);
    draw(canvas);

    canvas.endFrame();

		glEnable(GL_DEPTH_TEST);

    if ( swapFlag )
      glXSwapBuffers( dpy, glxWin );

    glFlush();

    std::this_thread::sleep_for(
      std::chrono::milliseconds(int(100/6)));

	}

	nvgDeleteGLES2(vg);

  GLOB.exit();
}

#endif // UIFRAMEWORK == RPI
