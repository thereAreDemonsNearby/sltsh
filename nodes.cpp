#include <set>
#include "nodes.h"

std::string RdObj::toStringDebug() const
{
    if (tag == FN) {
        std::string ret = "[file ";
        ret += fname + "]";
        return ret;
    } else if (tag == FD) {
        return std::to_string(fd);
    } else {
        return "";
    }
}

std::string RdObj::toString() const
{
    if (tag == FN) {
        return fname;
    } else if (tag == FD) {
        return std::to_string(fd);
    } else {
        return "";
    }
}

std::string RdUnit::toStringDebug() const
{
    std::string ret;
    ret += lhs.toStringDebug();
    switch (rdTag) {
    case RdTag::Out: ret += ">"; break;
    case RdTag::In:  ret += "<"; break;
    case RdTag::App: ret += ">>"; break;
    case RdTag::OutDup: ret += ">&"; break;
    case RdTag::OutErr: ret += ">&"; break;
    case RdTag::Invalid:ret += "INVALIDRD"; break;
    }

    ret += rhs.toStringDebug();

    return ret;
}

std::string RdUnit::toString() const
{
    std::string ret;
    ret += lhs.toString();
    switch (rdTag) {
    case RdTag::Out: ret += ">"; break;
    case RdTag::In:  ret += "<"; break;
    case RdTag::App: ret += ">>"; break;
    case RdTag::OutDup: ret += ">&"; break;
    case RdTag::OutErr: ret += ">&"; break;
    case RdTag::Invalid:ret += "INVALIDRD"; break;
    }

    ret += rhs.toString();

    return ret;
}


std::string Exec::toStringDebug() const
{
    std::string ret = "[argv-list ";
    for (size_t i = 0; argv[i] != nullptr; ++i) {
        assert(i < argv.size());
        ret += "[";
        ret += argv[i];
        ret += "] ";
    }
    for (auto& rd : rdUnits) {
        ret += rd.toStringDebug() + " ";
    }
    ret += "]";
    if (bg) {
        ret += " &";
    }
    return ret;
}

std::string Exec::toString() const
{
    std::string ret;
    for (size_t i = 0; argv[i] != nullptr; ++i) {
        assert(i < argv.size());
        ret += argv[i];
        ret += " ";
    }
    for (auto& rd : rdUnits) {
        ret += rd.toString() + " ";
    }
    if (ret.back() == ' ') ret.pop_back();
    if (bg) {
        ret += " &";
    }
    return ret;
}

extern std::set<std::string> builtinCmds;

bool Exec::runInCurrentProcess()
{
    return builtinCmds.find(argv[0]) != builtinCmds.end();
}

std::string Pipe::toStringDebug() const
{
    std::string ret = "[pipe ";
    ret += left->toStringDebug();
    ret += " | ";
    ret += right->toStringDebug();
    ret += "]";
    if (bg)
        ret += " &";
    return ret;
}

std::string Pipe::toString() const
{
    std::string ret;
    ret += left->toString();
    ret += " | ";
    ret += right->toString();
    if (bg)
        ret += " &";
    return ret;
}

std::string Group::toStringDebug() const
{
    std::string ret = "[group ";
    ret += cmd->toStringDebug() + " ";
    for (auto& rd : rdUnits) {
        ret += rd.toStringDebug() + " ";
    }
    ret += "]";
    if (bg) {
        ret += " &";
    }
    return ret;
}

std::string Group::toString() const
{
    std::string ret = "(";
    ret += cmd->toString() + " ";

    ret += ")";
    for (auto& rd : rdUnits) {
        ret += rd.toString() + " ";
    }
    if (ret.back() == ' ') ret.pop_back();
    if (bg) {
        ret += " &";
    }
    return ret;
}

std::string Ordered::toStringDebug() const
{
    std::string ret = "[Ordered ";
    for (auto& p : list) {
        ret += p->toStringDebug();
        ret += " ";
    }

    ret += "]";
    return ret;
}

std::string Ordered::toString() const
{
    std::string ret;
    for (auto& p : list) {
        ret += p->toString();
        ret += "; ";
    }
    if (ret.back() == ' ') ret.pop_back();
    return ret;
}

