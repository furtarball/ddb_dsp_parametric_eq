#ifndef PARSER_H
#define PARSER_H
#include <string>
#include <vector>

class FilterConfig {
	// returns 1 on success, 0 on error
	bool parse(const std::string& path);
	std::vector<std::string> types;
	std::vector<std::vector<std::string>> _argv;
	std::vector<std::vector<char*>> _c_argv;

	public:
	int n; // number of filters stated in config file
	// necessary for sox filter setup
	const char* type(size_t n) { return types[n].c_str(); }
	size_t argc(size_t n) { return _argv[n].size(); }
	char* const* argv(size_t n) { return _c_argv[n].data(); }
	FilterConfig(const std::string& path);
};

#endif
