// pipedash: the chip-pipeline dashboard. guikit compiles the GML onto an
// rmlui tree and this program writes the page, the same way the
// encryptid.systems module does. The browser module reads a length-prefixed
// pack on stdin (mast, dash, css, data). data is what the server read from a
// run directory: tab-separated records, one per line. This program turns the
// records into GML files (runs, stages, metrics, ...) that dash.gml imports.
//
//	wasigocvm.bat pipedash\main.go -o pipedash\public\pipedash.wasm
package main

import (
	"fmt"
	"guikit"
	"os"
	"rmlui"
	"strconv"
	"strings"
)

type stage struct {
	name  string
	state string
	secs  int
	start string
	end   string
}

type kv struct {
	step string
	key  string
	val  string
}

// the records of one data part
type run struct {
	now      string
	lineSel  string
	lines    []string
	runs     [][]string
	sel      []string
	seeded   string
	playbook string
	proven   string
	pipeline string
	end      string
	stages   []stage
	vmetrics []kv
	metrics  []kv
	findings []kv
	triage   []string
	monitor  string
	flowlog  []string
	flowline []string
	events   []string
	lessons  []string
	errors   []string
}

func main() {
	mast, dash, css, data, srcErr := sources()
	if srcErr != "" {
		fmt.Fprintln(os.Stderr, srcErr)
		os.Exit(1)
	}
	r := parse(data)
	guikit.AddFile("mast.gml", mast)
	guikit.AddFile("lines.gml", linesGML(r))
	guikit.AddFile("runs.gml", runsGML(r))
	guikit.AddFile("head.gml", headGML(r))
	guikit.AddFile("stages.gml", stagesGML(r))
	guikit.AddFile("timeline.gml", timelineGML(r))
	guikit.AddFile("headline.gml", headlineGML(r))
	guikit.AddFile("metrics.gml", metricsGML(r))
	guikit.AddFile("findings.gml", findingsGML(r))
	guikit.AddFile("flowlog.gml", flowlogGML(r))
	guikit.AddFile("events.gml", eventsGML(r))
	guikit.AddFile("lessons.gml", lessonsGML(r))

	quiet := rmlui.NewDefaultSystemInterface()
	quiet.Quiet = true
	rmlui.SetSystemInterface(quiet)
	rmlui.Initialise()
	ctx := rmlui.CreateContext("pipedash", rmlui.Vector2i{1440, 900})
	doc, err := guikit.Load(ctx, dash)
	if err != nil || doc == nil {
		fmt.Fprintln(os.Stderr, "load", err)
		os.Exit(1)
	}
	need := []string{"masthead", "runlist", "rail", "metrics", "events"}
	for i := 0; i < len(need); i++ {
		if doc.GetElementById(need[i]) == nil {
			fmt.Fprintln(os.Stderr, "tree missing", need[i])
			os.Exit(1)
		}
	}
	title := "Pipelines"
	if len(r.sel) > 0 {
		title = r.lineSel + " " + r.sel[0] + " · " + r.sel[1]
	}
	var page string
	page = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
	page = page + "<meta charset=\"utf-8\">\n"
	page = page + "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
	page = page + "<title>" + escape(title) + "</title>\n"
	page = page + "<style>\n" + css + "\n</style>\n</head>\n<body>\n"
	page = page + render(doc.GetElement())
	page = page + "</body>\n</html>\n"
	fmt.Print(page)
}

func parse(data string) *run {
	r := &run{}
	rows := strings.Split(data, "\n")
	for i := 0; i < len(rows); i++ {
		if rows[i] == "" {
			continue
		}
		f := strings.Split(rows[i], "\t")
		k := f[0]
		if k == "now" && len(f) > 2 {
			r.now = f[2]
		} else if k == "line" && len(f) > 1 {
			r.lines = append(r.lines, f[1])
		} else if k == "sel_line" && len(f) > 1 {
			r.lineSel = f[1]
		} else if k == "run" && len(f) > 3 {
			r.runs = append(r.runs, f[1:])
		} else if k == "sel" && len(f) > 3 {
			r.sel = f[1:]
		} else if k == "seeded" && len(f) > 1 {
			r.seeded = f[1]
		} else if k == "playbook" && len(f) > 1 {
			r.playbook = f[1]
		} else if k == "proven" && len(f) > 1 {
			r.proven = f[1]
		} else if k == "pipeline" && len(f) > 1 {
			r.pipeline = f[1]
		} else if k == "end" && len(f) > 1 {
			r.end = f[1]
		} else if k == "stage" && len(f) > 5 {
			n, _ := strconv.Atoi(f[3])
			r.stages = append(r.stages, stage{name: f[1], state: f[2], secs: n, start: f[4], end: f[5]})
		} else if k == "vmetric" && len(f) > 3 {
			r.vmetrics = append(r.vmetrics, kv{step: f[1], key: f[2], val: f[3]})
		} else if k == "metric" && len(f) > 2 {
			step := f[1]
			dot := strings.Index(step, ".")
			if dot > 0 {
				step = step[:dot]
			}
			r.metrics = append(r.metrics, kv{step: step, key: f[1], val: f[2]})
		} else if k == "finding" && len(f) > 2 {
			r.findings = append(r.findings, kv{step: f[1], val: f[2]})
		} else if k == "triage" && len(f) > 1 {
			r.triage = append(r.triage, f[1])
		} else if k == "monitor" && len(f) > 1 {
			r.monitor = f[1]
		} else if k == "flowlog" && len(f) > 2 {
			r.flowlog = f[1:]
		} else if k == "flowline" && len(f) > 1 {
			r.flowline = append(r.flowline, f[1])
		} else if k == "event" && len(f) > 1 {
			r.events = append(r.events, f[1])
		} else if k == "lesson" && len(f) > 1 {
			r.lessons = append(r.lessons, f[1])
		} else if k == "error" && len(f) > 1 {
			r.errors = append(r.errors, f[1])
		}
	}
	return r
}

// q quotes s as a GML string.
func q(s string) string {
	s = strings.ReplaceAll(s, "\\", "\\\\")
	s = strings.ReplaceAll(s, "\"", "\\\"")
	return "\"" + s + "\""
}

func dur(secs int) string {
	if secs <= 0 {
		return ""
	}
	if secs < 60 {
		return strconv.Itoa(secs) + "s"
	}
	m := secs / 60
	if m < 60 {
		return strconv.Itoa(m) + "m" + pad2(secs%60) + "s"
	}
	return strconv.Itoa(m/60) + "h" + pad2(m%60) + "m"
}

func pad2(n int) string {
	if n < 10 {
		return "0" + strconv.Itoa(n)
	}
	return strconv.Itoa(n)
}

func stateWord(s string) string {
	if s == "pass" {
		return "passed"
	}
	if s == "fail" {
		return "failed"
	}
	if s == "run" || s == "running" {
		return "running"
	}
	if s == "seed" {
		return "seeded"
	}
	if s == "dead" {
		return "never finished"
	}
	if s == "stopped" {
		return "stopped"
	}
	return "waiting"
}

func linesGML(r *run) string {
	out := ""
	for i := 0; i < len(r.lines); i++ {
		cls := "a.linetab"
		if r.lines[i] == r.lineSel {
			cls = cls + ".on"
		}
		out = out + cls + " :href." + q("?line="+r.lines[i]) + " (" + q(r.lines[i]) + ")\n"
	}
	if r.now != "" {
		out = out + "span.clock (" + q("read "+r.now) + ")\n"
	}
	return out
}

func runsGML(r *run) string {
	if len(r.runs) == 0 {
		return "p.empty (\"No runs under this line.\")\n"
	}
	out := ""
	for i := 0; i < len(r.runs); i++ {
		f := r.runs[i]
		cls := "a.runrow." + f[1]
		if len(r.sel) > 0 && f[0] == r.sel[0] {
			cls = cls + ".on"
		}
		out = out + cls + " :href." + q("?line="+r.lineSel+"&tag="+f[0]) + " (\n"
		out = out + "  span.pip ()\n"
		out = out + "  span.tag (" + q(f[0]) + ")\n"
		out = out + "  span.when (" + q(f[2]) + ")\n"
		out = out + ")\n"
	}
	return out
}

func headGML(r *run) string {
	if len(r.sel) == 0 {
		out := "div.runhead ( h1 (\"No run\") )\n"
		for i := 0; i < len(r.errors); i++ {
			out = out + "p.err (" + q(r.errors[i]) + ")\n"
		}
		return out
	}
	tag := r.sel[0]
	state := r.sel[1]
	out := "div.runhead (\n"
	out = out + "  div.title ( h1 (" + q(tag) + ") span.badge." + state + " (" + q(stateWord(state)) + ") )\n"
	out = out + "  dl.facts (\n"
	cur := ""
	for i := 0; i < len(r.stages); i++ {
		if r.stages[i].state == "run" {
			cur = r.stages[i].name + " for " + dur(r.stages[i].secs)
		}
	}
	if cur != "" {
		out = out + "    div ( dt (\"Running\") dd.now (" + q(cur) + ") )\n"
	}
	if r.end != "" {
		out = out + "    div ( dt (\"Result\") dd (" + q(r.end) + ") )\n"
	}
	if r.seeded != "" {
		out = out + "    div ( dt (\"Seeded\") dd (" + q(r.seeded) + ") )\n"
	}
	if r.proven != "" {
		out = out + "    div ( dt (\"Flow\") dd.proven (" + q(r.proven+" (pinned, proven)") + ") )\n"
	} else if r.playbook != "" {
		out = out + "    div ( dt (\"Flow\") dd.unpinned (\"playbook pins no proven flow\") )\n"
	}
	if r.playbook != "" {
		out = out + "    div ( dt (\"Playbook\") dd.path (" + q(r.playbook) + ") )\n"
	}
	out = out + "    div ( dt (\"Directory\") dd.path (" + q(r.sel[2]) + ") )\n"
	out = out + "  )\n"
	if r.monitor != "" {
		out = out + "  p.monitor (" + q(r.monitor) + ")\n"
	}
	out = out + ")\n"
	return out
}

func stagesGML(r *run) string {
	out := ""
	for i := 0; i < len(r.stages); i++ {
		s := r.stages[i]
		tip := s.name + ": " + stateWord(s.state)
		if s.start != "" {
			tip = tip + ", started " + s.start
		}
		if s.end != "" {
			tip = tip + ", ended " + s.end
		}
		out = out + "div.stage." + s.state + " :title." + q(tip) + " (\n"
		out = out + "  span.node ()\n"
		out = out + "  span.name (" + q(s.name) + ")\n"
		d := dur(s.secs)
		if d == "" {
			d = stateWord(s.state)
		}
		out = out + "  span.dur (" + q(d) + ")\n"
		out = out + ")\n"
	}
	return out
}

// timelineGML draws the steps this run executed, widths by wall time.
func timelineGML(r *run) string {
	total := 0
	for i := 0; i < len(r.stages); i++ {
		if r.stages[i].secs > 0 {
			total = total + r.stages[i].secs
		}
	}
	if total == 0 {
		return "p.empty (\"Nothing has run in this run yet.\")\n"
	}
	out := ""
	for i := 0; i < len(r.stages); i++ {
		s := r.stages[i]
		if s.secs <= 0 {
			continue
		}
		pct := s.secs * 1000 / total
		w := strconv.Itoa(pct/10) + "." + strconv.Itoa(pct%10) + "%"
		out = out + "div.seg." + s.state + " :style." + q("width: "+w) + " :title." + q(s.name+" "+dur(s.secs)) + " ( span (" + q(s.name) + ") )\n"
	}
	return out + "span.total (" + q("wall time "+dur(total)) + ")\n"
}

// metric returns the last value written for key, from metrics.txt or a verdict.
func metric(r *run, key string) string {
	v := ""
	for i := 0; i < len(r.metrics); i++ {
		if r.metrics[i].key == key {
			v = r.metrics[i].val
		}
	}
	if v != "" {
		return v
	}
	for i := 0; i < len(r.vmetrics); i++ {
		if r.vmetrics[i].key == key {
			v = r.vmetrics[i].val
		}
	}
	return v
}

func valClass(key string, v string) string {
	if strings.HasPrefix(v, "-") && (strings.HasSuffix(key, "_ws") || strings.Contains(key, "slack")) {
		return ".neg"
	}
	if strings.HasSuffix(key, "_ws") {
		return ".pos"
	}
	if v == "mismatch" {
		return ".neg"
	}
	return ""
}

// headlineGML: the numbers a sign-off is decided on, where this run has them.
func headlineGML(r *run) string {
	keys := []string{
		"floorplan.die", "floorplan.unplaced_pins",
		"place.setup_ws", "cts.setup_ws", "cts.hold_ws",
		"grt.setup_ws", "route.drc_markers", "route.antenna_violations",
		"signoff.setup_ws", "signoff.hold_ws", "signoff.corners",
		"gds.xor_differences", "drc.klayout_markers", "drc.magic_markers", "lvs.result",
	}
	labels := []string{
		"Die, µm", "Unplaced pins",
		"Place setup, ns", "CTS setup, ns", "CTS hold, ns",
		"GRT setup, ns", "Route DRC", "Antenna nets",
		"Signoff setup, ns", "Signoff hold, ns", "Corners",
		"XOR", "KLayout DRC", "Magic DRC", "LVS",
	}
	out := ""
	for i := 0; i < len(keys); i++ {
		v := metric(r, keys[i])
		if v == "" {
			continue
		}
		out = out + "div.fig ( span.num" + valClass(keys[i], v) + " (" + q(v) + ") span.lab (" + q(labels[i]) + ") )\n"
	}
	if out == "" {
		return "p.empty (\"No sign-off numbers yet.\")\n"
	}
	return out
}

func metricsGML(r *run) string {
	if len(r.metrics) == 0 && len(r.vmetrics) == 0 {
		return "p.empty (\"No metrics yet.\")\n"
	}
	out := "table.metrics (\n  thead ( tr ( th (\"Step\") th (\"Metric\") th (\"Value\") ) )\n  tbody (\n"
	last := ""
	src := r.metrics
	if len(src) == 0 {
		src = r.vmetrics
	}
	for i := 0; i < len(src); i++ {
		m := src[i]
		step := ""
		if m.step != last {
			step = m.step
			last = m.step
		}
		cls := "tr"
		if step != "" {
			cls = "tr.first"
		}
		out = out + "    " + cls + " ( td.step (" + q(step) + ") td.key (" + q(m.key) + ") td.val" + valClass(m.key, m.val) + " (" + q(m.val) + ") )\n"
	}
	return out + "  )\n)\n"
}

func findingsGML(r *run) string {
	if len(r.findings) == 0 && len(r.triage) == 0 {
		return "p.clean (\"No gate findings in this run.\")\n"
	}
	out := ""
	for i := 0; i < len(r.findings); i++ {
		out = out + "div.finding ( span.step (" + q(r.findings[i].step) + ") span.text (" + q(r.findings[i].val) + ") )\n"
	}
	for i := 0; i < len(r.triage); i++ {
		out = out + "div.triage ( span.step (\"triage\") span.text (" + q(r.triage[i]) + ") )\n"
	}
	return out
}

func flowlogGML(r *run) string {
	if len(r.flowlog) < 2 {
		return "p.empty (\"No flow step is running.\")\n"
	}
	out := "p.logname (" + q("pnr/"+r.flowlog[0]+", written "+r.flowlog[1]) + ")\n"
	out = out + "div.log (\n"
	for i := 0; i < len(r.flowline); i++ {
		out = out + "  div.ln (" + q(r.flowline[i]) + ")\n"
	}
	return out + ")\n"
}

func eventsGML(r *run) string {
	if len(r.events) == 0 {
		return "p.empty (\"No events.\")\n"
	}
	out := ""
	for i := len(r.events) - 1; i >= 0; i-- {
		e := r.events[i]
		cls := "div.ev"
		if strings.Contains(e, ": FAIL") || strings.Contains(e, "pipeline FAIL") {
			cls = cls + ".fail"
		} else if strings.Contains(e, ": PASS") || strings.Contains(e, "pipeline PASS") {
			cls = cls + ".pass"
		} else if strings.Contains(e, " triage ") {
			cls = cls + ".triage"
		}
		t := ""
		rest := e
		if len(e) > 9 && e[2] == ':' && e[5] == ':' {
			t = e[:8]
			rest = e[9:]
		}
		out = out + cls + " ( span.t (" + q(t) + ") span.msg (" + q(rest) + ") )\n"
	}
	return out
}

func lessonsGML(r *run) string {
	if len(r.lessons) == 0 {
		return "p.empty (\"No lessons recorded for this line.\")\n"
	}
	out := ""
	for i := 0; i < len(r.lessons); i++ {
		l := r.lessons[i]
		cls := "div.lesson"
		if strings.HasPrefix(l, "REWEIGHT") || strings.HasPrefix(l, "RESUME") || strings.HasPrefix(l, "LABEL") {
			cls = cls + ".queued"
		} else if strings.HasPrefix(l, "calibration") {
			cls = cls + ".cal"
		}
		out = out + cls + " (" + q(l) + ")\n"
	}
	return out
}

func sources() (string, string, string, string, string) {
	pack := readStdin()
	if strings.HasPrefix(pack, "mast\n") {
		i := 0
		var ok bool
		var mast, dash, css, data string
		mast, i, ok = takePart(pack, i, "mast")
		if ok {
			dash, i, ok = takePart(pack, i, "dash")
		}
		if ok {
			css, i, ok = takePart(pack, i, "css")
		}
		if ok {
			data, i, ok = takePart(pack, i, "data")
		}
		if !ok {
			return "", "", "", "", "stdin pack is short"
		}
		return mast, dash, css, data, ""
	}
	mast, e1 := slurp("gml/mast.gml")
	dash, e2 := slurp("gml/dash.gml")
	css, e3 := slurp("dash.css")
	data, e4 := slurp("data.txt")
	if e1 != "" || e2 != "" || e3 != "" || e4 != "" {
		return "", "", "", "", e1 + e2 + e3 + e4
	}
	return mast, dash, css, data, ""
}

func slurp(path string) (string, string) {
	buf, err := os.ReadFile(path)
	if err != nil {
		return "", path + " unreadable "
	}
	return string(buf), ""
}

func readStdin() string {
	out := ""
	buf := make([]byte, 8192)
	for {
		n, err := os.Stdin.Read(buf)
		if n > 0 {
			out = out + string(buf[:n])
		}
		if err != nil || n == 0 {
			break
		}
	}
	return out
}

func readLine(s string, i int) (string, int, bool) {
	if i >= len(s) {
		return "", i, false
	}
	j := strings.Index(s[i:], "\n")
	if j < 0 {
		return s[i:], len(s), true
	}
	return s[i : i+j], i + j + 1, true
}

func takePart(pack string, i int, want string) (string, int, bool) {
	name, i, ok := readLine(pack, i)
	if !ok || name != want {
		return "", i, false
	}
	lens, i, ok := readLine(pack, i)
	if !ok {
		return "", i, false
	}
	n, err := strconv.Atoi(lens)
	if err != nil || n < 0 || i+n > len(pack) {
		return "", i, false
	}
	return pack[i : i+n], i + n, true
}

func render(el *rmlui.Element) string {
	if el == nil {
		return ""
	}
	if el.GetTagName() == "#text" {
		text := rmlui.ElementTextOf(el)
		if text == nil {
			return ""
		}
		return escape(text.GetText())
	}
	tag := el.GetTagName()
	if tag == "" || tag == "body" {
		out := ""
		n := el.GetNumChildren(false)
		for i := 0; i < n; i++ {
			out = out + render(el.GetChild(i))
		}
		return out
	}
	out := "<" + tag
	names := el.GetAttributeNames()
	sortStrings(names)
	for i := 0; i < len(names); i++ {
		name := names[i]
		if name == "onclick" || name == "onsubmit" || name == "onchange" {
			continue
		}
		value := el.GetAttributeString(name, "")
		out = out + " " + name + "=\"" + escape(value) + "\""
	}
	out = out + ">"
	if tag == "br" || tag == "img" || tag == "meta" || tag == "link" {
		return out
	}
	n := el.GetNumChildren(false)
	for i := 0; i < n; i++ {
		out = out + render(el.GetChild(i))
	}
	return out + "</" + tag + ">"
}

func sortStrings(a []string) {
	for i := 1; i < len(a); i++ {
		for j := i; j > 0 && a[j] < a[j-1]; j-- {
			a[j], a[j-1] = a[j-1], a[j]
		}
	}
}

func escape(s string) string {
	s = strings.ReplaceAll(s, "&", "&amp;")
	s = strings.ReplaceAll(s, "<", "&lt;")
	s = strings.ReplaceAll(s, ">", "&gt;")
	s = strings.ReplaceAll(s, "\"", "&quot;")
	return s
}
