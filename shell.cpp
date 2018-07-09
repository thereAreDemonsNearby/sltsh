#include <string>
#include <set>
#include <sstream>
#include <iostream>
#include <type_traits>
#include <cstring>
#include <signal.h>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "expand.h"
#include "context.h"
#include "parse.h"
#include "executor.h"

constexpr unsigned CREATMODE = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;

Context& getContext()
{
    static Context context;
    return context;
}


std::set<std::string> builtinCmds = {
        "cd", "fg", "bg", "umask", "exit", "jobs"
};

using SigHandler = void(*)(int);
SigHandler setSignalHandler(int sig, SigHandler handler);
void init();
void restoreSignals();
void stopAndWait(pid_t pid);
void prepareRedirection(const std::vector<RdUnit>& rdvec);
void dup2Checked(int fd, int to);
void newContextRun(std::unique_ptr<NodeBase>& node, Executor* executor);
void sigchldHandler(int signo);
void doBuiltinCmd(Exec* node);
void becomeTtyFgPgrp();
void newPipeContextRun(Pipe* node, Executor* executor);

void doBg(int jid);
void doFg(int jid);

std::string strCmdOutput(std::unique_ptr<NodeBase> node);

void SIGTTOU_handler(int signo)
{
    abort();
}

class SignalBlockGuard
{
private:
    sigset_t set_;
	sigset_t oldset_;
    template<typename S, typename... Args>
    void addRest(S s, Args... rest) {
        static_assert(std::is_same<S, int>::value, "Not the correct signal type");
        sigaddset(&set_, s);
        addRest(rest...);
    };
    void addRest(int signo) {
        sigaddset(&set_, signo);
    }
public:
    template<typename S, typename... Args>
    SignalBlockGuard(S s1, Args... rest) {
        sigemptyset(&set_);
        addRest(s1, rest...);
        if (sigprocmask(SIG_BLOCK, &set_, &oldset_) < 0) {
            std::perror("sigpromask error");
            std::exit(6);
        }
    }

    void unblock() {
        if (sigprocmask(SIG_SETMASK, &oldset_, nullptr) < 0) {
            std::perror("sigpromask error");
            std::exit(6);
        }
    }

    ~SignalBlockGuard() {
        unblock();
    }
};

void prPrompt()
{
	char hostbuf[128];
	if (gethostname(hostbuf, 128) < 0) {
		std::perror("gethostname error");
		std::exit(1);
	}
	int pathmax;
	if ((pathmax = pathconf(".", _PC_PATH_MAX)) < 0) {
		std::perror("fpathconf error");
		std::exit(1);
	}
	char* path = new char[pathmax];
	char* pathp;
	getcwd(path, pathmax);
	pathp = std::strrchr(path, '/');
	if (pathp == NULL) {
		pathp = path;
	} else {
		pathp++;
	}
	std::fprintf(stderr, "<%s@%s %s> ", getlogin(), hostbuf, pathp);
	free(path);
}

int main()
{
    init();
    std::string cmd;

	prPrompt();
    while (std::getline(std::cin, cmd)) {
        auto expandRes = expand(cmd);
        if (expandRes.second != ExpandError::Ok) {
            std::cerr << "sltsh: expand error" << std::endl;
        } else {
            auto parseRes = parseCmdLine(expandRes.first.c_str());
            if (parseRes.second != ParseErr::Ok) {
                if (parseRes.second != ParseErr::EmptyCmd) {
                    std::cerr << "sltsh: syntax error" << std::endl;
                }
            } else {
                Executor executor;
                if (parseRes.first->runInCurrentProcess()) {
                    parseRes.first->accept(&executor);
                } else {
                    newContextRun(parseRes.first, &executor);
                }
            }
        }

        auto& delayed = getContext().delayedMsg;
        while (!delayed.empty()) {
            std::cerr << delayed.front();
            delayed.pop();
        }
		prPrompt();
    }

    if (std::cin.eof()) {
        std::cerr << "EOFFF" << std::endl;
    } else {
        std::cerr << "ERRRRRORR" << std::endl;
    }

    std::exit(getContext().lastExitStatus);
}

void init()
{
    auto setSH = [](int signo, SigHandler sh) {
        if (setSignalHandler(signo, sh) == SIG_ERR) {
            std::perror("init failed");
            std::exit(1);
        }
    };
    setSH(SIGTSTP, SIG_IGN);
    setSH(SIGINT, SIG_IGN);
    setSH(SIGQUIT, SIG_IGN);

    setSH(SIGCHLD, sigchldHandler);
}

void restoreSignals()
{
    auto setSH = [](int signo, SigHandler sh) {
        if (setSignalHandler(signo, sh) == SIG_ERR) {
            std::perror("restore failed");
            std::exit(1);
        }
    };
    setSH(SIGTSTP, SIG_DFL);
    setSH(SIGINT, SIG_DFL);
    setSH(SIGQUIT, SIG_DFL);
}



void Executor::visit(Exec* node)
{
    if (builtinCmds.find(node->argv[0]) != builtinCmds.end()) {
        /// builtin command
        doBuiltinCmd(node);
    } else {
        prepareRedirection(node->rdUnits);
		if (!strcmp(node->argv[0], "ls") || !strcmp(node->argv[0], "grep")) {
			node->argv.back() = strdup("--color=auto");
			node->argv.push_back(nullptr);
		}
        if (execvp(node->argv[0], node->argv.data()) < 0) {
            std::ostringstream os;
            os << "sltsh: " << node->argv[0];
            std::perror(os.str().c_str());
            std::exit(3);
        }
        assert(false);
    }
}

void stopAndWait(pid_t pid)
{
	//TODO
	pid_t waitret;
	int statloc;
    if ((waitret = waitpid(pid, &statloc, WUNTRACED)) < 0) {
        std::perror("waitpid error");
        std::exit(5);
    }

	becomeTtyFgPgrp();
    if (WIFEXITED(statloc)) {
		getContext().onProcessExited(waitret, statloc, false);
    } else if (WIFSTOPPED(statloc)) {
        getContext().onProcessStopped(waitret, statloc, false);
    } else if (WIFSIGNALED(statloc)) {
		getContext().onProcessSignaled(waitret, statloc, false);
    } else
        assert(false);
}

void Executor::visit(Pipe* node)
{
	newPipeContextRun(node, this);
}

void Executor::visit(Group* node)
{
    prepareRedirection(node->rdUnits);
    node->cmd->accept(this);
	// if reached here:
	std::exit(getContext().lastExitStatus);
}

void Executor::visit(Ordered* node)
{
    for (auto& child : node->list) {
        if (child->runInCurrentProcess()) {
            child->accept(this);
        } else {
            newContextRun(child, this);
        }
    }
}

void sigchldHandler(int signo)
{
    int statloc;
    pid_t pid;
    while ((pid = waitpid(-1, &statloc, WUNTRACED | WNOHANG)) > 0) {
		if (WIFEXITED(statloc)) {
			getContext().onProcessExited(pid, statloc, true);
		} else if (WIFSTOPPED(statloc)) {
			getContext().onProcessStopped(pid, statloc, true);
		} else if (WIFSIGNALED(statloc)) {
			getContext().onProcessSignaled(pid, statloc, true);
		} else
			assert(false);
    }

    if (pid == 0) {
        return;
    } else {
        if (errno != ECHILD) {
            std::perror("waitpid error");
            std::exit(15);
        }
    }
}


void newContextRun(std::unique_ptr<NodeBase>& node, Executor* executor)
{
	if (node->isPipe()) {
		auto pipeNode = static_cast<Pipe*>(node.get());
		newPipeContextRun(pipeNode, executor);
		return;
	}
	
    SignalBlockGuard blockGuard(SIGCHLD);
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork error");
        std::exit(2);
    } else if (pid == 0) {
        /// child
        if (setpgid(0, 0) < 0) {
            std::perror("setpgid error");
            std::exit(3);
        }
        blockGuard.unblock();
        restoreSignals();
        // if (node->getBg()) {
		// 	// why don't work ??
        //     becomeTtyFgPgrp();
        // }
        node->accept(executor);

    } else {
        /// parent
        if (setpgid(pid, pid) < 0) {
            std::perror("setpgid error");
            std::exit(4);
        }

		//std::cerr << "hapi" << node->toString() << std::endl;
		int jid = getContext().addJob(pid, node->toString());
        if (!node->getBg()) {
			if (tcsetpgrp(STDIN_FILENO, pid) < 0) {
				std::perror("tcsetpgrp error");
				std::exit(3);
			}
            stopAndWait(pid);
        } else {
            std::fprintf(stderr, "[%d] %ld Running\n", jid, (long)pid);
        }
    }
}

void newPipeContextRun(Pipe* node, Executor* executor)
{
	int fd[2];
	if (pipe(fd) < 0) {
		std::perror("pipe error");
		std::exit(20);
	}
	
	pid_t lhs, rhs;
	SignalBlockGuard guard(SIGCHLD);
	if ((rhs = fork()) < 0) {
		std::perror("fork error");
		std::exit(20);
	} else if (rhs == 0) {
		// rhs child
		guard.unblock();
		restoreSignals();
		if (setpgid(0, 0)) {
			std::perror("setpgid error");
			std::exit(21);
		}
		close(fd[1]);
		dup2Checked(fd[0], 0);
		close(fd[0]);

		node->right->accept(executor);

		// if reaches here
		std::exit(getContext().lastExitStatus);
		
	} else {
		// parent
		if (setpgid(rhs, rhs) < 0) {
			std::perror("setpgid error");
			std::exit(20);
		}
		
		if ((lhs = fork()) < 0) {
			std::perror("fork error");
			std::exit(21);
		} else if (lhs == 0) {
			// lhs child
			guard.unblock();
			restoreSignals();
			if (setpgid(0, rhs) < 0) {
				std::perror("setpgid error");
				std::exit(22);
			}
			close(fd[0]);
			dup2Checked(fd[1], 1);
			close(fd[1]);

			node->left->accept(executor);

			// if reaches here
			std::exit(getContext().lastExitStatus);
			
		} else {
			// parent
			close(fd[0]);
			close(fd[1]);
			if (setpgid(lhs, rhs) < 0) {
				std::perror("setpgid error");
				std::exit(23);
			}

			int jid = getContext().addJob(rhs, lhs, node->toString());
			
			if (!node->getBg()) {
				if (tcsetpgrp(STDIN_FILENO, rhs) < 0) {
					std::perror("tcsetpgrp error");
					std::exit(23);
				}
				// both lhs and rhs have group id 'rhs'
				stopAndWait(-rhs);
				stopAndWait(-rhs);
			} else {
				std::fprintf(stderr, "[%d] Running\n", jid);
			}
			
		}
	}	
}

SigHandler setSignalHandler(int sig, SigHandler handler)
{
    struct sigaction sa, osa;
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    if (sig == SIGALRM) {
        /** In some situation we want to use SIGALRM
          * to interrupt overtime system calls*/
#ifdef SA_INTERRUPT
        sa.sa_flags |= SA_INTERRUPT;
#endif
    } else {
        sa.sa_flags |= SA_RESTART;
    }
    sigemptyset(&sa.sa_mask);
    if (sigaction(sig, &sa, &osa) < 0)
        return SIG_ERR;
    return osa.sa_handler;
}

void prepareRedirection(const std::vector<RdUnit>& rdvec)
{
    for (auto& u : rdvec) {
        switch (u.rdTag) {
        case RdTag::In: {
            assert(u.lhs.tag == RdObj::Empty
                   && u.rhs.tag == RdObj::FN);
            int fd = open(u.rhs.fname.c_str(), O_RDONLY);
            if (fd < 0) {
                std::fprintf(stderr, "can't open %s for read\n", u.rhs.fname.c_str());
                std::exit(7);
            }
            dup2Checked(fd, 0);
            close(fd);
            break;
        }
        case RdTag::Out: {
            assert(u.rhs.tag == RdObj::FN);
			int fd = creat(u.rhs.fname.c_str(), CREATMODE);// TODO temporary
            if (fd < 0) {
                std::fprintf(stderr, "can't open %s for write\n", u.rhs.fname.c_str());
                std::exit(8);
            }
            if (u.lhs.tag == RdObj::Empty) {
                dup2Checked(fd, 1);
            } else {
                assert(u.lhs.tag == RdObj::FD);
                dup2Checked(fd, u.lhs.fd);
            }
            close(fd);
            break;
        }
        case RdTag::App: {
            assert(u.rhs.tag == RdObj::FN);
            int fd = open(u.rhs.fname.c_str(), O_WRONLY | O_APPEND);
			// int flags = fcntl(fd, F_GETFD);
			// flags &= ~FD_CLOEXEC;
			// fcntl(fd, F_SETFD, flags);
            if (fd < 0) {
                std::fprintf(stderr, "can't open %s for append\n", u.rhs.fname.c_str());
                std::exit(8);
            }
            if (u.lhs.tag == RdObj::Empty) {
                dup2Checked(fd, 1);
            } else {
                assert(u.lhs.tag == RdObj::FD);
                dup2Checked(fd, u.lhs.fd);
            }
            close(fd);
            break;
        }
        case RdTag::OutDup: {
            /// 2>&1
            assert(u.lhs.tag == RdObj::FD && u.rhs.tag == RdObj::FD);
            dup2Checked(u.rhs.fd, u.lhs.fd);
            break;
        }
        case RdTag::OutErr: {
            /// >& file
			int fd = creat(u.rhs.fname.c_str(), CREATMODE);
            if (fd < 0) {
                std::fprintf(stderr, "can't open %s for write\n", u.rhs.fname.c_str());
                std::exit(9);
            }
            dup2Checked(fd, 1);
            dup2Checked(fd, 2);
            close(fd);
            break;
        }
        default:
            assert(false);
        }
    }
}

void dup2Checked(int fd, int to)
{
    if (fd != to) {
        if (dup2(fd, to) != to) {
            std::perror("dup2 error");
            std::exit(0);
        }
    }
}

void doBuiltinCmd(Exec* node)
{
    auto& argv = node->argv;
    if (!strcmp(argv[0], "cd")) {
		if (argv.size() == 2) {
			argv.back() = strdup(getenv("HOME"));
			argv.push_back(nullptr);
		}
		
        if (argv.size() != 3) {
            std::fprintf(stderr, "sltsh: cd: wrong number of arguments\n");
            return;
        }

        if (chdir(argv[1]) < 0) {
            std::perror("sltsh: cd error");
            return;
        }
    } else if (!strcmp(argv[0], "exit")) {
        if (argv.size() != 2) {
            std::fprintf(stderr, "sltsh: exit: wrong number of arguments\n");
        } else
			std::exit(0);
    } else if (!strcmp(argv[0], "bg")) {
		if (argv.size() - 1 != 2) {
			std::fprintf(stderr, "sltsh: bg: usage: bg <jobid>\n");
			return;
		}
		char* endptr;
		long jid = std::strtol(argv[1], &endptr, 10);
		if (*endptr != '\0' || endptr == argv[1] || errno == ERANGE) {
			std::fprintf(stderr, "sltsh: bg: invalid argument");
			return;
		}

		doBg(jid);
		
    } else if (!strcmp(argv[0], "fg")) {
		if (argv.size() - 1 != 2) {
			std::fprintf(stderr, "sltsh: fg: usage: bg <jobid>\n");
			return;
		}
		char* endptr;
		long jid = std::strtol(argv[1], &endptr, 10);
		if (*endptr != '\0' || endptr == argv[1] || errno == ERANGE) {
			std::fprintf(stderr, "sltsh: fg: invalid argument");
			return;
		}

		doFg(jid);
		
    } else if (!strcmp(argv[0], "jobs")){
		if (argv.size() - 1 != 1) {
			std::fprintf(stderr, "sltsh: jobs: wrong number of arguments\n");
			return;
		}
		for (auto& kv: getContext().jobMap) {
			std::fprintf(stderr, "[%ld] %s\t%s\n",
					kv.first, strJobStatus[(int)kv.second.stat], kv.second.jobCmd.c_str());
		}
    } else {
        assert(!strcmp(argv[0], "umask"));
		if (argv.size() - 1 == 1) {
			auto oldmask = umask(0);
			umask(oldmask);
			std::printf("%04o\n", oldmask);
		} else if (argv.size() - 1 == 2) {
			char* endptr;
			auto mask = std::strtol(argv[1], &endptr, 8);
			if (*endptr != '\0') {
				std::fprintf(stderr, "sltsh: umask: wrong argument type\n");
				return;
			}
			if (mask > 0777 || mask < 0) {
				std::fprintf(stderr, "sltsh: umask: invalid range");
				return;
			}

			umask(mask);	
		} else {
			std::fprintf(stderr, "sltsh: umask: wrong number of arguments\n");
		}
    }
}

void becomeTtyFgPgrp()
{
    SigHandler old = setSignalHandler(SIGTTOU, SIG_IGN);
    if (old == SIG_ERR) {
        std::perror("ignore SIGTTOU failed");
        std::exit(3);
    }
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
        std::perror("tcsetpgrp error");
        std::exit(3);
    }
    if (setSignalHandler(SIGTTOU, old) == SIG_ERR) {
        std::perror("restore SIGTTOU failed");
        std::exit(3);
    }
}

template<typename T>
static inline auto toUInt(T t)
{
	return static_cast<typename std::underlying_type<T>::type>(t);
}
extern JobStatus resultTable[3][3];

void doBg(int jid)
{
	auto& jobMap = getContext().jobMap;
	auto iter = jobMap.find(jid);
	if (iter == jobMap.end()) {
		std::fprintf(stderr, "no job with id %d\n", jid);
		return;
	}
	auto& job = iter->second;
	if (job.stat == JobStatus::Stopped) {
		if (job.nproc == 1) {
			kill(job.pid[0], SIGCONT);
			job.statInd[0] = ProcStatus::Running;
			job.stat = JobStatus::Running;
		} else {
			assert(job.nproc == 2);
			kill(-job.pid[0], SIGCONT);
			if (job.statInd[0] == ProcStatus::Stopped) {
				job.statInd[0] = ProcStatus::Running;
			}
			if (job.statInd[1] == ProcStatus::Stopped) {
				job.statInd[1] = ProcStatus::Running;
			}
			job.stat = resultTable[toUInt(job.statInd[0])][toUInt(job.statInd[1])];
		}
		std::fprintf(stderr, "[%d] Continued\n", jid);
	} else {
		assert(false);
	}
}

void doFg(int jid)
{
	SignalBlockGuard guard(SIGCHLD);
	// enable the guard early to avoid race condition
	
	auto& jobMap = getContext().jobMap;
	auto iter = jobMap.find(jid);
	if (iter == jobMap.end()) {
		std::fprintf(stderr, "there's no job whose id is %d\n", jid);
		return;
	}
	auto& job = iter->second;
	if (job.stat == JobStatus::Stopped
		|| job.stat == JobStatus::Running) {
		for (int i = 0; i < job.nproc; ++i) {
			if (job.statInd[i] == ProcStatus::Stopped) {
				kill(job.pid[i], SIGCONT);
				job.statInd[i] = ProcStatus::Running;
			}
		}

		job.stat = resultTable[(int)job.statInd[0]][(int)job.statInd[1]];
		if (job.nproc == 1) {
			stopAndWait(job.pid[0]);
		} else {
			assert(job.nproc == 2);
			stopAndWait(-job.pid[0]);
			stopAndWait(-job.pid[0]);
		}
	}
	
	// if (job.stat == JobStatus::Stopped) {
	// 	if (job.nproc == 1) {
	// 		job.statInd[0] = ProcStatus::Running;
	// 		job.stat = JobStatus::Running;
	// 		kill(job.pid[0], SIGCONT);
	// 		stopAndWait(job.pid[0]);
	// 	} else {
	// 		// must be pipe
	// 		assert(job.nproc == 2);
	// 		kill(-job.pid[0], SIGCONT);
	// 		if (job.statInd[0] == ProcStatus::Stopped) {
	// 			job.statInd[0] = ProcStatus::Running;
	// 		}
	// 		if (job.statInd[1] == ProcStatus::Stopped) {
	// 			job.statInd[1] = ProcStatus::Running;
	// 		}
	// 		job.stat = resultTable[(int)job.statInd[0]][(int)job.statInd[1]];

	// 		stopAndWait(-job.pid[0]);
	// 		stopAndWait(-job.pid[0]);
	// 	}

		
		
	// } else if (job.stat == JobStatus::Running) {
	// 	// the job must be running background
	// 	for (int i = 0; i < job.nproc; ++i) {
	// 		if (job.statInd[i] == ProStatus::Stopped) {
	// 			kill(job.pid[i], SIGCONT);
	// 			job.statInd[i] = ProcStatus::Running;
	// 		}
	// 	}
	// 	job.stat = resultTable[(int)job.statInd[0]][(int)job.statInd[1]];
	// 	if (job.nproc == 1) {
	// 		stopAndWait(job.pid[0]);
	// 	} else {
	// 		assert(job.nproc == 2);
	// 		stopAndWait(-job.pid[0]);
	// 		stopAndWait(-job.pid[0]);
	// 	}
	// } else {
	// 	assert(false);
	// }
}

std::string strCmdOutput(std::unique_ptr<NodeBase>& node)
{
	Executor executor;
	int fd[2];
	if (pipe(fd) < 0) {
		std::fprintf(stderr, "pipe error while expanding");
		std::exit(1);
	}

	SignalBlockGuard guard(SIGCHLD);
	pid_t pid = fork();
	if (pid == 0) {
		close(fd[0]);
		dup2Checked(fd[1], 1);
		close(fd[1]);

		guard.unblock();
		restoreSignals();

		node->accept(&executor);
		// if reaches here
		std::exit(getContext().lastExitStatus);
		
	} else if (pid > 0) {
		std::string ret;
		
		close(fd[1]);
		constexpr std::size_t bufsz = 512;
		char buf[bufsz];
		ssize_t n;
		while ((n = read(fd[0], buf, bufsz)) > 0) {
			ret.append(buf, buf + n);
		}
		if (n < 0) {
			std::perror("read error while expanding");
			std::exit(1);
		}
		
		close(fd[0]);	
		waitpid(pid, nullptr, 0);

		return ret;
	} else {
		std::perror("fork error while expanding");
		std::exit(1);
	}
}
