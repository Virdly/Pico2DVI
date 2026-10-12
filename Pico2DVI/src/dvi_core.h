// dvi_core.h -- libdvi (PicoDVI) core API, merged for the Arduino IDE port.
// Target: Waveshare RP2350-PiZero on the earlephilhower arduino-pico core
// (the core ships the full pico SDK hardware API: pio/dma/interp/pwm/sio).
// BSD-3-Clause (C) 2021 Luke Wren / MIT portions (C) Shuichi Takano --
// see LICENSE and PROVENANCE.md in this repo.
//
// This ONE header merges these original Pico SDK library files:
//   dvi_config_defs.h, util_queue_u32_inline.h, dvi_serialiser.h,
//   dvi_timing.h, dvi.h  (+ the generated PIO headers live in
//   dvi_pio_merged.h, included at the bottom).
#ifndef _DVI_CORE_H
#define _DVI_CORE_H

#include <stdint.h>
#include <stdbool.h>

// Compile-time configuration definitions for libdvi. This file provides
// defaults -- you can override using a board header, or setting compile
// definitions directly from the commandline (e.g. using CMake
// target_compile_definitions())

// Pull in base headers to make sure board definitions override the
// definitions provided here. Note this file is included in asm and C.
//
// PIZERO build fix: when assembling tmds_encode.S these headers transitively
// pull in a Pico board header whose pico_board_cmake_set(...) lines are not
// valid assembler (pico.h, which no-ops those macros, isn't included first in
// the .S path). The .S only needs the DVI_* numeric macros from this file
// (also forced via -D build flags), so skip the SDK config headers for asm.
#ifndef __ASSEMBLER__
#include "hardware/platform_defs.h"
#include "pico/config.h"
#endif

// ----------------------------------------------------------------------------
// General DVI defines

// How many times to output the same TMDS buffer before recyling it onto the
// free queue. Pixels are repeated vertically if this is >1.
#ifndef DVI_VERTICAL_REPEAT
#define DVI_VERTICAL_REPEAT 2
#endif

// Number of TMDS buffers to allocate (malloc()) in DVI init. You can set this
// to 0 if you want to allocate your own (e.g. if you want static buffers)
#ifndef DVI_N_TMDS_BUFFERS
#define DVI_N_TMDS_BUFFERS 3
#endif

// If 1, replace the DVI serialiser with a 10n1 UART (1 start bit, 10 data
// bits, 1 stop bit) so the stream can be dumped and analysed easily.
#ifndef DVI_SERIAL_DEBUG
#define DVI_SERIAL_DEBUG 0
#endif

// If 1, the same TMDS symbols are sent to all 3 lanes during the horizontal
// active period. This means only monochrome colour is available, but the TMDS
// buffers are 3 times smaller as a result, and the performance requirements
// for encode are also cut by 3.
#ifndef DVI_MONOCHROME_TMDS
#define DVI_MONOCHROME_TMDS 0
#endif

// By default, we assume each 32-bit word written to a PIO FIFO contains 2x
// 10-bit TMDS symbols, concatenated into the lower 20 bits, least-significant
// first. This is convenient if you are generating two or more pixels at once,
// e.g. using the pixel-doubling TMDS encode. You can change this value to 1
// (so each word contains 1 symbol) for e.g. full resolution RGB encode. Note
// that this value needs to divide the DVI horizontal timings, so is limited
// to 1 or 2.
#ifndef DVI_SYMBOLS_PER_WORD
#define DVI_SYMBOLS_PER_WORD 2
#endif

#if DVI_SYMBOLS_PER_WORD != 1 && DVI_SYMBOLS_PER_WORD !=2
#error "Unsupported value for DVI_SYMBOLS_PER_WORD"
#endif

// ----------------------------------------------------------------------------
// Pixel component layout

// By default we go R, G, B from MSB -> LSB. Override to e.g. swap RGB <-> BGR

// Default 8bpp layout: RGB332, {r[1:0], g[2:0], b[1:0]}

#ifndef DVI_8BPP_RED_MSB
#define DVI_8BPP_RED_MSB 7
#endif

#ifndef DVI_8BPP_RED_LSB
#define DVI_8BPP_RED_LSB 5
#endif

#ifndef DVI_8BPP_GREEN_MSB
#define DVI_8BPP_GREEN_MSB 4
#endif

#ifndef DVI_8BPP_GREEN_LSB
#define DVI_8BPP_GREEN_LSB 2
#endif

#ifndef DVI_8BPP_BLUE_MSB
#define DVI_8BPP_BLUE_MSB 1
#endif

#ifndef DVI_8BPP_BLUE_LSB
#define DVI_8BPP_BLUE_LSB 0
#endif

// Default 16bpp layout: RGB565, {r[4:0], g[5:0], b[4:0]}

#ifndef DVI_16BPP_RED_MSB
#define DVI_16BPP_RED_MSB 15
#endif

#ifndef DVI_16BPP_RED_LSB
#define DVI_16BPP_RED_LSB 11
#endif

#ifndef DVI_16BPP_GREEN_MSB
#define DVI_16BPP_GREEN_MSB 10
#endif

#ifndef DVI_16BPP_GREEN_LSB
#define DVI_16BPP_GREEN_LSB 5
#endif

#ifndef DVI_16BPP_BLUE_MSB
#define DVI_16BPP_BLUE_MSB 4
#endif

#ifndef DVI_16BPP_BLUE_LSB
#define DVI_16BPP_BLUE_LSB 0
#endif

// Default 1bpp layout: bitwise little-endian, i.e. least significant bit of
// each word is the first (leftmost) of a block of 32 pixels.

// If 1, reverse the order of pixels within each byte. Order of bytes within
// each word is still little-endian.
#ifndef DVI_1BPP_BIT_REVERSE
#define DVI_1BPP_BIT_REVERSE 0
#endif

// ----------------------------------------------------------------------------
// TMDS encode controls

// Number of TMDS loop bodies between branches. cmp + branch costs 3 cycles,
// so you can easily save 10% of encode time by bumping this. Note that body
// will *already* produce multiple pixels, and total symbols per iteration
// must cleanly divide symbols per scanline, else the loop won't terminate.
// Point gun away from foot.
#ifndef TMDS_ENCODE_UNROLL
#define TMDS_ENCODE_UNROLL 1
#endif

// If 1, don't save/restore the interpolators on full-resolution TMDS encode.
// Speed hack. The TMDS code uses both interpolators, for each of the 3 data
// channels, so this define avoids 6 save/restores per scanline.
#ifndef TMDS_FULLRES_NO_INTERP_SAVE
#define TMDS_FULLRES_NO_INTERP_SAVE 0
#endif

// If 1, don't DC-balance the output of full resolution encode. Hilariously
// noncompliant, but Dell Ultrasharp -- the honey badger of computer monitors
// -- does not seem to mind (it helps that we DC-couple). Another speed hack,
// useful when you are trying to get everything else up to speed.
#ifndef TMDS_FULLRES_NO_DC_BALANCE
#define TMDS_FULLRES_NO_DC_BALANCE 0
#endif

// Faster versions of the functions found in pico/util/queue.h, for the common
// case of 32-bit-sized elements. Can be used on the same queue data
// structure, and mixed freely with the generic access methods, as long as
// element_size == 4.

#include "pico/util/queue.h"
#include "hardware/sync.h"

static inline uint16_t _queue_inc_index_u32(queue_t *q, uint16_t index) {
    if (++index > q->element_count) { // > because we have element_count + 1 elements
        index = 0;
    }
    return index;
}

static inline bool queue_try_add_u32(queue_t *q, void *data) {
    bool success = false;
    uint32_t flags = spin_lock_blocking(q->core.spin_lock);
    if (queue_get_level_unsafe(q) != q->element_count) {
        ((uint32_t*)q->data)[q->wptr] = *(uint32_t*)data;
        q->wptr = _queue_inc_index_u32(q, q->wptr);
        success = true;
    }
    spin_unlock(q->core.spin_lock, flags);
    if (success) __sev();
    return success;
}

static inline bool queue_try_remove_u32(queue_t *q, void *data) {
    bool success = false;
    uint32_t flags = spin_lock_blocking(q->core.spin_lock);
    if (queue_get_level_unsafe(q) != 0) {
        *(uint32_t*)data = ((uint32_t*)q->data)[q->rptr];
        q->rptr = _queue_inc_index_u32(q, q->rptr);
        success = true;
    }
    spin_unlock(q->core.spin_lock, flags);
    if (success) __sev();
    return success;
}

static inline bool queue_try_peek_u32(queue_t *q, void *data) {
    bool success = false;
    uint32_t flags = spin_lock_blocking(q->core.spin_lock);
    if (queue_get_level_unsafe(q) != 0) {
        *(uint32_t*)data = ((uint32_t*)q->data)[q->rptr];
        success = true;
    }
    spin_unlock(q->core.spin_lock, flags);
    return success;
}

static inline void queue_add_blocking_u32(queue_t *q, void *data) {
    bool done;
    do {
        done = queue_try_add_u32(q, data);
        if (done) break;
        __wfe();
    } while (true);
}

static inline void queue_remove_blocking_u32(queue_t *q, void *data) {
    bool done;
    do {
        done = queue_try_remove_u32(q, data);
        if (done) break;
        __wfe();
    } while (true);
}

static inline void queue_peek_blocking_u32(queue_t *q, void *data) {
    bool done;
    do {
        done = queue_try_peek_u32(q, data);
        if (done) break;
        __wfe();
    } while (true);
}

#include "hardware/pio.h"
// (merged into this header)

#define N_TMDS_LANES 3

struct dvi_serialiser_cfg {
	PIO pio;
	uint sm_tmds[N_TMDS_LANES];
	uint pins_tmds[N_TMDS_LANES];
	uint pins_clk;
	bool invert_diffpairs;
	uint prog_offs;
};

void dvi_serialiser_init(struct dvi_serialiser_cfg *cfg);
void dvi_serialiser_enable(struct dvi_serialiser_cfg *cfg, bool enable);
uint32_t dvi_single_to_diff(uint32_t in);

#include "hardware/dma.h"
#include "pico/util/queue.h"

// (merged into this header)
// (merged into this header)

struct dvi_timing {
	bool h_sync_polarity;
	uint h_front_porch;
	uint h_sync_width;
	uint h_back_porch;
	uint h_active_pixels;

	bool v_sync_polarity;
	uint v_front_porch;
	uint v_sync_width;
	uint v_back_porch;
	uint v_active_lines;

	uint bit_clk_khz;
};

enum dvi_line_state {
	DVI_STATE_FRONT_PORCH = 0,
	DVI_STATE_SYNC,
	DVI_STATE_BACK_PORCH,
	DVI_STATE_ACTIVE,
	DVI_STATE_COUNT
};

struct dvi_timing_state {
	uint v_ctr;
	enum dvi_line_state v_state;
};

// This should map directly to DMA register layout, but more convenient types
// (also this really shouldn't be here... we don't have a dma_cb in the SDK
// because there are many valid formats due to aliases)
typedef struct dma_cb {
	const void *read_addr;
	void *write_addr;
	uint32_t transfer_count;
	dma_channel_config c;
} dma_cb_t;

static_assert(sizeof(dma_cb_t) == 4 * sizeof(uint32_t), "bad dma layout");
static_assert(__builtin_offsetof(dma_cb_t, c.ctrl) == __builtin_offsetof(dma_channel_hw_t, ctrl_trig), "bad dma layout");

#define DVI_SYNC_LANE_CHUNKS DVI_STATE_COUNT
// PIZERO-30: lanes 1/2 split their blanking into [fp+sync][bp][active] (was
// [fp+sync+bp][active]) so the back porch can be repointed per-line at a small
// bp-only audio-island buffer -- frees the RAM that per-line full-blanking
// buffers ate. l*[0]=fp+sync, l*[1]=bp, l*[2]=active data.
#define DVI_NOSYNC_LANE_CHUNKS 3

struct dvi_scanline_dma_list {
	dma_cb_t l0[DVI_SYNC_LANE_CHUNKS];
	dma_cb_t l1[DVI_NOSYNC_LANE_CHUNKS];
	dma_cb_t l2[DVI_NOSYNC_LANE_CHUNKS];
};

static inline dma_cb_t* dvi_lane_from_list(struct dvi_scanline_dma_list *l, int i) {
	return i == 0 ? l->l0 : i == 1 ? l->l1 : l->l2;
}

// Each TMDS lane uses one DMA channel to transfer data to a PIO state
// machine, and another channel to load control blocks into this channel.
struct dvi_lane_dma_cfg {
	uint chan_ctrl;
	uint chan_data;
	void *tx_fifo;
	uint dreq;
};

// Note these are already converted to pseudo-differential representation
extern const uint32_t dvi_ctrl_syms[4];

extern const struct dvi_timing dvi_timing_640x480p_60hz;
extern const struct dvi_timing dvi_timing_800x480p_60hz;
extern const struct dvi_timing dvi_timing_800x600p_60hz;
extern const struct dvi_timing dvi_timing_960x540p_60hz;
extern const struct dvi_timing dvi_timing_1280x720p_30hz;

extern const struct dvi_timing dvi_timing_800x600p_reduced_60hz;
extern const struct dvi_timing dvi_timing_1280x720p_reduced_30hz;

void dvi_timing_state_init(struct dvi_timing_state *t);

void dvi_timing_state_advance(const struct dvi_timing *t, struct dvi_timing_state *s);

void dvi_scanline_dma_list_init(struct dvi_scanline_dma_list *dma_list);

void dvi_setup_scanline_for_vblank(const struct dvi_timing *t, const struct dvi_lane_dma_cfg dma_cfg[],
		bool vsync_asserted, struct dvi_scanline_dma_list *l);

void dvi_setup_scanline_for_active(const struct dvi_timing *t, const struct dvi_lane_dma_cfg dma_cfg[],
		uint32_t *tmdsbuf, struct dvi_scanline_dma_list *l);

void dvi_update_scanline_data_dma(const struct dvi_timing *t, const uint32_t *tmdsbuf, struct dvi_scanline_dma_list *l);

// PIZERO-28 (M2): build a vblank scanline carrying one HDMI data island, using
// three caller-provided per-lane buffers of (h_active / DVI_SYMBOLS_PER_WORD)
// words each. Repoints the list's active blocks at those buffers.
void dvi_setup_scanline_for_vblank_island(const struct dvi_timing *t,
		const struct dvi_lane_dma_cfg dma_cfg[], bool vsync_asserted,
		struct dvi_scanline_dma_list *l, const dvi_data_packet_t *pkts, int npkts,
		uint32_t *buf0, uint32_t *buf1, uint32_t *buf2);

// PIZERO-29 (M3): add the HDMI video preamble + guard band to active lines.
// bp0 needs h_back_porch/DVI_SYMBOLS_PER_WORD words; bk1/bk2 need
// (h_front_porch+h_sync_width+h_back_porch)/DVI_SYMBOLS_PER_WORD words.
void dvi_setup_active_hdmi_framing(const struct dvi_timing *t,
		const struct dvi_lane_dma_cfg dma_cfg[], struct dvi_scanline_dma_list *l,
		uint32_t *bp0, uint32_t *bk1, uint32_t *bk2);

// PIZERO-30 (M2): rewrite one data-island period at word offset `word_off` in
// the three per-lane buffers (preamble + guard + TERC4 island). Used by the
// core-0 per-frame audio refill. Runs on core 0 only (NOT the DMA IRQ).
void dvi_write_audio_island(const struct dvi_timing *t, bool vsync_asserted,
		uint32_t *b0, uint32_t *b1, uint32_t *b2, int word_off,
		const dvi_data_packet_t *pkt);

// PIZERO-30 (M2 active-line audio): build one active scanline's blanking buffers
// with `npkts` audio sample packets + the HDMI video preamble/guard, and repoint
// the active dma_list's back-porch (lane0 l0[2]) and full-blanking (lanes 1/2
// l1[0]/l2[0]) blocks at them. la0 needs h_bp/2 words; la1/la2 need
// (h_fp+h_sync+h_bp)/2 words. Core 0 rewrites the audio data words per frame at
// la0[5+16*k] (header) and la1/la2[(h_fp+h_sync)/2 + 5 + 16*k] (subpackets).
void dvi_setup_active_audio_line(const struct dvi_timing *t,
		const struct dvi_lane_dma_cfg dma_cfg[], struct dvi_scanline_dma_list *l,
		uint32_t *la0, uint32_t *la1, uint32_t *la2,
		const dvi_data_packet_t *pkts, int npkts, bool video_follows);

#ifdef __cplusplus
extern "C" {
#endif

#define N_TMDS_LANES 3
#define TMDS_SYNC_LANE 0 // blue!

#include "pico/util/queue.h"

// (merged into this header)
// (merged into this header)
// (merged into this header)
// (merged into this header)

typedef void (*dvi_callback_t)(void);

struct dvi_inst {
	// Config ---
	const struct dvi_timing *timing;
	struct dvi_lane_dma_cfg dma_cfg[N_TMDS_LANES];
	struct dvi_timing_state timing_state;
	struct dvi_serialiser_cfg ser_cfg;
	// Called in the DMA IRQ once per scanline -- careful with the run time!
	dvi_callback_t scanline_callback;

	// PIZERO-30: called in the DMA IRQ on each vertical-blank (no-sync) line,
	// before that line's DMA list is loaded. Lets the HDMI audio engine repoint
	// the data-island read_addrs per line. Runs in IRQ context -- keep it CHEAP
	// (no TERC4/BCH encoding here; that disrupts scanout -- see Option A).
	dvi_callback_t vblank_callback;

	// PIZERO-30: called in the DMA IRQ on each ACTIVE scanline, BEFORE the
	// active DMA list is loaded. Lets the engine repoint the back-porch /
	// blanking blocks at a per-line audio-island buffer so audio is distributed
	// across the visible frame. Pointer stores only -- no encoding in the IRQ.
	dvi_callback_t active_line_callback;

	// State ---
	struct dvi_scanline_dma_list dma_list_vblank_sync;
	struct dvi_scanline_dma_list dma_list_vblank_nosync;
	struct dvi_scanline_dma_list dma_list_active;
	struct dvi_scanline_dma_list dma_list_error;

	// After a TMDS buffer has been enqueue via a control block for the last
	// time, two IRQs must go by before freeing. The first indicates the control
	// block for this buf has been loaded, and the second occurs some time after
	// the actual data DMA transfer has completed.
	uint32_t *tmds_buf_release_next;
	uint32_t *tmds_buf_release;
	// Remember how far behind the source is on TMDS scanlines, so we can output
	// solid colour until they catch up (rather than dying spectacularly)
	uint late_scanline_ctr;

	// Encoded scanlines:
	queue_t q_tmds_valid;
	queue_t q_tmds_free;

	// Either scanline buffers or frame buffers:
	queue_t q_colour_valid;
	queue_t q_colour_free;

};

// Set up data structures and hardware for DVI.
void dvi_init(struct dvi_inst *inst, uint spinlock_tmds_queue, uint spinlock_colour_queue);

// Call this after calling dvi_init(). DVI DMA interrupts will be routed to
// whichever core called this function. Registers an exclusive IRQ handler.
void dvi_register_irqs_this_core(struct dvi_inst *inst, uint irq_num);

// Start actually wiggling TMDS pairs. Call this once you have initialised the
// DVI, have registered the IRQs, and are producing rendered scanlines.
void dvi_start(struct dvi_inst *inst);

// TMDS encode worker function: core enters and doesn't leave, but still
// responds to IRQs. Repeatedly pop a scanline buffer from q_colour_valid,
// TMDS encode it, and pass it to the tmds valid queue.
void dvi_scanbuf_main_8bpp(struct dvi_inst *inst);
void dvi_scanbuf_main_16bpp(struct dvi_inst *inst);

// Same as above, but each q_colour_valid entry is a framebuffer
void dvi_framebuf_main_8bpp(struct dvi_inst *inst);
void dvi_framebuf_main_16bpp(struct dvi_inst *inst);

// PIZERO addition: encode a fixed RGB565 framebuffer forever, ignoring the
// colour queues entirely. Row stride is timing h_active/DVI_SYMBOLS_PER_WORD
// (320 for the 320x240->640x480 doubled mode). The producer core rewrites the
// framebuffer asynchronously; this never starves (tearing instead of dropouts).
void dvi_static_framebuf_main_16bpp(struct dvi_inst *inst, const uint16_t * volatile *framebuf_ptr);

// PIZERO-175: true encodes 16bpp scanlines on the RP2350's SIO TMDS encoder.
extern volatile bool dvi_sio_tmds_encode;

#ifdef __cplusplus
}
#endif

// ----------------------------------------------------------------------------
// Generated PIO programs (pioasm output, merged: dvi_serialiser.pio.h +
// tmds_encode_1bpp.pio.h) and TMDS lookup tables (tmds_table*.h)
#include "dvi_pio_merged.h"

// ----------------------------------------------------------------------------
// Board pin configs (merged from common_dvi_pin_configs.h).
// Waveshare RP2350-PiZero: HDMI TMDS pairs on GPIO 32..37, clock pair 38/39.

#ifndef DVI_DEFAULT_SERIAL_CONFIG
#define DVI_DEFAULT_SERIAL_CONFIG waveshare_rp2350_pizero_cfg
#endif

#ifndef DVI_DEFAULT_PIO_INST
#define DVI_DEFAULT_PIO_INST pio0
#endif

static const struct dvi_serialiser_cfg waveshare_rp2350_pizero_cfg = {
        .pio = DVI_DEFAULT_PIO_INST,
        .sm_tmds = {0, 1, 2},
        .pins_tmds = {36, 34, 32},   // {blue, green, red} data pins (clock is data+1)
        .pins_clk = 38,              // clock pair 38/39
        .invert_diffpairs = false
};

#endif // _DVI_CORE_H
