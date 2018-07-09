#include <cctype>
#include <vector>
#include <memory>
#include <cassert>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <unistd.h>
#include <exception>
#include "calc_aslib.h"
#include "expand.h"
#include "context.h"
#include "parse.h"

class NotStringifyException : std::exception
{
	
};

enum class NodeType
{
	Root, Plain, Space, WithinParen, SingleQuoted, DoubleQuoted,
	ExprEval, CmdEval, VarEval, HomeExp,
    PidEval, LastExitStatusEval
};

struct ExpandBase;

using ChildList = std::vector<std::unique_ptr<ExpandBase>>;

// from shell.cpp
extern Context& getContext();

struct ExpandBase
{   
	ExpandBase(NodeType t) : type(t) {}
	virtual std::string toString() const = 0;
	NodeType type;
};

struct Root : public ExpandBase
{
	Root() : ExpandBase(NodeType::Root) {}
	ChildList children;
	std::string toString() const override {
		std::string res;
		for (auto& p : children) {
			res.append(p->toString());
		}
		return res;
	}
};

struct Plain : public ExpandBase
{
	Plain(std::string s)
		: ExpandBase(NodeType::Plain), text(std::move(s)) {}
	std::string text;
	std::string toString() const override {
		return text;
	}
};

struct Space : public ExpandBase
{
	Space(std::string s)
		:  ExpandBase(NodeType::Space), spaces(std::move(s)) {}
	std::string spaces;
	std::string toString() const override {
		return spaces;
	}
};

struct WithinParen : public ExpandBase
{
	WithinParen() : ExpandBase(NodeType::WithinParen) {}
	ChildList children;
	std::string toString() const override {
		std::string res("(");
		for (auto& p : children) {
			res.append(p->toString());
		}
		res.append(")");
		return res;
	}
};

struct SingleQuoted : public ExpandBase
{
	SingleQuoted(std::string s)
		: ExpandBase(NodeType::SingleQuoted), quoted(std::move(s)) {}
	std::string quoted;
	std::string toString() const override {
		return std::string("'") + quoted + "'";
	}
};

struct DoubleQuoted : public ExpandBase
{
	DoubleQuoted()
		: ExpandBase(NodeType::DoubleQuoted) {}
	ChildList children;
	std::string toString() const override {
		std::string res = "\"";
		for (auto& p : children) {
			res.append(p->toString());
		}
		return res + "\"";
	};
};

struct ExprEval : public ExpandBase
{
	ExprEval()
		: ExpandBase(NodeType::ExprEval) {}
	ChildList children;
	std::string toString() const override {
		std::string expr;
		for (auto& p : children) {
			expr.append(p->toString());			
		}
		CalcResult res = calc(expr);
		if (res.error) {
			throw NotStringifyException{};
		} else {
			if (res.type == CalcResult::Int) {
				return std::to_string(res.intResult());
			} else {
				return std::to_string(res.doubleResult());
			}
		}
	}
};


extern std::string strCmdOutput(std::unique_ptr<NodeBase>&);
struct CmdEval : public ExpandBase
{
	CmdEval()
		: ExpandBase(NodeType::CmdEval) {}
	ChildList children;
	std::string toString() const override {
		std::string cmd;
		for (auto& i : children) {
			cmd.append(i->toString());
		}

		auto parseRes = parseCmdLine(cmd.c_str());
		if (parseRes.second != ParseErr::Ok) {
			throw NotStringifyException{};
		}

		auto str = strCmdOutput(parseRes.first);
		if (str.back() == '\n')
			str.pop_back();
		return str;
	}
};

struct VarEval : public ExpandBase
{
	VarEval(std::string s)
		: ExpandBase(NodeType::VarEval),varName(std::move(s)) {}
	std::string varName;
	std::string toString() const override {
		assert(false && "not implemented");
	}
};

struct PidEval : public ExpandBase
{
    PidEval() : ExpandBase(NodeType::PidEval) {}
    std::string toString() const override {
        long pid = (long) getpid();
        return std::to_string(pid);
    }
};

struct LastExitStatusEval : public ExpandBase
{
    LastExitStatusEval() : ExpandBase(NodeType::LastExitStatusEval) {}
    std::string toString() const override {
        return std::to_string(getContext().lastExitStatus);
    }
};

struct HomeExp : public ExpandBase
{
	HomeExp() : ExpandBase(NodeType::HomeExp) {}
	std::string toString() const override {
		char* home = getenv("HOME");
		if (home == NULL) {
			std::fprintf(stderr, "getenv error");
			std::exit(1);
		}
		return home;
	}
};

namespace
{
using ParseRet = std::pair<std::unique_ptr<ExpandBase>, ExpandError>;

ParseRet parseRoot(const std::string& str);

ParseRet parseItem(const std::string& str, std::size_t* idx);

std::pair<std::unique_ptr<WithinParen>, ExpandError>
parseWithinParen(const std::string& str, std::size_t* idx);

// ret: ExprEval CmdEval VarEval
//      and maybe Plain
ParseRet parseEval(const std::string& str, std::size_t* idx);

ParseRet parseSingleQuoted(const std::string& str, std::size_t* idx);

ParseRet parseDoubleQuoted(const std::string& str, std::size_t* idx);

ParseRet parseSpace(const std::string& str, std::size_t* idx);


bool notDelimiter(char ch)
{
	return ch != '$' && !std::isspace(ch)
		   && ch != '\'' && ch != '\"' && ch != ')';
}
	
}; // end anonymous namespace

std::pair<std::string, ExpandError>
expand(const std::string& str)
{
	auto result = parseRoot(str);
	if (result.second != ExpandError::Ok) {
		// deal with error
		return {"", ExpandError::Error};
	} else {
		try {
			std::string expanded = result.first->toString();
			return {std::move(expanded), ExpandError::Ok};
		} catch (const NotStringifyException&) {
			return {"", ExpandError::Error};
		}
	}
}


namespace
{
ParseRet parseRoot(const std::string& str)
{
	auto root = std::make_unique<Root>();
	std::size_t idx = 0;

	while (idx < str.size()) {
		auto res = parseItem(str, &idx);
		if (res.second != ExpandError::Ok) {
			return res;
		} else {
			root->children.push_back(std::move(res.first));
		}
	}

	return {std::move(root), ExpandError::Ok};
} // end parseRoot

ParseRet parseItem(const std::string& str, std::size_t* idx)
{
	switch (str[*idx]) {
	case '(':
		return parseWithinParen(str, idx);
	case '$':
		return parseEval(str, idx);
	case '\"':
		return parseDoubleQuoted(str, idx);
	case '\'':
		return parseSingleQuoted(str, idx);
	case '~': {
		char next = str[*idx + 1];
		if ((!!std::isspace(next)) | (next == '/')
			| (next == '\'') | (next == '\"') | (next == ')')
			| (next == '\0')) {
			++*idx;
			return {std::make_unique<HomeExp>(), ExpandError::Ok};
		} else {
			// plain text
			std::string text;
			do {
				// if (str[*idx] == '\\') {
				// 	++*idx;
				// }
				text.push_back(str[*idx]);
				++*idx;
			} while (*idx < str.size()
					 && notDelimiter(str[*idx]));
			return {std::make_unique<Plain>(std::move(text)), ExpandError::Ok};
		}
	}

	default:
		if (std::isspace(str[*idx])) {
			return parseSpace(str, idx);
		} else {
			// plain text
			std::string text;
			do {
				// if (str[*idx] == '\\')
				// 	++*idx;
				text.push_back(str[*idx]);
				++*idx;
			} while (*idx < str.size() && notDelimiter(str[*idx]));
			return {std::make_unique<Plain>(std::move(text)), ExpandError::Ok};
		}

	}
}

std::pair<std::unique_ptr<WithinParen>, ExpandError>
parseWithinParen(const std::string& str, std::size_t* idx)
{
	assert(str[*idx] == '(');
	auto result = std::make_unique<WithinParen>();
	++*idx;
	while (*idx < str.size() && str[*idx] != ')') {
		auto res = parseItem(str, idx);
		if (res.second != ExpandError::Ok) {
			return {nullptr, res.second};
		}

		result->children.push_back(std::move(res.first));
	}

	if (str[*idx] != ')') {
		return {nullptr, ExpandError::Error};
	} else {
		++*idx;
		return {std::move(result), ExpandError::Ok};
	}
}


ParseRet parseSingleQuoted(const std::string& str, std::size_t* idx)
{
	assert(str[*idx] == '\'');
	++*idx;
	std::string quoted;
	while (*idx < str.size() && str[*idx] != '\'') {
		quoted.push_back(str[*idx]);
		++*idx;
	}

	if (str[*idx] == '\'') {
		// ok
		++*idx;
		return {std::make_unique<SingleQuoted>(std::move(quoted)),
				ExpandError::Ok};
	} else {
		return {nullptr, ExpandError::Error};
	}
}

ParseRet parseDoubleQuoted(const std::string& str, std::size_t* idx)
{
	assert(str[*idx] == '\"');
	++*idx;
	auto root = std::make_unique<DoubleQuoted>();
	while (*idx < str.size() && str[*idx] != '\"') {
		switch (str[*idx]) {
		case '$': {
			auto res = parseEval(str, idx);
			if (res.second != ExpandError::Ok) {
				return res;
			} else {
				root->children.push_back(std::move(res.first));
			}
			break;
		}
		default: {
			std::string text;
			while (*idx < str.size() && str[*idx] != '$' && str[*idx] != '\"') {
				text.push_back(str[*idx]);
				++*idx;
			}
			root->children.push_back(
					std::make_unique<Plain>(std::move(text)));
			break;
		}

		}
	}
	if (*idx < str.size() && str[*idx] == '\"') {
		++*idx;
		return {std::move(root), ExpandError::Ok};
	} else
		return {nullptr, ExpandError::Error};
}


ParseRet parseSpace(const std::string& str, std::size_t* idx)
{
	assert(std::isspace(str[*idx]));
	std::string spaces;
	do {
		spaces.push_back(str[*idx]);
		++*idx;
	} while (*idx < str.size() && std::isspace(str[*idx]));
	return {std::make_unique<Space>(std::move(spaces)),
			ExpandError::Ok};
}

ParseRet parseEval(const std::string& str, std::size_t* idx)
{
	assert(str[*idx] == '$');
	if (str[*idx + 1] == '(') {
		++*idx;
		auto res = parseWithinParen(str, idx);
		if (res.second != ExpandError::Ok) {
			return {nullptr, res.second};
		}

		if (res.first->children.size() == 1
			&& res.first->children[0]->type == NodeType::WithinParen) {
			// $(())
			auto inner = static_cast<WithinParen*>(res.first->children[0].get());
			auto expr = std::make_unique<ExprEval>();
			expr->children = std::move(inner->children);
			return {std::move(expr), ExpandError::Ok};
		} else {
			auto cmd = std::make_unique<CmdEval>();
			cmd->children = std::move(res.first->children);
			return {std::move(cmd), ExpandError::Ok};
		}

	} else {
		// variable eval or plain text
		++*idx;
		if (std::isalpha(str[*idx])) {
			std::string name;
			do {
				name.push_back(str[*idx]);
				++*idx;
			} while (std::isalnum(str[*idx]));

			return {std::make_unique<VarEval>(std::move(name)),
					ExpandError::Ok};
		} else {

			if (str[*idx] == '$') {
                ++*idx;
                return {std::make_unique<PidEval>(), ExpandError::Ok};
			} else if (str[*idx] == '?') {
				++*idx;
				return {std::make_unique<LastExitStatusEval>(), ExpandError::Ok};
			} /* something else */ else {
				std::string text("$");
				while (*idx < str.size() && notDelimiter(str[*idx])) {
					text.push_back(str[*idx]);
					++*idx;
				}

				return {std::make_unique<Plain>(std::move(text)),
						ExpandError::Ok};
			}
		}
	}
} // end parseEval


};
