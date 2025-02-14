#include "parser.h"
#include <assert.h>
#include <deadbeef/deadbeef.h>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sox.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <vector>

namespace {
enum {
	DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH,
	DDB_DSP_PARAMETRIC_EQ_PARAM_COUNT
};
ddb_dsp_context_t* open();
void close(ddb_dsp_context_t* ctx);
int process(ddb_dsp_context_t* ctx, float* samples, int nframes, int maxframes,
			ddb_waveformat_t* fmt, float* r);
void reset(ddb_dsp_context_t* ctx);
int num_params(void) { return DDB_DSP_PARAMETRIC_EQ_PARAM_COUNT; }
const char* get_param_name(int p);
void set_param(ddb_dsp_context_t* ctx, int p, const char* val);
void get_param(ddb_dsp_context_t* ctx, int p, char* val, int sz);

DB_dsp_t plugin{
	.plugin{
		.type = DB_PLUGIN_DSP,
		.api_vmajor = DB_API_VERSION_MAJOR,
		.api_vminor = DB_API_VERSION_MINOR,
		.version_major = 0,
		.version_minor = 2,
		.id = "ddb_dsp_parametric_eq",
		.name = "Parametric equalizer (libsox)",
		.descr =
			"Parametric equalizer based on libsox. Mostly compatible with\n"
			"Equalizer APO. Config file may be set in DSP settings.\n",
		.copyright =
			"BSD 2-Clause License\n\n"
			"Copyright 2025 furtarball\n\n"
			"Redistribution and use in source and binary forms, with or\n"
			"without modification, are permitted provided that the following\n"
			"conditions are met:\n"
			"1. Redistributions of source code must retain the above\n"
			"copyright notice, this list of conditions and the following\n"
			"disclaimer.\n"
			"2. Redistributions in binary form must reproduce the above\n"
			"copyright notice, this list of conditions and the following\n"
			"disclaimer in the documentation and/or other materials provided\n"
			"with the distribution.\n"
			"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND\n"
			"CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,\n"
			"INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\n"
			"MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"
			"DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR\n"
			"CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
			"SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT\n"
			"NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n"
			"LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n"
			"HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n"
			"CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR\n"
			"OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,\n"
			"EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."},
	.open = open,
	.close = close,
	.process = process,
	.reset = reset,
	.num_params = num_params,
	.get_param_name = get_param_name,
	.set_param = set_param,
	.get_param = get_param,
	/* https://github.com/DeaDBeeF-Player/deadbeef/wiki/GUI-Script-Syntax */
	.configdialog = "property \"Preset file\" file 0 \"\";"};

DB_functions_t* deadbeef; /* program's api object, deadbeef.h line 828 */
struct Context {
	ddb_dsp_context_t ctx;
	// instance-specific variables here
	std::string path;
	std::vector<std::unique_ptr<sox_effect_t>> filters;
	ddb_waveformat_t fmt_old; // format change detection
	std::unique_ptr<FilterConfig> conf;
	std::vector<sox_sample_t> ibuf, obuf;
	Context() : ctx() {
		ctx.plugin = &plugin;
		ctx.enabled = 1;
	}
};

std::unordered_map<ddb_dsp_context_t*, std::unique_ptr<Context>> ctxmap;
bool sox_initialized = false;

ddb_dsp_context_t* open() {
	auto newctx = std::make_unique<Context>();
	auto ctx = &(newctx->ctx);
	ctxmap[ctx] = std::move(newctx);
	return ctx;
}

void reset(ddb_dsp_context_t* ctx) {
	if (sox_initialized) {
		sox_quit();
		sox_initialized = false;
	}
	sox_init();
	sox_initialized = true;
	ctxmap[ctx]->filters.clear();
}

void close(ddb_dsp_context_t* ctx) {
	ctxmap.erase(ctx); // hopefully won't introduce new segfaults
}

bool init(ddb_dsp_context_t* ctx, ddb_waveformat_t* fmt) {
	try {
		ctxmap[ctx]->conf = std::make_unique<FilterConfig>(ctxmap[ctx]->path);
	} catch (std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return false;
	}
	auto& conf = *(ctxmap[ctx]->conf);
	for (int i = 0; i < (fmt->channels * conf.n); i++) {
		auto n = i % conf.n;
		auto newf = sox_create_effect(sox_find_effect(conf.type(n)));
		if (sox_effect_options(newf, conf.argc(n), conf.argv(n)) !=
			SOX_SUCCESS) {
			reset(ctx);
			return 0;
		}
		newf->clips = 0;
		newf->global_info->plot = sox_plot_off;
		newf->in_signal =
			sox_signalinfo_t{static_cast<sox_rate_t>(fmt->samplerate), 1,
							 static_cast<unsigned>(fmt->bps),
							 std::numeric_limits<sox_uint64_t>::max(), nullptr};
		// hopefully users won't listen to music in negative bitdepths
		// ...::max() is equivalent to -1, which means undefined length
		newf->handler.start(newf);
		ctxmap[ctx]->filters.emplace_back(newf);
	}
	return true;
}

bool operator!=(const ddb_waveformat_t& a, const ddb_waveformat_t& b) {
	return (a.channels != b.channels) || (a.samplerate != b.samplerate);
}

/* ddb_waveformat_t line 674 */
int process(ddb_dsp_context_t* ctx, float* samples, int nframes, int maxframes,
			ddb_waveformat_t* fmt, float* r) {
	if (ctxmap[ctx]->filters.size() && (ctxmap[ctx]->fmt_old != *fmt)) {
		reset(ctx);
	}
	ctxmap[ctx]->fmt_old = *fmt;
	if (ctxmap[ctx]->filters.size() == 0) {
		if (init(ctx, fmt) == false)
			return 0;
	}
	auto& ibuf = ctxmap[ctx]->ibuf;
	auto& obuf = ctxmap[ctx]->obuf;
	ibuf.resize(nframes * fmt->channels);
	obuf.resize(nframes * fmt->channels);

	size_t clips = 0;
	size_t samp = nframes;
	SOX_SAMPLE_LOCALS; // necessary for using sox macros
	// convert from frame-sequential (ddb) to channel-sequential (sox)
	for (int i = 0; i < nframes; i++)
		for (int j = 0; j < fmt->channels; j++)
			ibuf[(j * nframes) + i] = SOX_FLOAT_32BIT_TO_SAMPLE(
				samples[(i * fmt->channels) + j], clips);
	// let each filter do its thing
	for (size_t i = 0; i < ctxmap[ctx]->filters.size(); i++) {
		// offset to access samples for the correct channel
		size_t offs = nframes * (i / ctxmap[ctx]->conf->n);
		auto f = ctxmap[ctx]->filters[i].get();
		f->handler.flow(f, ibuf.data() + offs, obuf.data() + offs, &samp,
						&samp);
		// move data back for the next filter to take care of
		if ((i % ctxmap[ctx]->conf->n) < (ctxmap[ctx]->conf->n - 1lu)) {
			std::copy(obuf.begin() + offs, obuf.begin() + offs + nframes,
					  ibuf.begin() + offs);
		}
	}
	// re-convert to output format
	for (int i = 0; i < fmt->channels; i++)
		for (int j = 0; j < nframes; j++)
			samples[(j * fmt->channels) + i] =
				SOX_SAMPLE_TO_FLOAT_32BIT(obuf[(i * nframes) + j], clips);

	return nframes;
}

const char* get_param_name(int p) {
	if (p != DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH) {
		std::cerr << "ddb_dsp_parametric_eq: get_param_name: invalid param "
					 "index ("
				  << p << ")" << std::endl;
		return nullptr;
	}
	return "Configuration file"; // the only option
}

void set_param(ddb_dsp_context_t* ctx, int p, const char* val) {
	if (p != DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH) {
		std::cerr << "ddb_dsp_parametric_eq: set_param: invalid param index ("
				  << p << ")" << std::endl;
		return;
	}
	ctxmap[ctx]->path = val;
}

void get_param(ddb_dsp_context_t* ctx, int p, char* val, int sz) {
	if (p != DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH) {
		std::cerr << "ddb_dsp_parametric_eq: get_param: invalid param index ("
				  << p << ")" << std::endl;
		return;
	}
	snprintf(val, sz, "%s", ctxmap[ctx]->path.c_str());
}
} // anonymous namespace

extern "C" {
DB_plugin_t* ddb_dsp_parametric_eq_load(
	DB_functions_t* f) { /* entry point for the program */
	deadbeef = f;
	return &plugin.plugin;
}
}
