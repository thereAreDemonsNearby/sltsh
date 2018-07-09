#include "context.h"
#include <cassert>
#include <sys/wait.h>
#include <sstream>

template<typename T>
static inline auto toUInt(T t)
{
	return static_cast<typename std::underlying_type<T>::type>(t);
}

JobStatus resultTable[4][4] = {
	            /* Running    Stopped    Exited   Signaled*/
	/*Running*/ {JobStatus::Running, JobStatus::Running, JobStatus::Running, JobStatus::Running},
	/*Stopped*/ {JobStatus::Running, JobStatus::Stopped, JobStatus::Stopped, JobStatus::Stopped},	
	/*Exited  */{JobStatus::Running, JobStatus::Stopped, JobStatus::Finished, JobStatus::Finished},	
	/*Signaled*/{JobStatus::Running, JobStatus::Stopped, JobStatus::Finished, JobStatus::Finished},
};

int Context::minUsableJobId()
{
    int i;
    for (i = 0; i < jobIdUsed.size() && jobIdUsed[i]; ++i) {
    }

    if (i == jobIdUsed.size()) {
        jobIdUsed.push_back(false);
    }
    return i;
}


std::pair<Context::JobMap::iterator, int>
Context::getJob(pid_t pid)
{
	for (auto iter = jobMap.begin(); iter != jobMap.end(); ++iter) {
		auto& job = iter->second;
		if (job.pid[0] == pid) {
			return {iter, 0};
		}
		if (job.pid[1] != 0 && job.pid[1] == pid) {
			return {iter, 1};
		}
	}

	return {jobMap.end(), 0};
}

void Context::onProcessExited(pid_t pid, int statloc, bool bg)
{
	assert(WIFEXITED(statloc));
	auto [iter, idx] = getJob(pid);
	assert(iter != jobMap.end());
	lastExitStatus = WEXITSTATUS(statloc);
	auto& job = iter->second;	
	job.statInd[idx] = ProcStatus::Exited;
	job.eventStr[idx] = std::to_string((long)pid) +
		" exited with status " + std::to_string((long)WEXITSTATUS(statloc));
	auto origstat = job.stat;
	int another = 1 - idx;
	if (job.nproc == 2) {
		job.stat = resultTable[toUInt(job.statInd[idx])][toUInt(job.statInd[another])];
	} else {
		job.stat = JobStatus::Finished;
	}

	if (job.stat == JobStatus::Finished) {
		std::ostringstream os;
		os << "[" << iter->first << "] Finished\t" << job.jobCmd << "\n\t"
		   << job.eventStr[0] << "\n";
		if (job.nproc == 2)
			os << '\t' << job.eventStr[1] << "\n";
		if (!bg) {
			if (!(job.statInd[0] == ProcStatus::Exited &&
				(job.nproc == 1 || job.statInd[1] == ProcStatus::Exited))) 
			fprintf(stderr, "%s", os.str().c_str());
		} else {
			delayedMsg.push(os.str());
		}
		
		jobIdUsed[iter->first] = false;
		jobMap.erase(iter);
	} else if (job.stat == JobStatus::Stopped) {
		std::ostringstream os;
		os << "[" << iter->first << "] Stopped\n\t" << job.jobCmd << "\n\t"
		   << job.eventStr[0] << "\n";
		if (job.nproc == 2)
			os << '\t' << job.eventStr[1] << "\n";
		if (!bg) {
			fprintf(stderr, "%s", os.str().c_str());
		} else {
			delayedMsg.push(os.str());
		}
	}

	
}

void Context::onProcessStopped(pid_t pid, int statloc, bool bg)
{
	assert(WIFSTOPPED(statloc));
	auto [iter, idx] = getJob(pid);
	assert(iter != jobMap.end());

	auto& job = iter->second;
	job.statInd[idx] = ProcStatus::Stopped;
	lastExitStatus = 128 + WSTOPSIG(statloc);
	job.eventStr[idx] = std::to_string((long)pid) +
		" stopped by signal " + std::to_string((long)WSTOPSIG(statloc));
	int another = 1 - idx;
	auto origStat = job.stat;
	if (job.nproc == 2) {
		job.stat = resultTable[toUInt(job.statInd[idx])][toUInt(job.statInd[another])];
	} else {
		job.stat = JobStatus::Stopped;
	}

	if (job.stat == JobStatus::Stopped) {
		std::ostringstream os;
		os << "[" << iter->first << "] Stopped\t" << job.jobCmd << "\n\t"
		   << job.eventStr[0] << "\n";
		if (job.nproc == 2)
			os << '\t' << job.eventStr[1] << "\n";
		if (!bg) {
			fprintf(stderr, "%s", os.str().c_str());
		} else {
			delayedMsg.push(os.str());
		}
	}
}

void Context::onProcessSignaled(pid_t pid, int statloc, bool bg)
{
	assert(WIFSIGNALED(statloc));
	auto [iter, idx] = getJob(pid);
	assert(iter != jobMap.end());

	auto& job = iter->second;
	job.statInd[idx] = ProcStatus::Signaled;
	lastExitStatus = 128 + WTERMSIG(statloc);
	job.eventStr[idx] = std::to_string(pid) +
		"terminated by signal " + std::to_string(WTERMSIG(statloc));
	
	int another = 1 - idx;
	if (job.nproc == 2) {
		job.stat = resultTable[toUInt(job.statInd[idx])][toUInt(job.statInd[another])];
	} else {
		job.stat = JobStatus::Finished;
	}

	if (job.stat == JobStatus::Finished) {
		std::ostringstream os;
		os << "[" << iter->first << "] Finished\t" << job.jobCmd << "\n\t"
		   << job.eventStr[0] << "\n";
		if (job.nproc == 2)
			os << '\t' << job.eventStr[1] << "\n";
		if (!bg) {
			fprintf(stderr, "%s", os.str().c_str());
		} else {
			delayedMsg.push(os.str());
		}

		jobMap.erase(iter);
		jobIdUsed[iter->first] = false;
	} else if (job.stat == JobStatus::Stopped) {
		std::ostringstream os;
		os << "[" << iter->first << "] Stopped\n\t" << job.jobCmd << "\n\t"
		   << job.eventStr[0] << "\n";
		if (job.nproc == 2)
			os << '\t' << job.eventStr[1] << "\n";
		if (!bg) {
			fprintf(stderr, "%s", os.str().c_str());
		} else {
			delayedMsg.push(os.str());
		}
	}
}
