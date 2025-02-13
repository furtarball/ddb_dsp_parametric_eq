#include "parser.h"
#include <assert.h>
#include <deadbeef/deadbeef.h>
#include <iostream>
#include <limits>
#include <memory>
#include <sox.h>
#include <stdlib.h>
#include <string.h>
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
constexpr int num_params(void) { return DDB_DSP_PARAMETRIC_EQ_PARAM_COUNT; }
const char* get_param_name(int p);
void set_param(ddb_dsp_context_t* ctx, int p, const char* val);
void get_param(ddb_dsp_context_t* ctx, int p, char* val, int sz);

/* this plugin's api object, defined at the end, d.h line 2091 */
DB_dsp_t plugin{
	.plugin{
		.type = DB_PLUGIN_DSP,
		.api_vmajor = DB_API_VERSION_MAJOR,
		.api_vminor = DB_API_VERSION_MINOR,
		.version_major = 0,
		.version_minor = 1,
		.id = "ddb_dsp_parametric_eq",
		.name = "Parametric equalizer (libsox)",
		.descr = "Parametric equalizer based on libsox",
		.copyright =
			"BSD 2-Clause License\n\n"
			"Copyright (c) 2025, furtarball\n"
			"All rights reserved.\n\n"
			"Redistribution and use in source and binary forms, with or "
			"without\n"
			"modification, are permitted provided that the following "
			"conditions "
			"are met:\n\n"
			"1. Redistributions of source code must retain the above copyright "
			"notice, this\n"
			"list of conditions and the following disclaimer.\n\n"
			"2. Redistributions in binary form must reproduce the above "
			"copyright "
			"notice,\n"
			"this list of conditions and the following disclaimer in the "
			"documentation\n"
			"and/or other materials provided with the distribution.\n\n"
			"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND "
			"CONTRIBUTORS "
			"\"AS IS\"\n"
			"AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED "
			"TO, "
			"THE\n"
			"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A "
			"PARTICULAR "
			"PURPOSE ARE\n"
			"DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR "
			"CONTRIBUTORS BE "
			"LIABLE\n"
			"FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR "
			"CONSEQUENTIAL\n"
			"DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE "
			"GOODS OR\n"
			"SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS "
			"INTERRUPTION) "
			"HOWEVER\n"
			"CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, "
			"STRICT "
			"LIABILITY,\n"
			"OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY "
			"OUT OF "
			"THE USE\n"
			"OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH "
			"DAMAGE.\n"},
	.open = open,
	.close = close,
	.process = process,
	.reset = reset,
	.num_params = num_params,
	.get_param_name = get_param_name,
	.set_param = set_param,
	.get_param = get_param,
	/* https://github.com/DeaDBeeF-Player/deadbeef/wiki/GUI-Script-Syntax */
	.configdialog = "property \"Configuration file\" file 0 \"\";"};

static DB_functions_t* deadbeef; /* program's api object, deadbeef.h line 828 */
struct Context {
	ddb_dsp_context_t ctx;
	// instance-specific variables here
	std::string path;
	std::vector<std::unique_ptr<sox_effect_t>> filters;
	ddb_waveformat_t fmt_old;
	FilterConfig conf;
	std::vector<sox_sample_t> ibuf, obuf;
};

static bool sox_initialized = 0;

ddb_dsp_context_t* open() {
	Context* plugin_ctx = new Context{};
	DDB_INIT_DSP_CONTEXT(plugin_ctx, Context, &plugin);

	if (!sox_initialized) {
		if (sox_init() == SOX_SUCCESS)
			sox_initialized = 1;
		else {
			delete plugin_ctx;
			return nullptr;
		}
	}
	return &(plugin_ctx->ctx);
}

void reset(ddb_dsp_context_t* ctx) {
	auto plugin_ctx = reinterpret_cast<Context*>(ctx);
	// use this method to flush dsp buffers, reset filters, etc
	if (plugin_ctx->filters.size()) {
		plugin_ctx->filters.clear();
	}
}

void close(ddb_dsp_context_t* ctx) {
	auto plugin_ctx = reinterpret_cast<Context*>(ctx);

	// free instance-specific allocations
	reset(ctx);
	if (sox_initialized)
		sox_quit();
	sox_initialized = 0;
	delete plugin_ctx;
}

/* ddb_waveformat_t line 674 */
int process(ddb_dsp_context_t* ctx, float* samples, int nframes, int maxframes,
			ddb_waveformat_t* fmt, float* r) {
	auto plugin = reinterpret_cast<Context*>(ctx);
	if (plugin->filters.size() &&
		((plugin->fmt_old.samplerate != fmt->samplerate) ||
		 (plugin->fmt_old.channels != fmt->channels))) {
		reset(ctx);
	}
	plugin->fmt_old = *fmt;
	if (plugin->filters.size() == 0) {
		try {
			plugin->conf = FilterConfig(plugin->path);
		} catch (std::runtime_error& e) {
			std::cerr << e.what() << std::endl;
		}
		for (size_t i = 0; i < (fmt->channels * plugin->conf.n); i++) {
			auto new_filter = sox_create_effect(sox_find_effect(
				plugin->conf.types[i % plugin->conf.n].c_str()));
			if (sox_effect_options(
					new_filter, plugin->conf.argv_c[i % plugin->conf.n].size(),
					plugin->conf.argv_c[i % plugin->conf.n].data()) !=
				SOX_SUCCESS) {
				reset(ctx);
				return 0;
			}
			new_filter->clips = 0;
			new_filter->global_info->plot = sox_plot_off;
			new_filter->in_signal = sox_signalinfo_t{
				fmt->samplerate, 1, fmt->bps,
				std::numeric_limits<sox_uint64_t>::max(), nullptr};
			// ...::max() is equivalent to -1 which means undefined length
			new_filter->handler.start(new_filter);
			plugin->filters.emplace_back(new_filter);
		}
	}

	auto& ibuf = plugin->ibuf;
	auto& obuf = plugin->obuf;
	ibuf.resize(nframes * fmt->channels);
	obuf.resize(nframes * fmt->channels);

	size_t clips = 0;
	size_t samp = nframes;
	SOX_SAMPLE_LOCALS; // necessary for using sox macros

	for (size_t i = 0; i < nframes; i++)
		for (size_t j = 0; j < fmt->channels; j++)
			ibuf[(j * nframes) + i] = SOX_FLOAT_32BIT_TO_SAMPLE(
				samples[(i * fmt->channels) + j], clips);

	for (size_t i = 0; i < (plugin->conf.n * fmt->channels); i++) {
		size_t offs = nframes * (i / plugin->conf.n);

		plugin->filters[i].get()->handler.flow(
			plugin->filters[i].get(), ibuf.data() + offs, obuf.data() + offs,
			&samp, &samp);

		std::copy(obuf.begin() + offs, obuf.begin() + offs + nframes,
				  ibuf.begin() + offs);
	}

	for (size_t i = 0; i < fmt->channels; i++)
		for (size_t j = 0; j < nframes; j++)
			samples[(j * fmt->channels) + i] =
				SOX_SAMPLE_TO_FLOAT_32BIT(obuf[(i * nframes) + j], clips);

	return nframes;
}

const char* get_param_name(int p) {
	if (p != DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH) {
		std::cerr
			<< "ddb_dsp_parametric_eq: get_param_name: invalid param index ("
			<< p << ")" << std::endl;
		return nullptr;
	}
	return "Configuration file"; // the only option
}

void set_param(ddb_dsp_context_t* ctx, int p, const char* val) {
	auto plugin_ctx = reinterpret_cast<Context*>(ctx);
	if (p != DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH) {
		std::cerr << "ddb_dsp_parametric_eq: set_param: invalid param index ("
				  << p << ")" << std::endl;
		return;
	}
	plugin_ctx->path = std::string{val};
}

void get_param(ddb_dsp_context_t* ctx, int p, char* val, int sz) {
	auto plugin_ctx = reinterpret_cast<Context*>(ctx);
	if (p != DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH) {
		std::cerr << "ddb_dsp_parametric_eq: get_param: invalid param index ("
				  << p << ")" << std::endl;
		return;
	}
	snprintf(val, sz, "%s", plugin_ctx->path.c_str());
}
} // anonymous namespace

extern "C" {
DB_plugin_t* ddb_dsp_parametric_eq_load(
	DB_functions_t* f) { /* entry point for the program */
	deadbeef = f;
	return &plugin.plugin;
}
}
