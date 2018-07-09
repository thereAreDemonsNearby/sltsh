#ifndef PARSE_H__
#define PARSE_H__

#include <utility>
#include <memory>
#include "nodes.h"

enum class ParseErr
{
	Ok, MissRightParen, ExpectNumber, UnpairedDoubleQuotationMark,
    UnpairedSingleQuotationMark, FdOutOfRange, NotSingular, InvalidRedirection,
    EmptyArgvList,
	NotBgable, EmptyCmd,
};

using ParseResult = std::pair<std::unique_ptr<NodeBase>, ParseErr>;

ParseResult parseCmdLine(char const*);

ParseResult parsePrimary(char const*&);

ParseResult parseRedirected(char const*&);

ParseResult parsePipe(char const*&);

ParseResult parseList(char const*&);

#endif
