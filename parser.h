#ifndef PARSER_H
#define PARSER_H
#include <vector>
#include <string>

class FilterConfig {
  // returns 1 on success, 0 on error
  bool parse(const std::string& path);
  public:
    size_t n; // number of filters stated in config file
    std::vector<std::string> types;
    std::vector<std::vector<std::string>> argv;
    // necessary for sox filter setup
    std::vector<std::vector<char*>> argv_c;
    FilterConfig() {}
    FilterConfig(const std::string& path);
};
  
#endif
