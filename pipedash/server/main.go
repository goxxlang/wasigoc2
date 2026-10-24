// pipedash server. It runs in WSL next to the pipelines, serves public/ and
// turns a run directory into the data part of the pack the go++ module reads
// on stdin (tab-separated lines, one record per line). It only reads run
// files; it never starts, stops or writes a pipeline.
//
//	go run . -addr :3092 -root ../public -line declut=$HOME/declut_runs/dl1 -line ramgen=$HOME/ramgen_runs -line ieda=$IEDA_RUNS
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
)

type lineFlag []string

func (l *lineFlag) String() string     { return strings.Join(*l, ",") }
func (l *lineFlag) Set(v string) error { *l = append(*l, v); return nil }

type line struct{ name, base string }

var lines []line

func main() {
	home := os.Getenv("HOME")
	addr := flag.String("addr", ":3092", "listen address")
	root := flag.String("root", "../public", "files to serve")
	var lf lineFlag
	flag.Var(&lf, "line", "name=run base (repeatable)")
	flag.Parse()
	if len(lf) == 0 {
		lf = lineFlag{
			"declut=" + filepath.Join(home, "declut_runs", "dl1"),
			"ramgen=" + filepath.Join(home, "ramgen_runs"),
			"ieda=" + iedaRuns(),
		}
	}
	for _, s := range lf {
		n, b, ok := strings.Cut(s, "=")
		if !ok {
			log.Fatalf("-line %q: want name=dir", s)
		}
		lines = append(lines, line{n, b})
	}
	abs, err := filepath.Abs(*root)
	if err != nil {
		log.Fatal(err)
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, r *http.Request) { w.Write([]byte("ok\n")) })
	mux.HandleFunc("/data", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		w.Header().Set("Cache-Control", "no-store")
		w.Write([]byte(data(r.URL.Query().Get("line"), r.URL.Query().Get("tag"))))
	})
	mux.HandleFunc("/chipview", serveChipview)
	fs := http.FileServer(http.Dir(abs))
	mux.Handle("/", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if strings.HasSuffix(r.URL.Path, ".wasm") {
			w.Header().Set("Content-Type", "application/wasm")
		}
		w.Header().Set("Cache-Control", "no-cache")
		fs.ServeHTTP(w, r)
	}))
	log.Printf("pipedash on %s, root %s, lines %v", *addr, abs, lines)
	log.Fatal(http.ListenAndServe(*addr, mux))
}

// rec writes one record; fields lose tabs and newlines.
func rec(b *strings.Builder, fields ...string) {
	for i, f := range fields {
		if i > 0 {
			b.WriteByte('\t')
		}
		f = strings.NewReplacer("\t", " ", "\r", "", "\n", " ").Replace(f)
		b.WriteString(f)
	}
	b.WriteByte('\n')
}

type runInfo struct {
	tag   string
	dir   string
	state string // running, pass, fail, stopped, unknown
	mod   time.Time
}

func runsOf(base string) []runInfo {
	ents, _ := os.ReadDir(base)
	var out []runInfo
	for _, e := range ents {
		if !e.IsDir() {
			continue
		}
		dir := filepath.Join(base, e.Name())
		ev := filepath.Join(dir, "pipeline", "events.log")
		st, err := os.Stat(ev)
		if err != nil {
			continue
		}
		out = append(out, runInfo{e.Name(), dir, runState(ev, st.ModTime()), st.ModTime()})
	}
	sort.Slice(out, func(i, j int) bool { return out[i].mod.After(out[j].mod) })
	return out
}

// runState reads the end of events.log: the pipeline's own last word, or a
// monitor line written in the last 5 minutes (it ticks every 2).
func runState(ev string, mod time.Time) string {
	tail := tailLines(ev, 40)
	for i := len(tail) - 1; i >= 0; i-- {
		if strings.Contains(tail[i], " pipeline PASS") {
			return "pass"
		}
		if strings.Contains(tail[i], " pipeline FAIL") {
			return "fail"
		}
	}
	if time.Since(mod) < 5*time.Minute {
		return "running"
	}
	return "stopped"
}

func tailLines(path string, n int) []string {
	f, err := os.Open(path)
	if err != nil {
		return nil
	}
	defer f.Close()
	st, _ := f.Stat()
	off := st.Size() - 64*1024
	if off < 0 {
		off = 0
	}
	f.Seek(off, 0)
	var all []string
	sc := bufio.NewScanner(f)
	sc.Buffer(make([]byte, 1<<20), 1<<20)
	for sc.Scan() {
		all = append(all, sc.Text())
	}
	if off > 0 && len(all) > 0 {
		all = all[1:]
	}
	if len(all) > n {
		all = all[len(all)-n:]
	}
	return all
}

var (
	reStart = regexp.MustCompile(`^(\d\d:\d\d:\d\d) step (\S+): start`)
	reDone  = regexp.MustCompile(`^(\d\d:\d\d:\d\d) step (\S+): (PASS|FAIL) in (\d+)s`)
	reEvent = regexp.MustCompile(`^\d\d:\d\d:\d\d (event|monitor) `)
)

type stage struct {
	name, state, start, end string
	secs                    int
}

func data(lineName, tag string) string {
	var b strings.Builder
	rec(&b, "now", strconv.FormatInt(time.Now().Unix(), 10), time.Now().Format("2006-01-02 15:04:05"))
	var cur *line
	for i := range lines {
		rec(&b, "line", lines[i].name, lines[i].base)
		if lines[i].name == lineName || (lineName == "" && cur == nil) {
			cur = &lines[i]
		}
	}
	if cur == nil {
		rec(&b, "error", "no line "+lineName)
		return b.String()
	}
	rec(&b, "sel_line", cur.name)
	runs := runsOf(cur.base)
	var sel *runInfo
	for i := range runs {
		r := runs[i]
		rec(&b, "run", r.tag, r.state, r.mod.Format("01-02 15:04"))
		if r.tag == tag || (tag == "" && sel == nil) {
			sel = &runs[i]
		}
	}
	if sel == nil {
		rec(&b, "error", "no runs under "+cur.base)
		return b.String()
	}
	rec(&b, "sel", sel.tag, sel.state, sel.dir)
	if href := chipviewHref(cur.name, sel.tag); href != "" {
		rec(&b, "chipview", href)
	}
	p := func(n string) string { return filepath.Join(sel.dir, "pipeline", n) }
	if s, err := os.ReadFile(p("seeded_from.txt")); err == nil {
		rec(&b, "seeded", strings.TrimSpace(string(s)))
	}
	pbPath := ""
	if s, err := os.ReadFile(p("playbook")); err == nil {
		pbPath = strings.TrimSpace(string(s))
		rec(&b, "playbook", pbPath)
	}

	// stages: the playbook's order, then what events.log says happened
	order, pin := playbookStages(pbPath)
	if pin != "" {
		rec(&b, "proven", pin)
	}
	st := map[string]*stage{}
	for _, n := range order {
		st[n] = &stage{name: n, state: "wait"}
	}
	evs, _ := os.ReadFile(p("events.log"))
	var events, triage []string
	var pipeName, endLine string
	firstStep := ""
	for _, l := range strings.Split(string(evs), "\n") {
		if m := reStart.FindStringSubmatch(l); m != nil {
			s := st[m[2]]
			if s == nil {
				s = &stage{name: m[2]}
				st[m[2]] = s
				order = append(order, m[2])
			}
			if firstStep == "" {
				firstStep = m[2]
			}
			s.state, s.start, s.end, s.secs = "run", m[1], "", 0
		} else if m := reDone.FindStringSubmatch(l); m != nil {
			if s := st[m[2]]; s != nil {
				s.state, s.end = strings.ToLower(m[3]), m[1]
				s.secs, _ = strconv.Atoi(m[4])
			}
		}
		if strings.Contains(l, " triage ") {
			triage = append(triage, l[9:])
		}
		if strings.Contains(l, " pipeline ") && strings.Contains(l, " jobs, run ") {
			pipeName = l[9:]
		}
		if strings.Contains(l, " pipeline PASS") || strings.Contains(l, " pipeline FAIL") {
			endLine = l[9:]
		}
		if l != "" && !reEvent.MatchString(l) && !strings.Contains(l, " learn ") {
			events = append(events, l)
		}
	}
	if pipeName != "" {
		rec(&b, "pipeline", pipeName)
	}
	if endLine != "" {
		rec(&b, "end", endLine)
	}
	seeded := firstStep != ""
	for _, n := range order {
		s := st[n]
		if n == firstStep {
			seeded = false
		}
		state := s.state
		if state == "wait" && seeded && sel.state != "" {
			state = "seed" // ran in the run this one was seeded from
		}
		if state == "run" && sel.state != "running" {
			state = "dead" // started, never finished, pipeline gone
		}
		secs := s.secs
		if state == "run" {
			if cs, err := os.ReadFile(p("current")); err == nil {
				for _, kv := range strings.Split(string(cs), "\n") {
					if v, ok := strings.CutPrefix(kv, "start="); ok {
						if t, err := strconv.ParseInt(strings.TrimSpace(v), 10, 64); err == nil {
							secs = int(time.Now().Unix() - t)
						}
					}
				}
			}
		}
		rec(&b, "stage", n, state, strconv.Itoa(secs), s.start, s.end)
	}

	// gate verdicts: metrics and findings per step
	for _, n := range order {
		raw, err := os.ReadFile(p(n + ".verdict.json"))
		if err != nil {
			continue
		}
		var v struct {
			Pass     bool           `json:"pass"`
			Findings []string       `json:"findings"`
			Metrics  map[string]any `json:"metrics"`
		}
		if json.Unmarshal(raw, &v) != nil {
			continue
		}
		keys := make([]string, 0, len(v.Metrics))
		for k := range v.Metrics {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		for _, k := range keys {
			rec(&b, "vmetric", n, k, fmt.Sprint(v.Metrics[k]))
		}
		for _, f := range v.Findings {
			rec(&b, "finding", n, f)
		}
	}
	// metrics.txt: everything the stages measured, in the order written
	if raw, err := os.ReadFile(filepath.Join(sel.dir, "metrics.txt")); err == nil {
		for _, l := range strings.Split(string(raw), "\n") {
			k, v, ok := strings.Cut(strings.TrimSpace(l), " ")
			if ok {
				rec(&b, "metric", k, strings.TrimSpace(v))
			}
		}
	}
	for _, t := range triage {
		rec(&b, "triage", t)
	}
	// the monitor's last tick: elapsed, memory, tools, flow progress
	if ml := tailLines(p("events.log"), 200); len(ml) > 0 {
		for i := len(ml) - 1; i >= 0; i-- {
			if strings.Contains(ml[i], " monitor ") {
				rec(&b, "monitor", ml[i])
				break
			}
		}
	}
	// the flow's own log for the running step: last few lines
	if sel.state == "running" {
		if logs, _ := filepath.Glob(filepath.Join(sel.dir, "pnr", "*.log")); len(logs) > 0 {
			sort.Slice(logs, func(i, j int) bool { return modTime(logs[i]).After(modTime(logs[j])) })
			rec(&b, "flowlog", filepath.Base(logs[0]), modTime(logs[0]).Format("15:04:05"))
			for _, l := range tailLines(logs[0], 8) {
				rec(&b, "flowline", l)
			}
		}
	}
	if len(events) > 40 {
		events = events[len(events)-40:]
	}
	for _, e := range events {
		rec(&b, "event", e)
	}
	// lessons the decision layer queued for this line
	if raw, err := os.ReadFile(filepath.Join(cur.base, "lessons.md")); err == nil {
		for _, l := range strings.Split(string(raw), "\n") {
			l = strings.TrimSpace(l)
			if l == "" || strings.HasPrefix(l, "#") {
				continue
			}
			rec(&b, "lesson", l)
		}
	}
	return b.String()
}

// chipviewPath is the HTML chipview wrote into this run, when the file is there.
func chipviewPath(lineName, tag string) string {
	var cur *line
	for i := range lines {
		if lines[i].name == lineName || (lineName == "" && cur == nil) {
			cur = &lines[i]
		}
	}
	if cur == nil {
		return ""
	}
	runs := runsOf(cur.base)
	var sel *runInfo
	for i := range runs {
		if runs[i].tag == tag || (tag == "" && sel == nil) {
			sel = &runs[i]
		}
	}
	if sel == nil {
		return ""
	}
	p := filepath.Join(sel.dir, "chipview.html")
	st, err := os.Stat(p)
	if err != nil || st.IsDir() {
		return ""
	}
	return p
}

func chipviewHref(lineName, tag string) string {
	if chipviewPath(lineName, tag) == "" {
		return ""
	}
	q := url.Values{}
	q.Set("line", lineName)
	q.Set("tag", tag)
	return "/chipview?" + q.Encode()
}

func serveChipview(w http.ResponseWriter, r *http.Request) {
	p := chipviewPath(r.URL.Query().Get("line"), r.URL.Query().Get("tag"))
	if p == "" {
		http.NotFound(w, r)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	http.ServeFile(w, r, p)
}

func modTime(p string) time.Time {
	st, err := os.Stat(p)
	if err != nil {
		return time.Time{}
	}
	return st.ModTime()
}

// iedaRuns is the directory wasmdispatch writes for a container playbook.
// On WSL that is the /mnt/c view of WASMApollo/eda/runs.
func iedaRuns() string {
	if v := os.Getenv("IEDA_RUNS"); v != "" {
		return v
	}
	if st, err := os.Stat(`/mnt/c/Users/grego/WASMApollo/eda`); err == nil && st.IsDir() {
		return `/mnt/c/Users/grego/WASMApollo/eda/runs`
	}
	home, err := os.UserHomeDir()
	if err != nil {
		return filepath.Join("WASMApollo", "eda", "runs")
	}
	return filepath.Join(home, "WASMApollo", "eda", "runs")
}

// playbookStages returns the stage order and the proven-flow pin.
func playbookStages(path string) ([]string, string) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return nil, ""
	}
	var pb struct {
		Stack struct {
			Stages []struct {
				Name string `json:"name"`
			} `json:"stages"`
		} `json:"stack"`
		Proven *struct {
			Dir string `json:"dir"`
		} `json:"proven_flow"`
	}
	if json.Unmarshal(raw, &pb) != nil {
		return nil, ""
	}
	var out []string
	for _, s := range pb.Stack.Stages {
		out = append(out, s.Name)
	}
	pin := ""
	if pb.Proven != nil {
		pin = pb.Proven.Dir
	}
	return out, pin
}
