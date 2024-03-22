#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sox/sox.h>
#include <deadbeef/deadbeef.h>
#include "parser.h"

enum {
  DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH,
  DDB_DSP_PARAMETRIC_EQ_PARAM_COUNT
};

static DB_functions_t *deadbeef; /* program's api object, deadbeef.h line 828 */
static DB_dsp_t plugin; /* this plugin's api object, defined at the end, d.h line 2091 */
typedef struct {
  ddb_dsp_context_t ctx;
  // instance-specific variables here
  char path[256];
  sox_effect_t** filters;
  ddb_waveformat_t fmt_old;
  FilterConfig conf;
} ddb_dsp_parametric_eq_t;

static _Bool sox_initialized = 0, effects_initialized = 0;

static void
set_param (const char *key, const char *value)
{
    return;
}

static void
get_param (const char *key, char *value, int len, const char *def)
{
    return;
}

static int
action_edit (DB_plugin_action_t *act, ddb_action_context_t ctx) {
  ddb_dialog_t conf = {
    .title = "EQ Configuration Editor",
    .layout = "property \"New filters ([OK] to create)\" hscale[0,15,1] n 1;"\
    "property \"File ([OK] to open/save)\" file f \"\";"\
    "property \"Preamp [dB]\" hscale[-12,0,0.1] p 0;"\
    "property \"Filter 1\" select[9] t1 0 Peak Lowshelf Highshelf Lowpass Highpass Bandpass Notch Allpass \"Biquad (2nd order)\";"\
    "property \"Filter 1 settings\" entry f1 \"100 -2.0 1q\";",
    .set_param = set_param,
    .get_param = get_param,
    .parent = NULL
  };

  struct DB_plugin_s **plugin_list;
  for (plugin_list = deadbeef->plug_get_list(); *plugin_list && (*plugin_list)->type != DB_PLUGIN_GUI; plugin_list++);
  if (*plugin_list) {
    DB_gui_t *gui_plugin = (DB_gui_t *)*plugin_list;
    if (gui_plugin->run_dialog(&conf, 1<<ddb_button_ok|1<<ddb_button_cancel, NULL, NULL) == ddb_button_ok) {
      
    }
  }
  return 0;
}

static DB_plugin_action_t add_cd_action = {
  .name = "eq_conf_gui",
  .title = "Edit/Edit parametric EQ configuration",
  .flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
  .callback2 = action_edit,
  .next = NULL
};

static DB_plugin_action_t* get_actions(DB_playItem_t*) {
  return &add_cd_action;
}

ddb_dsp_context_t*
ddb_dsp_parametric_eq_open (void) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = malloc (sizeof (ddb_dsp_parametric_eq_t));
  DDB_INIT_DSP_CONTEXT (ddb_dsp_parametric_eq,ddb_dsp_parametric_eq_t,&plugin);

  // initialize
  memset(ddb_dsp_parametric_eq->path, 0, 256);
  ddb_dsp_parametric_eq->fmt_old = (ddb_waveformat_t){ 0 };
    
  if(!sox_initialized)
    assert(sox_init() == SOX_SUCCESS);
  sox_initialized = 1;
  return (ddb_dsp_context_t *)ddb_dsp_parametric_eq;
}

void
ddb_dsp_parametric_eq_reset (ddb_dsp_context_t *ctx) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = (ddb_dsp_parametric_eq_t *)ctx;
  // use this method to flush dsp buffers, reset filters, etc
  if(effects_initialized) {
    for(size_t i = 0; i < ddb_dsp_parametric_eq->fmt_old.channels * ddb_dsp_parametric_eq->conf.n; i++)
      free(ddb_dsp_parametric_eq->filters[i]);
    filterconfig_destroy(&(ddb_dsp_parametric_eq->conf));
  }
  effects_initialized = 0;
}

void
ddb_dsp_parametric_eq_close (ddb_dsp_context_t *ctx) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = (ddb_dsp_parametric_eq_t *)ctx;

  // free instance-specific allocations
  ddb_dsp_parametric_eq_reset(ctx);
  if(sox_initialized)
    sox_quit();
  sox_initialized = 0;
  free (ddb_dsp_parametric_eq);
}

/* ddb_waveformat_t line 674 */
int
ddb_dsp_parametric_eq_process (ddb_dsp_context_t *ctx, float *samples, int nframes, int maxframes, ddb_waveformat_t *fmt, float *r) {
  ddb_dsp_parametric_eq_t *plugin = (ddb_dsp_parametric_eq_t *)ctx;
  if(effects_initialized && ((plugin->fmt_old.samplerate != fmt->samplerate) || (plugin->fmt_old.channels != fmt->channels))) {
    ddb_dsp_parametric_eq_reset(ctx);
    effects_initialized = 0;
  }
  plugin->fmt_old = *fmt;
  if(!effects_initialized) {
    if(parse(plugin->path, &(plugin->conf)) == 0) return 0;
    plugin->filters = malloc(sizeof(sox_effect_t*) * plugin->conf.n * fmt->channels);
    for(size_t i = 0; i < (fmt->channels * plugin->conf.n); i++) {
      plugin->filters[i] = sox_create_effect(sox_find_effect(plugin->conf.types[i % plugin->conf.n]));
      assert(sox_effect_options(plugin->filters[i], plugin->conf.argc[i % plugin->conf.n], plugin->conf.argv[i % plugin->conf.n]) == SOX_SUCCESS);
      plugin->filters[i]->clips = 0; plugin->filters[i]->global_info->plot = sox_plot_off;
      plugin->filters[i]->in_signal = (struct sox_signalinfo_t){ fmt->samplerate, 1, fmt->bps, -1, NULL };
      plugin->filters[i]->handler.start(plugin->filters[i]);
    }
    effects_initialized = 1;
  }

  size_t clips = 0;
  sox_sample_t ibuf[nframes * fmt->channels], obuf[nframes * fmt->channels];

  SOX_SAMPLE_LOCALS;
  
  for(size_t i = 0; i < nframes; i++)
    for(size_t j = 0; j < fmt->channels; j++)
      ibuf[(j * nframes) + i] = SOX_FLOAT_32BIT_TO_SAMPLE(samples[(i * fmt->channels) + j], clips);

  size_t samp = nframes;

  for(size_t i = 0; i < (plugin->conf.n * fmt->channels); i++) {
    size_t offs = nframes * (i / plugin->conf.n);
    
    plugin->filters[i]->handler.flow(plugin->filters[i], ibuf + offs, obuf + offs, &samp, &samp);
    memcpy(ibuf + offs, obuf + offs, nframes * sizeof(sox_sample_t));
  }
  
  for(size_t i = 0; i < fmt->channels; i++)
    for(size_t j = 0; j < nframes; j++)
      samples[(j * fmt->channels) + i] = SOX_SAMPLE_TO_FLOAT_32BIT(obuf[(i * nframes) + j], clips);

  return nframes;
}

const char *
ddb_dsp_parametric_eq_get_param_name (int p) {
  switch (p) {
  case DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH:
    return "Configuration file";
  default:
    fprintf (stderr, "ddb_dsp_parametric_eq_param_name: invalid param index (%d)\n", p);
  }
  return NULL;
}

int
ddb_dsp_parametric_eq_num_params (void) {
  return DDB_DSP_PARAMETRIC_EQ_PARAM_COUNT;
}

void
ddb_dsp_parametric_eq_set_param (ddb_dsp_context_t *ctx, int p, const char *val) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = (ddb_dsp_parametric_eq_t *)ctx;
  switch (p) {
  case DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH:
    strcpy(ddb_dsp_parametric_eq->path, val);
    break;
  default:
    fprintf (stderr, "ddb_dsp_parametric_eq_param: invalid param index (%d)\n", p);
  }
}

void
ddb_dsp_parametric_eq_get_param (ddb_dsp_context_t *ctx, int p, char *val, int sz) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = (ddb_dsp_parametric_eq_t *)ctx;
  switch (p) {
  case DDB_DSP_PARAMETRIC_EQ_PARAM_CONFIGPATH:
    snprintf (val, sz, "%s", ddb_dsp_parametric_eq->path);
    break;
  default:
    fprintf (stderr, "ddb_dsp_parametric_eq_get_param: invalid param index (%d)\n", p);
  }
}

/* https://github.com/DeaDBeeF-Player/deadbeef/wiki/GUI-Script-Syntax */
static const char settings_dlg[] =
  "property \"Configuration file\" file 0 \"\";";

static DB_dsp_t plugin = {
  .plugin.api_vmajor = DB_API_VERSION_MAJOR,
  .plugin.api_vminor = DB_API_VERSION_MINOR,
  .open = ddb_dsp_parametric_eq_open,
  .close = ddb_dsp_parametric_eq_close,
  .process = ddb_dsp_parametric_eq_process,
  .plugin.version_major = 0,
  .plugin.version_minor = 1,
  .plugin.type = DB_PLUGIN_DSP,
  .plugin.id = "ddb_dsp_parametric_eq",
  .plugin.name = "Parametric equalizer (libsox)",
  .plugin.descr = "Parametric equalizer based on libsox",
  .plugin.copyright = "BSD-2-Clause furtarball.github.io",
  .plugin.website = "https://furtarball.github.io",
  .num_params = ddb_dsp_parametric_eq_num_params,
  .get_param_name = ddb_dsp_parametric_eq_get_param_name,
  .set_param = ddb_dsp_parametric_eq_set_param,
  .get_param = ddb_dsp_parametric_eq_get_param,
  .reset = ddb_dsp_parametric_eq_reset,
  .configdialog = settings_dlg,
  .plugin.get_actions = get_actions,
};

DB_plugin_t *
ddb_dsp_parametric_eq_load (DB_functions_t *f) { /* entry point for the program */
  deadbeef = f;
  return &plugin.plugin;
}

