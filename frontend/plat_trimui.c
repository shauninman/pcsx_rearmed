/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <SDL.h>

#include "main.h"
#include "menu.h"
#include "plat.h"
#include "cspace.h"
#include "blit320.h"
#include "plugin_lib.h"
#include "libpicofe/menu.h"
#include "libpicofe/plat.h"
#include "libpicofe/input.h"
#include "libpicofe/in_sdl.h"
#include "../libpcsxcore/psxmem_map.h"

static SDL_Surface* screen;
struct plat_target plat_target;

static unsigned short *psx_vram;
static unsigned short *cspace_buf;
static int psx_step, psx_width, psx_height, psx_bpp;
static int psx_offset_x, psx_offset_y, psx_src_width, psx_src_height;
static int fb_offset_x, fb_offset_y;

static const struct in_default_bind in_sdl_defbinds[] = {
	{ SDLK_UP,        IN_BINDTYPE_PLAYER12, DKEY_UP },
	{ SDLK_DOWN,      IN_BINDTYPE_PLAYER12, DKEY_DOWN },
	{ SDLK_LEFT,      IN_BINDTYPE_PLAYER12, DKEY_LEFT },
	{ SDLK_RIGHT,     IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
	{ SDLK_LSHIFT,    IN_BINDTYPE_PLAYER12, DKEY_TRIANGLE },
	{ SDLK_LCTRL,     IN_BINDTYPE_PLAYER12, DKEY_CROSS },
	{ SDLK_SPACE,     IN_BINDTYPE_PLAYER12, DKEY_CIRCLE },
	{ SDLK_LALT,      IN_BINDTYPE_PLAYER12, DKEY_SQUARE },
	{ SDLK_RETURN,    IN_BINDTYPE_PLAYER12, DKEY_START },
	{ SDLK_RCTRL,     IN_BINDTYPE_PLAYER12, DKEY_SELECT },
	{ SDLK_TAB,       IN_BINDTYPE_PLAYER12, DKEY_L1 },
	{ SDLK_BACKSPACE, IN_BINDTYPE_PLAYER12, DKEY_R1 },
	{ SDLK_e,         IN_BINDTYPE_PLAYER12, DKEY_L2 },
	{ SDLK_t,         IN_BINDTYPE_PLAYER12, DKEY_R2 },
	{ SDLK_ESCAPE,    IN_BINDTYPE_EMU, SACTION_ENTER_MENU },
	{ SDLK_F1,        IN_BINDTYPE_EMU, SACTION_SAVE_STATE },
	{ SDLK_F2,        IN_BINDTYPE_EMU, SACTION_LOAD_STATE },
	{ SDLK_F3,        IN_BINDTYPE_EMU, SACTION_PREV_SSLOT },
	{ SDLK_F4,        IN_BINDTYPE_EMU, SACTION_NEXT_SSLOT },
	{ SDLK_F5,        IN_BINDTYPE_EMU, SACTION_TOGGLE_FSKIP },
	{ SDLK_F6,        IN_BINDTYPE_EMU, SACTION_SCREENSHOT },
	{ SDLK_F7,        IN_BINDTYPE_EMU, SACTION_TOGGLE_FPS },
	{ SDLK_F8,        IN_BINDTYPE_EMU, SACTION_SWITCH_DISPMODE },
	{ SDLK_F11,       IN_BINDTYPE_EMU, SACTION_TOGGLE_FULLSCREEN },
	{ SDLK_F12,       IN_BINDTYPE_EMU, SACTION_FAST_FORWARD },
	{ 0, 0, 0 }
};

const struct menu_keymap in_sdl_key_map[] =
{
	{ SDLK_UP,        PBTN_UP },
	{ SDLK_DOWN,      PBTN_DOWN },
	{ SDLK_LEFT,      PBTN_LEFT },
	{ SDLK_RIGHT,     PBTN_RIGHT },
	{ SDLK_SPACE,     PBTN_MOK },
	{ SDLK_LCTRL,     PBTN_MBACK },
	{ SDLK_LALT,      PBTN_MA2 },
	{ SDLK_LSHIFT,    PBTN_MA3 },
	{ SDLK_TAB,       PBTN_L },
	{ SDLK_BACKSPACE, PBTN_R },
};

const struct menu_keymap in_sdl_joy_map[] =
{
	{ SDLK_UP,    PBTN_UP },
	{ SDLK_DOWN,  PBTN_DOWN },
	{ SDLK_LEFT,  PBTN_LEFT },
	{ SDLK_RIGHT, PBTN_RIGHT },
	{ SDLK_WORLD_0, PBTN_MOK },
	{ SDLK_WORLD_1, PBTN_MBACK },
	{ SDLK_WORLD_2, PBTN_MA2 },
	{ SDLK_WORLD_3, PBTN_MA3 },
};

static const char * const in_sdl_key_names[SDLK_LAST] = {
	[SDLK_UP]         = "UP",
	[SDLK_DOWN]       = "DOWN",
	[SDLK_LEFT]       = "LEFT",
	[SDLK_RIGHT]      = "RIGHT",
	[SDLK_LSHIFT]     = "X",
	[SDLK_LCTRL]      = "B",
	[SDLK_SPACE]      = "A",
	[SDLK_LALT]       = "Y",
	[SDLK_RETURN]     = "START",
	[SDLK_RCTRL]      = "SELECT",
	[SDLK_TAB]        = "L",
	[SDLK_BACKSPACE]  = "R",
	[SDLK_ESCAPE]     = "MENU",
};

static const struct in_pdata in_sdl_platform_data = {
	.defbinds  = in_sdl_defbinds,
	.key_map   = in_sdl_key_map,
	.kmap_size = sizeof(in_sdl_key_map) / sizeof(in_sdl_key_map[0]),
	.joy_map   = in_sdl_joy_map,
	.jmap_size = sizeof(in_sdl_joy_map) / sizeof(in_sdl_joy_map[0]),
	.key_names = in_sdl_key_names,
};

static void *fb_flip(void)
{
	static int flip=0;

	{
		flip^=1;
		SDL_Flip(screen);
	}
	return screen->pixels;
}

void plat_video_menu_enter(int is_rom_loaded)
{
}

void plat_video_menu_begin(void)
{
	g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_end(void)
{
	g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_leave(void)
{
	if (psx_vram == NULL) {
		printf("%s, GPU plugin did not provide vram\n", __func__);
		exit(-1);
	}

	SDL_LockSurface(screen);
	memset(g_menuscreen_ptr, 0, 320*240 * sizeof(uint16_t));
	SDL_UnlockSurface(screen);
	g_menuscreen_ptr = fb_flip();
	SDL_LockSurface(screen);
	memset(g_menuscreen_ptr, 0, 320*240 * sizeof(uint16_t));
	SDL_UnlockSurface(screen);
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	*w = 320;
	*h = 240;
	*bpp = psx_bpp;
	return pl_vout_buf;
}

void plat_minimize(void)
{
}

#define EXTRACT(c, mask, offset) ((c >> offset) & mask)
#define BLENDCHANNEL(cl, cm, cr, mask, offset) ((((EXTRACT(cl, mask, offset) + 2 * EXTRACT(cm, mask, offset) + EXTRACT(cr, mask, offset)) >> 2) & mask) << offset)
#define BLENDB(cl, cm, cr) BLENDCHANNEL(cl, cm, cr, 0b0000000000011111, 0)
#define BLENDG(cl, cm, cr) BLENDCHANNEL(cl, cm, cr, 0b0000011111100000, 0)
#define BLENDR(cl, cm, cr) BLENDCHANNEL(cl, cm, cr, 0b0011111000000000, 2)
static void blit320_256(unsigned char *dst8, const unsigned char *src8, int unused)
{
	int source = 0;
	int x;
	uint16_t *src = (uint16_t *)src8;
	uint16_t *dst = (uint16_t *)dst8;

	for (x = 0; x < 320/5; x++)
	{
		register uint16_t a, b, c, d;

		a = src[source];
		b = src[source+1];
		c = src[source+2];
		d = src[source+3];

		*dst++ = a;
		*dst++ = b;
		*dst++ = BLENDB(b, b, c) | BLENDG(b, c, c) | BLENDR(c, c, c);
		*dst++ = BLENDB(c, c, c) | BLENDG(c, c, d) | BLENDR(c, d, d);
		*dst++ = d;

		source+=4;
	}
}

#define make_flip_func(name, scale, blitfunc)                                             \
  static void name(int doffs, const void *vram_, int w_, int h_, int sstride, int bgr24)  \
  {                                                                                       \
    const unsigned short *vram = vram_;                                                   \
    int w = w_ < psx_src_width  ? w_ : psx_src_width;                                     \
    int h = h_ < psx_src_height ? h_ : psx_src_height;                                    \
    int dst_offset_x = (fb_offset_x + ((psx_src_width - w) / 2)) * sizeof(uint16_t);      \
    int dst_offset_y = (fb_offset_y + ((psx_src_height - h) / 2)) * sizeof(uint16_t);     \
    unsigned char *conv = (unsigned char *)cspace_buf;                                    \
    unsigned char *dst = (unsigned char *)screen->pixels + dst_offset_y * 320;            \
    unsigned char *buf = (scale ? conv : dst) + dst_offset_x;                             \
    int buf_stride = scale ? 640 * sizeof(uint16_t) : 320 * sizeof(uint16_t);             \
    int dst_stride = 320 * sizeof(uint16_t);                                              \
    int len = w * psx_bpp / 8;                                                            \
    int i;                                                                                \
    void (*convertfunc)(void *dst, const void *src, int bytes);                           \
    convertfunc = psx_bpp == 24 ? bgr888_to_rgb565 : bgr555_to_rgb565;                    \
                                                                                          \
    SDL_LockSurface(screen);                                                              \
    vram += psx_offset_y * 1024 + psx_offset_x;                                           \
                                                                                          \
    for (i = h; i > 0; i--, vram += psx_step * 1024, buf += buf_stride)  {                \
      convertfunc(buf, vram, len);                                                        \
    }                                                                                     \
                                                                                          \
    if (scale) {                                                                          \
      for (i = h; i > 0; i--, dst += dst_stride, conv += buf_stride)  {                   \
        blitfunc(dst, conv, dst_stride);                                                  \
      }                                                                                   \
    }                                                                                     \
    SDL_UnlockSurface(screen);                                                            \
  }

make_flip_func(raw_blit_soft_256, true,  blit320_256)
make_flip_func(raw_blit_soft,     false, memcpy)
make_flip_func(raw_blit_soft_368, true,  blit320_368)
make_flip_func(raw_blit_soft_512, true,  blit320_512)
make_flip_func(raw_blit_soft_640, true,  blit320_640)

void *plat_gvideo_set_mode(int *w_, int *h_, int *bpp_)
{
	int poff_w, poff_h, w_max;
	int w = *w_, h = *h_, bpp = *bpp_;

	if (!w || !h || !bpp)
		return NULL;

	printf("psx mode: %dx%d@%d\n", w, h, bpp);
	psx_width = w;
	psx_height = h;
	psx_bpp = bpp;

	switch (w + (bpp != 16)) {
	case 640:
		pl_plat_blit = raw_blit_soft_640;
		w_max = 640;
		break;
	case 512:
		pl_plat_blit = raw_blit_soft_512;
		w_max = 512;
		break;
	case 384:
	case 368:
		pl_plat_blit = raw_blit_soft_368;
		w_max = 368;
		break;
	case 256:
	case 257:
		if (soft_scaling) {
			pl_plat_blit = raw_blit_soft_256;
			w_max = 256;
		} else {
			pl_plat_blit = raw_blit_soft;
			w_max = 320;
		}
		break;
	default:
		pl_plat_blit = raw_blit_soft;
		w_max = 320;
		break;
	}

	psx_step = 1;
	if (h > 256) {
		psx_step = 2;
		h /= 2;
	}

	poff_w = poff_h = 0;
	if (w > w_max) {
		poff_w = w / 2 - w_max / 2;
		w = w_max;
	}
	fb_offset_x = 0;
	if (w < 320 && !soft_scaling)
		fb_offset_x = 320/2 - w / 2;
	if (h > 240) {
		poff_h = h / 2 - 240/2;
		h = 240;
	}
	fb_offset_y = 240/2 - h / 2;

	psx_offset_x = poff_w * psx_bpp/8 / 2;
	psx_offset_y = poff_h;
	psx_src_width = w;
	psx_src_height = h;

	if (fb_offset_x || fb_offset_y) {
		// not fullscreen, must clear borders
		SDL_LockSurface(screen);
		memset(g_menuscreen_ptr, 0, 320*240 * sizeof(uint16_t));
		SDL_UnlockSurface(screen);
		g_menuscreen_ptr = fb_flip();
		SDL_LockSurface(screen);
		memset(g_menuscreen_ptr, 0, 320*240 * sizeof(uint16_t));
		SDL_UnlockSurface(screen);
		memset(cspace_buf, 0, 320*240 * sizeof(uint16_t));
	}

	// adjust for hud
	*w_ = 320;
	*h_ = fb_offset_y + psx_src_height;
	return g_menuscreen_ptr;
}

/* not really used, we do raw_flip */
void plat_gvideo_open(int is_pal)
{
}

void *plat_gvideo_flip(void)
{
	g_menuscreen_ptr = fb_flip();
	return g_menuscreen_ptr;
}

void plat_gvideo_close(void)
{
}

static void *pl_emu_mmap(unsigned long addr, size_t size, int is_fixed, enum psxMapTag tag)
{
	void *retval;

	retval = plat_mmap(addr, size, 0, is_fixed);
	if (tag == MAP_TAG_VRAM) {
		psx_vram = retval;
	}
	return retval;
}

static void pl_emu_munmap(void *ptr, size_t size, enum psxMapTag tag)
{
	plat_munmap(ptr, size);
}

void plat_sdl_event_handler(void *event_)
{
}

void plat_init(void)
{
	if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}
	else {
		SDL_Init(SDL_INIT_VIDEO);
	}
	screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
	if (screen == NULL) {
		printf("%s, failed to set video mode\n", __func__);
		exit(-1);
	}
	SDL_ShowCursor(0);
	system("echo 10 > /proc/sys/vm/swappiness");

	cspace_buf = calloc(1, 640 * 480 * sizeof(uint16_t));
	if (!cspace_buf) {
		printf("%s, failed to allocate color conversion buffer\n", __func__);
		exit(-1);
	}

	g_menuscreen_w = 320;
	g_menuscreen_h = 240;
	g_menuscreen_pp = 320;
	g_menuscreen_ptr = fb_flip();

	in_sdl_init(&in_sdl_platform_data, plat_sdl_event_handler);
	in_probe();

	pl_plat_blit = raw_blit_soft;
	psx_src_width = 320;
	psx_src_height = 240;
	psx_bpp = 16;

	pl_rearmed_cbs.screen_w = 320;
	pl_rearmed_cbs.screen_h = 240;
	psxMapHook = pl_emu_mmap;
	psxUnmapHook = pl_emu_munmap;
	in_enable_vibration = 1;
}

void plat_pre_finish(void)
{
	system("echo 60 > /proc/sys/vm/swappiness");
}

void plat_finish(void)
{
	if (cspace_buf) {
		free(cspace_buf);
	}

	SDL_Quit();
}

void plat_trigger_vibrate(int pad, int low, int high)
{
}
