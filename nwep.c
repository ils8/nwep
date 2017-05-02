#define XRES 1280
#define YRES 720

#ifdef _DEBUG
#ifdef FULLSCREEN
#undef FULLSCREEN
#endif
#define DEBUG_FUNCLOAD
#define SHADER_DEBUG
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#define VC_LEANMEAN
#define VC_EXTRALEAN

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <GL/gl.h>

#ifdef _DEBUG
//#pragma data_seg(".fltused")
int _fltused = 1;
#endif
#elif defined(__linux__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#define __stdcall
#define __inline inline __attribute__((always_inline))

#define oglCreateShaderProgramv glCreateShaderProgramv
#define oglGenFramebuffers glGenFramebuffers
#define oglBindFramebuffer glBindFramebuffer
#define oglFramebufferTexture2D glFramebufferTexture2D
#define oglUseProgram glUseProgram
#define oglGetUniformLocation glGetUniformLocation
#define oglUniform1i glUniform1i
#define oglUniform3f glUniform3f
#endif

#include "glext.h"

#include "music/4klang.h"
#define INTRO_LENGTH (1000 * MAX_SAMPLES / SAMPLE_RATE)

#pragma data_seg(".raymarch.glsl")
#include "raymarch.h"

#pragma data_seg(".post.glsl")
#include "post.h"

#pragma data_sef(".timeline.h")
#include "timeline.h"

#ifdef NO_CREATESHADERPROGRAMV
FUNCLIST_DO(PFNGLCREATESHADERPROC, CreateShader) \
FUNCLIST_DO(PFNGLSHADERSOURCEPROC, ShaderSource) \
FUNCLIST_DO(PFNGLCOMPILESHADERPROC, CompileShader) \
FUNCLIST_DO(PFNGLCREATEPROGRAMPROC, CreateProgram) \
FUNCLIST_DO(PFNGLATTACHSHADERPROC, AttachShader) \
FUNCLIST_DO(PFNGLLINKPROGRAMPROC, LinkProgram)
#endif

#define FUNCLIST \
  FUNCLIST_DO(PFNGLCREATESHADERPROGRAMVPROC, CreateShaderProgramv) \
  FUNCLIST_DO(PFNGLUSEPROGRAMPROC, UseProgram) \
  FUNCLIST_DO(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) \
  FUNCLIST_DO(PFNGLUNIFORM1IPROC, Uniform1i) \
  FUNCLIST_DO(PFNGLUNIFORM3FPROC, Uniform3f) \
  FUNCLIST_DO(PFNGLGENFRAMEBUFFERSPROC, GenFramebuffers) \
  FUNCLIST_DO(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer) \
  FUNCLIST_DO(PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D)
#ifndef DEBUG
#define FUNCLIST_DBG
#else
#define FUNCLIST_DBG \
  FUNCLIST_DO(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) \
  FUNCLIST_DO(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) \
  FUNCLIST_DO(PFNGLCHECKFRAMEBUFFERSTATUSPROC, CheckFramebufferStatus)
#endif

#ifdef _WIN32
#pragma data_seg(".glfuncs")
#define FUNCLIST_DO(T,N) static T ogl##N;
FUNCLIST FUNCLIST_DBG
#undef FUNCLIST_DO

#pragma data_seg(".pfd")
static const PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
	32, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 32, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };

#pragma data_seg(".devmode")
static const DEVMODE screenSettings = { {0},
	#if _MSC_VER < 1400
	0,0,148,0,0x001c0000,{0},0,0,0,0,0,0,0,0,0,{0},0,32,XRES,YRES,0,0,      // Visual C++ 6.0
	#else
	0,0,156,0,0x001c0000,{0},0,0,0,0,0,{0},0,32,XRES,YRES,{0}, 0,           // Visual Studio 2005+
	#endif
	#if(WINVER >= 0x0400)
	0,0,0,0,0,0,
	#if (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400)
	0,0
	#endif
	#endif
	};

#pragma data_seg(".wavefmt")
static const WAVEFORMATEX WaveFMT =
{
#ifdef FLOAT_32BIT
	WAVE_FORMAT_IEEE_FLOAT,
#else
	WAVE_FORMAT_PCM,
#endif
	2,                                   // channels
	SAMPLE_RATE,                         // samples per sec
	SAMPLE_RATE*sizeof(SAMPLE_TYPE) * 2, // bytes per sec
	sizeof(SAMPLE_TYPE) * 2,             // block alignment;
	sizeof(SAMPLE_TYPE) * 8,             // bits per sample
	0                                    // extension not needed
};

#pragma data_seg(".wavehdr")
static WAVEHDR WaveHDR =
{
	(LPSTR)lpSoundBuffer, MAX_SAMPLES*sizeof(SAMPLE_TYPE)*2,0,0,0,0,0,0
};

/*
static MMTIME MMTime =
{
	TIME_SAMPLES, 0
};
*/
#elif defined(__linux__)
#endif


#pragma data_seg(".static")
static SAMPLE_TYPE lpSoundBuffer[MAX_SAMPLES * 2];
static int p_ray, p_dof;
enum { FbTex_Ray, FbTex_COUNT };
static GLuint tex[FbTex_COUNT], fb[FbTex_COUNT];

#pragma code_seg(".compileProgram")
static __inline int compileProgram(const char *fragment) {
#ifdef NO_CREATESHADERPROGRAMV
	const int pid = oglCreateProgram();
	const int fsId = oglCreateShader(GL_FRAGMENT_SHADER);
	oglShaderSource(fsId, 1, &fragment, 0);
	oglCompileShader(fsId);

#ifdef SHADER_DEBUG
	int result;
	char info[2048];
#define oglGetObjectParameteriv ((PFNGLGETOBJECTPARAMETERIVARBPROC) wglGetProcAddress("glGetObjectParameterivARB"))
#define oglGetInfoLog ((PFNGLGETINFOLOGARBPROC) wglGetProcAddress("glGetInfoLogARB"))
	oglGetObjectParameteriv(fsId, GL_OBJECT_COMPILE_STATUS_ARB, &result);
	oglGetInfoLog(fsId, 2047, NULL, (char*)info);
	if (!result)
	{
		MessageBox(NULL, info, "COMPILE", 0x00000000L);
		ExitProcess(0);
	}
#endif

	oglAttachShader(pid, fsId);
	oglLinkProgram(pid);

#else
	const int pid = oglCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragment);
#endif

#ifdef SHADER_DEBUG
	{
		int result;
		char info[2048];
#define oglGetObjectParameteriv ((PFNGLGETOBJECTPARAMETERIVARBPROC) wglGetProcAddress("glGetObjectParameterivARB"))
#define oglGetInfoLog ((PFNGLGETINFOLOGARBPROC) wglGetProcAddress("glGetInfoLogARB"))
		oglGetObjectParameteriv(pid, GL_OBJECT_LINK_STATUS_ARB, &result);
		oglGetInfoLog(pid, 2047, NULL, (char*)info);
		if (!result)
		{
			MessageBox(NULL, info, "LINK", 0x00000000L);
			ExitProcess(0);
		}
	}
#endif
	return pid;
}

static __inline void initFbTex(int fb, int tex) {
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, XRES, YRES, 0, GL_RGBA, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	oglBindFramebuffer(GL_FRAMEBUFFER, fb);
	oglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
}

static void paint(int prog, int src_tex, int dst_fb, int time) {
	oglUseProgram(prog);
	glBindTexture(GL_TEXTURE_2D, src_tex);
	oglBindFramebuffer(GL_FRAMEBUFFER, dst_fb);
	oglUniform1i(oglGetUniformLocation(prog, "B"), 0);
	oglUniform3f(oglGetUniformLocation(prog, "V"), XRES, YRES, time * 1e-3f);
	oglUniform3f(oglGetUniformLocation(prog, "C"), 20., 0., 0.);
	oglUniform3f(oglGetUniformLocation(prog, "A"), 0., 0., 0.);
	glRects(-1, -1, 1, 1);
}

static __inline void intro_init() {
	p_ray = compileProgram(raymarch_glsl);
	p_dof = compileProgram(post_glsl);
	//const int p_out = compileProgram(out_glsl);

	glGenTextures(FbTex_COUNT, tex);
	oglGenFramebuffers(FbTex_COUNT, fb);

	initFbTex(tex[FbTex_Ray], fb[FbTex_Ray]);
	//initFbTex(tex[FbTex_Dof], fb[FbTex_Dof]);
}

static __inline void intro_paint(int time) {
	paint(p_ray, 0, fb[FbTex_Ray], time);
	paint(p_dof, tex[FbTex_Ray], 0, time);
}

#ifdef _WIN32
#pragma code_seg(".entry")
void entrypoint( void ) {
	#ifdef FULLSCREEN
	ChangeDisplaySettings(&screenSettings,CDS_FULLSCREEN);
	ShowCursor(0);
	HDC hDC = GetDC(CreateWindow((LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE, 0, 0, XRES, YRES, 0, 0, 0, 0));
	#else
	HDC hDC = GetDC(CreateWindow("static", 0, WS_POPUP | WS_VISIBLE, 0, 0, XRES, YRES, 0, 0, 0, 0));
	#endif

	// initalize opengl
	SetPixelFormat(hDC,ChoosePixelFormat(hDC,&pfd),&pfd);
	wglMakeCurrent(hDC,wglCreateContext(hDC));
	
#ifdef DEBUG_FUNCLOAD
#define FUNCLIST_DO(T, N) ogl##N = (T)wglGetProcAddress("gl" # N); \
	if (!ogl##N) { \
		MessageBox(NULL, gl # N, "wglGetProcAddress", 0x00000000L); \
	}
#else
#define FUNCLIST_DO(T, N) ogl##N = (T)wglGetProcAddress("gl" # N);
#endif
	FUNCLIST FUNCLIST_DBG
#undef FUNCLIST_DO

	intro_init();

	// initialize sound
	HWAVEOUT hWaveOut;
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)_4klang_render, lpSoundBuffer, 0, 0);
	//_4klang_render(lpSoundBuffer);
	waveOutOpen(&hWaveOut, WAVE_MAPPER, &WaveFMT, NULL, 0, CALLBACK_NULL);
	waveOutPrepareHeader(hWaveOut, &WaveHDR, sizeof(WaveHDR));
	waveOutWrite(hWaveOut, &WaveHDR, sizeof(WaveHDR));
	const int to = timeGetTime();

	// play intro
	do {
		//waveOutGetPosition(hWaveOut, &MMTime, sizeof(MMTIME));
		const int time = timeGetTime() - to;
		intro_paint(time);
		SwapBuffers(hDC);
		/* hide cursor properly */
		PeekMessageA(0, 0, 0, 0, PM_REMOVE);
		if (time > INTRO_LENGTH) break;
	} while (!GetAsyncKeyState(VK_ESCAPE));
		// && MMTime.u.sample < MAX_SAMPLES);

	#ifdef CLEANDESTROY
	sndPlaySound(0,0);
	ChangeDisplaySettings( 0, 0 );
	ShowCursor(1);
	#endif

	ExitProcess(0);
}
#elif defined(__linux__)
/* cc -Wall -Wno-unknown-pragmas `pkg-config --cflags --libs sdl` -lGL nwep.c -o nwep */
#include <SDL.h>

//void _start() {
void main() {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetVideoMode(XRES, YRES, 32, SDL_OPENGL);
	glViewport(0, 0, XRES, YRES);
	intro_init();
	for(;;) {
		SDL_Event e;
		SDL_PollEvent(&e);
		if (e.type == SDL_KEYDOWN) break;
		intro_paint(SDL_GetTicks());
		SDL_GL_SwapBuffers();
	}
/*
	asm ( \
		"xor %eax,%eax\n" \
		"inc %eax\n" \
		"int $0x80\n" \
	);*/
	SDL_Quit();
}
#else
#error Not ported
#endif
