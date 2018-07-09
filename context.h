#ifndef CONTEXT_H__
#define CONTEXT_H__

#include <map>
#include <queue>
#include <utility>
#include <string>
#include <cassert>
#include <iostream>
enum class JobStatus
{
    Running, Stopped, Finished,
};

enum class ProcStatus
{
	Running, Stopped, Exited, Signaled,
};

static const char* strJobStatus[3] = {"Running", "Stopped", "Finished"};

struct Job
{
    pid_t pid[2];
	ProcStatus statInd[2] = { ProcStatus::Running, ProcStatus::Running };
	std::string eventStr[2];
    std::string jobCmd;
    JobStatus stat;
	int nproc;

	Job(pid_t p, std::string cmd)
		: jobCmd(std::move(cmd)), stat(JobStatus::Running), nproc(1) {
		pid[0] = p; pid[1] = 0;
	}
	Job(pid_t p, pid_t q, std::string cmd)
		: jobCmd(std::move(cmd)), stat(JobStatus::Running), nproc(2) {
		pid[0] = p; pid[1] = q;
	}
};

struct Context
{
	using JobMap = std::map<int, Job>;
    std::vector<bool> jobIdUsed;
    JobMap jobMap;
    //Job* currFg;
    std::queue<std::string> delayedMsg;
    int lastExitStatus = 0;

    Context() {
        //currFg = nullptr;
    }

	std::pair<JobMap::iterator, int> getJob(pid_t pid);
	template<typename... Args>
	int /*jobid*/ addJob(Args&&... args);
	
	void onProcessExited(pid_t pid, int statloc, bool bg);
	void onProcessStopped(pid_t pid, int statloc, bool bg);
	void onProcessSignaled(pid_t pid, int statloc, bool bg);

private:
	int minUsableJobId();
};


template<typename... Args>
int Context::addJob(Args&&... args)
{
	int jid = minUsableJobId();
	jobIdUsed[jid] = true;
	auto pr = jobMap.insert({jid, {std::forward<Args>(args)...}});
	assert(pr.second);
	return jid;
}

#endif //CONTEXT_H__
