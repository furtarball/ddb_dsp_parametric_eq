#ifndef PARSER_H
#define PARSER_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

  typedef struct {
    size_t n;
    char** types;
    size_t* argc;
    char*** argv;
  } FilterConfig;

  void filterconfig_destroy(FilterConfig* fc);

  // returns 1 on success, 0 on error
  int parse(const char* path, FilterConfig* conf);
  
#ifdef __cplusplus
}
#endif
#endif
