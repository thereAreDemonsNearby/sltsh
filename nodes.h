#ifndef NODES_H__
#define NODES_H__

#include <memory>
#include <cstdlib>
#include <string>
#include <vector>
#include <cassert>

#include "executor.h"

struct RdObj
{
    enum {
        FN, FD, Empty,
    } tag;

    explicit RdObj(std::string fn) : tag(FN), fname(std::move(fn)) {}

    explicit RdObj(int fildes) : tag(FD), fd(fildes) {}

    RdObj() : tag(Empty) {}

    std::string fname;
    int fd;
    std::string toStringDebug() const;
    std::string toString() const;
};

enum class RdTag
{
    Out, In, App, OutDup, OutErr, Invalid
};

struct RdUnit
{
    RdTag rdTag;
    RdObj lhs;
    RdObj rhs;

    RdUnit() : rdTag(RdTag::Invalid), lhs(RdObj::Empty), rhs(RdObj::Empty) {}
    RdUnit(RdTag tag, RdObj l, RdObj r) : rdTag(tag), lhs(std::move(l)), rhs(std::move(r)) {}
    std::string toStringDebug() const;
    std::string toString() const;
};

struct NodeBase
{
    virtual ~NodeBase();
	virtual std::string toStringDebug() const = 0;
	virtual std::string toString() const = 0;
	virtual void accept(Executor*) = 0;

	virtual bool setBg(bool) {
	    return false;
	}

	virtual bool getBg() {
	    assert(false);
	}

	virtual bool setRd(std::vector<RdUnit>&&) {
	    return false;
	}

	virtual bool runInCurrentProcess() {
	    return false;
	}

	virtual bool isPipe() {
		return false;
	}
};

/// redirectable backgroundable
struct Exec final : public NodeBase
{
    friend class Executor;
    bool bg = false;
    std::vector<RdUnit> rdUnits;
	std::vector<char*> argv;
	~Exec() override {
	    size_t i;
	    for (i = 0; i < argv.size() - 1; ++i) {
	        if (argv[i])
	            delete [] argv[i];
	        argv[i] = nullptr;
	    }
	}

	bool setBg(bool b) override {
	    bg = b;
	    return true;
	}

	bool getBg() override {
	    return bg && !runInCurrentProcess();
	}

	bool setRd(std::vector<RdUnit>&& v) override {
	    rdUnits = std::move(v);
	    return true;
	}

	std::string toStringDebug() const override;
	std::string toString() const override;
	void accept(Executor* e) override { e->visit(this); }
	bool runInCurrentProcess() override;
};

/// backgroundable
struct Pipe final : public NodeBase
{
    friend class Executor;
    bool bg = false;
    Pipe(std::unique_ptr<NodeBase> l, std::unique_ptr<NodeBase> r)
            : left(std::move(l)), right(std::move(r)) {}
	std::unique_ptr<NodeBase> left;
	std::unique_ptr<NodeBase> right;

    bool setBg(bool b) override {
        bg = b;
        return true;
    }

    bool getBg() override {
        return bg;
    }

	bool isPipe() override {
		return true;
	}

	std::string toStringDebug() const override;
    std::string toString() const override;
	void accept(Executor* e) override { e->visit(this); }
};

/// redirectable backgroundable
struct Group final : public NodeBase
{
    friend class Executor;
    bool bg = false;
    std::vector<RdUnit> rdUnits;
    explicit Group(std::unique_ptr<NodeBase> p)
            : cmd(std::move(p)) {}
	std::unique_ptr<NodeBase> cmd;

    bool setBg(bool b) override {
        bg = b;
        return true;
    }

    bool getBg() override {
        return bg;
    }

    bool setRd(std::vector<RdUnit>&& v) override {
        rdUnits = std::move(v);
        return true;
    }

    std::string toStringDebug() const override;
    std::string toString() const override;
	void accept(Executor* e) override { e->visit(this); }
};

struct Ordered final : public NodeBase
{
    friend class Executor;
	std::vector<std::unique_ptr<NodeBase>> list;
	std::string toStringDebug() const override;
	std::string toString() const override;
	void accept(Executor* e) override { e->visit(this); }
	bool runInCurrentProcess() override { return true; }
};

#endif
