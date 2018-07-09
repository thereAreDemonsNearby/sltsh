#include "parse.h"
#include <cctype>
#include <cassert>
#include <limits.h>
#include <utility>
#include <thread>
namespace
{

class StrBuffer
{
public:
    StrBuffer()
		: size(0), cap(15), buff(new char[cap+1])
    {
        buff[0] = '\0';	   
    }

    StrBuffer(const StrBuffer&) = delete;
    StrBuffer(StrBuffer&& rhs) {
        buff = rhs.buff;
        rhs.buff = nullptr;
        size = rhs.size;
        cap = rhs.cap;	   
    }
    ~StrBuffer() {
        if (buff)
            delete [] buff;
		
    }

    void push_back(char ch) {
        assert(buff);
        if (size == cap) {
            auto nbuff = new char[2 * cap + 1];
            for (std::size_t i = 0; i < size; ++i) {
                nbuff[i] = buff[i];
            }
            delete [] buff;
            buff = nbuff;
            cap = 2 * cap;
			std::vector<int> vvv;
        }
        buff[size++] = ch;
        buff[size] = '\0';
    }

    char* release() {
        assert(buff);
        auto ret = buff;
        buff = nullptr;
        ret[size] = '\0';

        return ret;
    }

    char* get() {
        return buff;
    }
private:
    std::size_t size;
    std::size_t cap;
    char* buff;
};

bool isDelim(char ch)
{
    return ch == '(' || ch == ')' || ch == ';' || ch == '&' || ch == '|'
            || ch == '<' || ch == '>';
}

std::pair<StrBuffer, ParseErr> nextArgv(char const*& p)
{
    assert(!isDelim(*p));
    StrBuffer chunk;
    if (*p == '\"') {
        ++p;
        while (*p != '\0' && *p != '\"') {
            chunk.push_back(*p++);
        }

        if (*p == '\"') {
            // ok
            ++p;
            return {std::move(chunk), ParseErr::Ok};
        } else {
            return {std::move(chunk), ParseErr::UnpairedDoubleQuotationMark};
        }
    } else if (*p == '\'') {
        ++p;
        while (*p != '\0' && *p != '\'') {
            chunk.push_back(*p++);
        }

        if (*p == '\'') {
            ++p;
            return {std::move(chunk), ParseErr::Ok};
        } else {
            return {std::move(chunk), ParseErr::UnpairedSingleQuotationMark};
        }
    } else {
        while (*p != '\0' && !std::isblank(*p) &&
               !isDelim(*p)) {
            if (*p == '\\') {
                ++p;
            }
            chunk.push_back(*p++);
        }

        return {std::move(chunk), ParseErr::Ok};
    }
}

void skipBlank(char const*& p) {
    while (*p != '\0' && std::isblank(*p))
        ++p;
}

std::pair<RdUnit, ParseErr> parseRdUnit(char const*& p);

}

ParseResult parsePrimary(char const*& p)
{
	if (*p == '(') {
		++p;
		auto result = parseList(p);
        if (result.second != ParseErr::Ok) {
            return result;
        }

        skipBlank(p);
        if (*p != ')') {
            return {nullptr, ParseErr::MissRightParen};
        }
        ++p;

        auto ret = std::make_unique<Group>(std::move(result.first));
        return {std::move(ret), ParseErr::Ok};
	} else {
	    assert(!isDelim(*p));
		auto execNode = std::make_unique<Exec>();
        while (*p != '\0' && !isDelim(*p)) {
            if (std::isdigit(*p)) {
                char const* currPos = p;
                do { ++currPos; } while (std::isdigit(*currPos));
                //skipBlank(currPos);
                if (*currPos == '>' || *currPos == '<') {
                    break;
                }

            } else if (*p == '>' || *p == '<') {
                break;
            }

            auto pair = nextArgv(p);
            if (pair.second != ParseErr::Ok) {
                return {nullptr, pair.second};
            }

            execNode->argv.push_back(pair.first.release());
            skipBlank(p);
        }
        if (execNode->argv.empty()) {
            return {nullptr, ParseErr::EmptyArgvList};
        }
        execNode->argv.push_back(nullptr);

        return {std::move(execNode), ParseErr::Ok};
	}
}

ParseResult parseRedirected(char const*& p)
{
    assert(!isDelim(*p) || *p == '(');
    auto primary = parsePrimary(p);
    if (primary.second != ParseErr::Ok) {
        return {nullptr, primary.second};
    }

    skipBlank(p);

    if (std::isdigit(*p) || *p == '>' || *p == '<') {
        std::vector<RdUnit> rdUnits;
        do {
            auto rdpair = parseRdUnit(p);
            if (rdpair.second != ParseErr::Ok) {
                return {nullptr, ParseErr::InvalidRedirection};
            } else {
                rdUnits.push_back(rdpair.first);
            }
            skipBlank(p);
        } while (std::isdigit(*p) || *p == '>' || *p == '<');

        primary.first->setRd(std::move(rdUnits));
        return {std::move(primary.first), ParseErr::Ok};

    } else {
        return primary;
    }
}

ParseResult parsePipe(char const*& p)
{
    auto pair = parseRedirected(p);
    if (pair.second != ParseErr::Ok) {
        return {nullptr, pair.second};
    }
    skipBlank(p);

    if (*p != '|') {
        return pair;
    } else {
        auto root = std::move(pair.first);
        do {
            ++p;
            skipBlank(p);
            auto rhs = parseRedirected(p);
            if (rhs.second != ParseErr::Ok) {
                return {nullptr, rhs.second};
            }
            root = std::make_unique<Pipe>(std::move(root), std::move(rhs.first));
            skipBlank(p);
        } while (*p == '|');
        return {std::move(root), ParseErr::Ok};
    }
}

ParseResult parseList(char const*& p)
{
    auto lhspair = parsePipe(p);
    if (lhspair.second != ParseErr::Ok) {
        return {nullptr, lhspair.second};
    }
    skipBlank(p);

    if (*p == '&' || *p == ';') {
        auto curr = std::move(lhspair.first);
        auto ordered = std::make_unique<Ordered>();
        if (*p == '&') {
            auto canSet = curr->setBg(true);
            if (!canSet) {
                return {nullptr, ParseErr::NotBgable};
            }
        } else {
            assert(*p == ';');
        }
        ordered->list.push_back(std::move(curr));
        ++p;
        skipBlank(p);

        while (*p != '\0' && !isDelim(*p)) {
            auto pr = parsePipe(p);
            if (pr.second != ParseErr::Ok) {
                return {nullptr, pr.second};
            }

            skipBlank(p);
            if (*p == '&') {
                pr.first->setBg(true);
                ordered->list.push_back(std::move(pr.first));
                ++p;
            } else if(*p == ';') {
                ordered->list.push_back(std::move(pr.first));
                ++p;
            } else {
                ordered->list.push_back(std::move(pr.first));
                break;
            }
            skipBlank(p);
        }
        if (ordered->list.size() == 1) {
            return {std::move(ordered->list[0]), ParseErr::Ok};
        } else {
            return {std::move(ordered), ParseErr::Ok};
        }

    } else {
        return lhspair;
    }
}

ParseResult parseCmdLine(char const* begin)
{
    skipBlank(begin);
    if (*begin == '\0') {
        return {nullptr, ParseErr::EmptyCmd};
    }
    auto res = parseList(begin);
    if (res.second != ParseErr::Ok) {
        return {nullptr, res.second};
    }

    skipBlank(begin);
    if (*begin != '\0') {
        return {nullptr, ParseErr::NotSingular};
    }

    return res;
}


namespace
{

std::pair<std::string, ParseErr> parseFilename(char const*& p)
{
    std::string filename;
    auto pair = nextArgv(p);
    if (pair.second != ParseErr::Ok) {
        return {"", pair.second};
    } else {
        return {pair.first.get(), ParseErr::Ok};
    }
}

std::pair<int, ParseErr> parseFd(char const*& p)
{
    std::string lexeme;
    auto pair = nextArgv(p);
    if (pair.second != ParseErr::Ok) {
        return {0, pair.second};
    } else {
        int fd;
        try {
            fd = std::stoi(pair.first.get());
        } catch (const std::invalid_argument& ) {
            return {0, ParseErr::ExpectNumber};
        } catch (const std::out_of_range& ) {
            return {0, ParseErr::FdOutOfRange};
        }

        constexpr int OPEN_MAX_GUESS = 1024;
        if (fd < 0 || fd > OPEN_MAX_GUESS) {
            return {0, ParseErr::FdOutOfRange};
        }

        return {fd, ParseErr::Ok};
    }
}

std::pair<RdUnit, ParseErr> parseRdUnit(char const*& p)
{
    assert(std::isdigit(*p) || *p == '<' || *p == '>');
    if (std::isdigit(*p)) {
        int fd = parseFd(p).first;

        if (*p == '>') {
			++p;
			skipBlank(p);
            if (*p == '>') {
                p += 1;
                skipBlank(p);

                auto res = parseFilename(p);
                if (res.second != ParseErr::Ok) {
                    return {{}, res.second};
                } else {
                    return {{RdTag::App, RdObj(fd), RdObj(std::move(res.first))},
                            ParseErr::Ok};
                }

            } else if (*p == '&') {
                p += 1;
                skipBlank(p);
                auto res = parseFd(p);

                if (res.second != ParseErr::Ok) {
                    return {{}, res.second};
                } else {
                    if (*p == '\0' || std::isblank(*p) || isDelim(*p)) {
                        return {{RdTag::OutDup, RdObj(fd), RdObj(res.first)},
                                ParseErr::Ok};
                    } else {
                        return {{}, ParseErr::NotSingular};
                    }
                }

            } else {
                auto res = parseFilename(p);
                if (res.second != ParseErr::Ok) {
                    return {{}, res.second};
                } else {
                    return {{RdTag::Out, RdObj(fd), RdObj(std::move(res.first))},
                            ParseErr::Ok};
                }
            }
        } else {
            assert(false);
        }

    } else if (*p == '>') {
		
        if (*(p + 1) == '>') {
            p += 2;
            skipBlank(p);
            auto res = parseFilename(p);
            if (res.second != ParseErr::Ok) {
                return {{}, res.second};
            } else {
                return {{RdTag::App, RdObj(), RdObj(std::move(res.first))},
                        ParseErr::Ok};
            }
        } else if (*(p + 1) == '&') {
            p += 2;
            skipBlank(p);
            auto res = parseFilename(p);
            if (res.second != ParseErr::Ok) {
                return {{}, res.second};
            } else {
                return {{RdTag::OutErr, RdObj(), RdObj(std::move(res.first))},
                        ParseErr::Ok};
            }
        } else {
            p += 1;
            skipBlank(p);
            auto res = parseFilename(p);
            if (res.second != ParseErr::Ok) {
                return {{}, res.second};
            } else {
                return {{RdTag::Out, RdObj(), RdObj(std::move(res.first))},
                        ParseErr::Ok};
            }
        }

    } else {
        assert(*p == '<');
        p += 1;
        skipBlank(p);
        auto res = parseFilename(p);
        if (res.second != ParseErr::Ok) {
            return {{}, res.second};
        } else {
            return {{RdTag::In, RdObj(), RdObj(std::move(res.first))},
                    ParseErr::Ok};
        }
    }


}

}
