#include "../comp-config.h"
#ifdef UIFRAMEWORK_RPI

#include <nanocanvas/NanoCanvas.h>
#define NANOVG_GLES2_IMPLEMENTATION
#include <GLES2/gl2.h>
#include <GL/osmesa.h>
#include <nanovg/nanovg_gl.h>
#include <nanovg/nanovg_gl_utils.h>

#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>


#include "base.h"
#include "mainui.h"
#include "utils.h"
#include "../globals.h"
#include "../util/configfile.h"

static top1::json defaultConfig() {
  return {
    {"framebuffer", "/dev/fb0"}
  };
};

void MainUI::mainRoutine() {
  top1::ConfigFile configFile ("data/rpi.json", defaultConfig());
  configFile.read();
  top1::json config = configFile.data;

  std::string filename = config["framebuffer"].get<std::string>();

  int fbfd = 0; // framebuffer filedescriptor
  fb_var_screeninfo vinfo;
  fb_fix_screeninfo finfo;

  // Open the framebuffer device file for reading and writing
  fbfd = open(filename.c_str(), O_RDWR);
  if (fbfd == -1) {
    LOGF << "Error: cannot open framebuffer device.";
    return;
  }

  LOGD << "The framebuffer device opened.";

  if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
    LOGF << "Error reading fixed framebuffer info";
    return;
  }
  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
    LOGF << "Error reading variable framebuffer info";
    return;
  }
  int width = vinfo.width;
  int height = vinfo.height;

  long int screensize = finfo.smem_len;
  char* fbptr = (char*) mmap(0,
    screensize,
    PROT_READ | PROT_WRITE,
    MAP_SHARED,
    fbfd, 0);

  OSMesaContext ctx;
  /* Create an RGBA-mode context */
#if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
  /* specify Z, stencil, accum sizes */
  ctx = OSMesaCreateContextExt( OSMESA_RGBA, 16, 0, 0, NULL );
#else
  ctx = OSMesaCreateContext( OSMESA_RGBA, NULL );
#endif

	if (!ctx) {
		LOGF << "Failed to init OSMesa. Exiting";
    return;
	}

  /* Bind the buffer to the context and make it current */
  if (!OSMesaMakeCurrent( ctx, fbptr, GL_UNSIGNED_BYTE, width, height )) {
    LOGF << "OSMesaMakeCurrent failed!";
    return;
  }

	NVGcontext* vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
	if (vg == NULL) {
		printf("Could not init nanovg.\n");
		return;
	}

  drawing::Canvas canvas(vg, drawing::WIDTH, drawing::HEIGHT);
  drawing::initUtils(canvas);

  float scale = std::min(width/float(drawing::WIDTH), height/float(drawing::HEIGHT));

  canvas.setSize(width, height);

	while (GLOB.running())
	{
		// Update and render
		glViewport(0, 0, width, height);

    glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		// glEnable(GL_BLEND);
		// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		// glEnable(GL_CULL_FACE);
		// glDisable(GL_DEPTH_TEST);

    canvas.clearColor(drawing::Colours::Black);
    canvas.begineFrame(width, height);

    canvas.scale(scale, scale);
    draw(canvas);

    canvas.endFrame();

		// glEnable(GL_DEPTH_TEST);

    std::this_thread::sleep_for(
      std::chrono::milliseconds(int(100/6)));

	}

	nvgDeleteGLES2(vg);

  // unmap framebuffer
  munmap(fbptr, screensize);
  // close file
  close(fbfd);

  GLOB.exit();
}

#endif // UIFRAMEWORK == RPI
