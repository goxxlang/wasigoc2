package wazgoc

import (
	"errors"

	"../wasmbinpkg"
)

var errJob = errors.New("job")

// Oilpan / cppgc job shape from ~/WASMv8Bindings Platform::PostJob.
// kUserBlocking runs on post; kBestEffort on Join. Functions, not
// methods, so wasigoc does not lift the interpreter into a Task.

const PrioBlocking = 0
const PrioBestEffort = 1

const jobCompile = 1
const jobExec = 2

type Job struct {
	kind  int
	prio  int
	ran   bool
	img   *wasmbin.Image
	idx   int
	slot  *compiledFn
	mod   *Module
	args  []uint64
	results []uint64
	err   error
}

func jobRun(j *Job) {
	if j == nil || j.ran {
		return
	}
	j.ran = true
	if j.kind == jobCompile {
		cf, e := compileOne(j.img, j.idx)
		if j.slot != nil {
			*j.slot = cf
		}
		j.err = e
		return
	}
	if j.kind == jobExec {
		if j.mod == nil || j.idx < 0 || j.idx >= len(j.mod.funcs) || j.mod.funcs[j.idx] == nil {
			j.err = errJob
			return
		}
		j.results, j.err = j.mod.execFn(*j.mod.funcs[j.idx], j.args)
	}
}

func jobJoin(j *Job) {
	jobRun(j)
}

func postJob(prio int, j *Job) *Job {
	j.prio = prio
	if prio == PrioBlocking {
		jobRun(j)
	}
	return j
}
