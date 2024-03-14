// this is a dsp plugin skeleton/example
// use to create new dsp plugins

// usage:
// 1. copy to your plugin folder
// 2. s/example/plugname/g
// 3. s/EXAMPLE/PLUGNAME/g

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sox/sox.h>
#include <deadbeef/deadbeef.h>

enum {
  DDB_DSP_PARAMETRIC_EQ_PARAM_LEVEL, //TODO
  DDB_DSP_PARAMETRIC_EQ_PARAM_COUNT
};

static DB_functions_t *deadbeef; /* program's api object, deadbeef.h line 828 */
static DB_dsp_t plugin; /* this plugin's api object, defined at the end, d.h line 2091 */
typedef struct {
  ddb_dsp_context_t ctx;
  // instance-specific variables here
  float level; // this is example
  sox_effect_t** filters;
  ddb_waveformat_t fmt_old;
} ddb_dsp_parametric_eq_t;

static _Bool sox_initialized = 0, effects_initialized = 0;
static size_t n_filters = 8;

ddb_dsp_context_t*
ddb_dsp_parametric_eq_open (void) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = malloc (sizeof (ddb_dsp_parametric_eq_t));
  DDB_INIT_DSP_CONTEXT (ddb_dsp_parametric_eq,ddb_dsp_parametric_eq_t,&plugin);

  // initialize
  ddb_dsp_parametric_eq->level = 0.5; //TODO
  ddb_dsp_parametric_eq->fmt_old = (ddb_waveformat_t){ 0 };
    
  puts("ddb_dsp_parametric_eq_open");
  if(!sox_initialized)
    assert(sox_init() == SOX_SUCCESS);
  sox_initialized = 1;
  return (ddb_dsp_context_t *)ddb_dsp_parametric_eq;
}

void
ddb_dsp_parametric_eq_reset (ddb_dsp_context_t *ctx) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = (ddb_dsp_parametric_eq_t *)ctx;
  // use this method to flush dsp buffers, reset filters, etc
  puts("ddb_dsp_parametric_eq_reset");
  if(effects_initialized) {
    for(size_t i = 0; i < ddb_dsp_parametric_eq->fmt_old.channels * n_filters; i++)
      free(ddb_dsp_parametric_eq->filters[i]);
  }
  effects_initialized = 0;
}

void
ddb_dsp_parametric_eq_close (ddb_dsp_context_t *ctx) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = (ddb_dsp_parametric_eq_t *)ctx;

  // free instance-specific allocations
  puts("ddb_dsp_parametric_eq_close");
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
  //puts("ddb_dsp_parametric_eq_process");

  if(effects_initialized && ((plugin->fmt_old.samplerate != fmt->samplerate) || (plugin->fmt_old.channels != fmt->channels))) {
    ddb_dsp_parametric_eq_reset(ctx);
    effects_initialized = 0;
  }
  plugin->fmt_old = *fmt;
  if(!effects_initialized) {
    plugin->filters = malloc(sizeof(sox_effect_t*) * n_filters * fmt->channels);
    char ***argv = malloc(sizeof(char**) * n_filters);
    argv[0] = (char*[]){ "60", "0.91q", "-6.4" };
    argv[1] = (char*[]){ "254", "2.02q", "4.8" };
    argv[2] = (char*[]){ "480", "2.2q", "-0.4" };
    argv[3] = (char*[]){ "2165", "1.16q", "-4.4" };
    argv[4] = (char*[]){ "3473", "5.24q", "1.1" };
    argv[5] = (char*[]){ "4599", "3.70q", "5.1" };
    argv[6] = (char*[]){ "5114", "6q", "1.2" };
    argv[7] = (char*[]){ "6650", "4.63q", "-3.5" };

    for(size_t i = 0; i < (fmt->channels * n_filters); i++) {
      plugin->filters[i] = sox_create_effect(sox_find_effect("equalizer"));
      assert(sox_effect_options(plugin->filters[i], 3, argv[i % n_filters]) == SOX_SUCCESS);
      plugin->filters[i]->clips = 0; plugin->filters[i]->global_info->plot = sox_plot_off;
      plugin->filters[i]->in_signal = (struct sox_signalinfo_t){ fmt->samplerate, 1, fmt->bps, -1, NULL };
      plugin->filters[i]->handler.start(plugin->filters[i]);
    }
  }

  size_t clips = 0;
    
  effects_initialized = 1;
  sox_sample_t ibuf[nframes * fmt->channels], obuf[nframes * fmt->channels];

  SOX_SAMPLE_LOCALS;
  
  for(size_t i = 0; i < nframes; i++)
    for(size_t j = 0; j < fmt->channels; j++)
      ibuf[(j * nframes) + i] = SOX_FLOAT_32BIT_TO_SAMPLE(samples[(i * fmt->channels) + j], clips);

  size_t samp = nframes;

  for(size_t i = 0; i < (n_filters * fmt->channels); i++) {
    size_t offs = nframes * (i / n_filters);
    
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
  case DDB_DSP_PARAMETRIC_EQ_PARAM_LEVEL:
    return "Volume level";
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
  case DDB_DSP_PARAMETRIC_EQ_PARAM_LEVEL:
    ddb_dsp_parametric_eq->level = atof (val);
    break;
  default:
    fprintf (stderr, "ddb_dsp_parametric_eq_param: invalid param index (%d)\n", p);
  }
}

void
ddb_dsp_parametric_eq_get_param (ddb_dsp_context_t *ctx, int p, char *val, int sz) {
  ddb_dsp_parametric_eq_t *ddb_dsp_parametric_eq = (ddb_dsp_parametric_eq_t *)ctx;
  switch (p) {
  case DDB_DSP_PARAMETRIC_EQ_PARAM_LEVEL:
    snprintf (val, sz, "%f", ddb_dsp_parametric_eq->level);
    break;
  default:
    fprintf (stderr, "ddb_dsp_parametric_eq_get_param: invalid param index (%d)\n", p);
  }
}

/* https://github.com/DeaDBeeF-Player/deadbeef/wiki/GUI-Script-Syntax */
static const char settings_dlg[] =
  "property \"Volume Level\" spinbtn[0,2,0.1] 0 0.5;\n"
  /*                name       type with params ↑ default value*/
  /*                              number of controlled value   */
  /*                             according to enum in line 14  */
  /*                              passed to set_param et al.   */
  ;

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
  .plugin.copyright = "BSD0 furtarball.github.io",
  .plugin.website = "https://furtarball.github.io",
  .num_params = ddb_dsp_parametric_eq_num_params,
  .get_param_name = ddb_dsp_parametric_eq_get_param_name,
  .set_param = ddb_dsp_parametric_eq_set_param,
  .get_param = ddb_dsp_parametric_eq_get_param,
  .reset = ddb_dsp_parametric_eq_reset,
  .configdialog = settings_dlg,
};

DB_plugin_t *
ddb_dsp_parametric_eq_load (DB_functions_t *f) { /* entry point for the program */
  deadbeef = f;
  return &plugin.plugin;
}
