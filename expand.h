#ifndef EXPAND_H__
#define EXPAND_H__

#include <string>
#include <utility>

enum class ExpandError
{
	Ok, Error,
};

std::pair<std::string, ExpandError>
expand(const std::string&);

#endif
