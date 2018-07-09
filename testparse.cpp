#include <cstdio>
#include <iostream>
#include "parse.h"
#include "expand.h"

void printAst__(const std::string& txt, int lineno)
{
    using namespace std;
    cout << "line " << lineno << ":\n";
    auto expres = expand(txt);
    if (expres.second != ExpandError::Ok) {
        cout << "expand error\n";
    } else {
        auto parseres = parseCmdLine(expres.first.c_str());
        if (parseres.second != ParseErr::Ok) {
            cout << "parse error: " << (int)parseres.second << endl;
        } else {
            cout << parseres.first->toString() << endl;
        }
    }
}

#define printAst(txt) printAst__(txt, __LINE__)

/*int main()
{
    printAst("python$((1+2)) -c manage.py lol");
    printAst("echo \"some thing to say\" | cat");
    printAst("./a.out 2>&1");
    printAst("./a.out 1+2 >log.txt");
    printAst("./a.out 1+2 > log.txt");
    printAst("./a.out 1+2 2+3 2>log.txt");
    printAst("calc \'1*2+3-4\' >&result.dat");
    printAst("calc $((1+2+3))+4 1>stdout.log 2>stderr.log");
    printAst("calc \"$((1+2 +3))*4\" 1> stdout.log 2> stderr.log");

    printAst("ls -l | grep .c");
    printAst("ls -l | grep '^d'");
    printAst("ls -l | grep '()$;><|\\'");
    printAst("ls -l 2>&1 | grep \'()$;><|(asdf)\' 2>&1");
    printAst("\\p\\y\\t\\h\\o\\n\\$\\<\\;");
    printAst("ls -l 2>&1 | tee out.txt 1>&2 | sort ");

    printAst("(./a.out; ./b.out) 2>&1");
    printAst("./a.out ; ./b.out & c.out &");
    printAst("mkdir test; cd test");
    printAst("mkdir test;");
    printAst("mkdir test\\;");
    printAst("trash a.cpp~");
    printAst("trash \\#a.cpp~\\#");
    printAst("ls -l 2>&1 1>&2 | grep '^d' &");
    printAst("ls -l 2>&1 1>&2 | grep '^d' & echo done");
    printAst("\\(\\)\\^");
    printAst("echo &");
    printAst("echo \\&");
    printAst("(./a.out ; ./b.out) & ./c.out >> out.txt");
    printAst("emacs -nw ~/codes/project/shell.cpp");
    printAst("echo $$");
    printAst("echo $?");
    printAst("(./a.out ; ./b.out) >&tmpfile.$$");
}*/

