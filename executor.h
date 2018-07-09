#ifndef EXECUTOR_H__
#define EXECUTOR_H__

struct Exec;
struct Pipe;
struct Group;
struct Ordered;
struct Background;
struct Redirected;

class Executor
{
public:
    void visit(Exec*);
    void visit(Pipe*);
    void visit(Group*);
    void visit(Ordered*);
    void visit(Background*);
    void visit(Redirected*);
private:

};




#endif
