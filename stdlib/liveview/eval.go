package liveview

import (
	"html"
	"strconv"
	"strings"
)

func evalNodes(nodes []Node) string {
	var b strings.Builder
	for i := 0; i < len(nodes); i++ {
		b.WriteString(evalNode(nodes[i]))
	}
	return b.String()
}

func evalNode(n Node) string {
	if n.Kind == nText {
		return n.Text
	}
	tag := strings.ToLower(n.Tag)
	if tag == "rule" {
		sel := ""
		if len(n.Kids) > 0 {
			sel = evalNode(n.Kids[0])
		}
		var b strings.Builder
		b.WriteString(sel)
		b.WriteString(" {\n")
		for i := 1; i < len(n.Kids); i++ {
			b.WriteString("\t")
			b.WriteString(evalNode(n.Kids[i]))
			b.WriteString(";\n")
		}
		b.WriteString("}\n")
		return b.String()
	}
	var b strings.Builder
	b.WriteString("<")
	b.WriteString(tag)
	for i := 0; i < len(n.Attrs); i++ {
		b.WriteString(" ")
		b.WriteString(n.Attrs[i].Key)
		b.WriteString("=\"")
		b.WriteString(strings.ReplaceAll(n.Attrs[i].Val, `"`, "&quot;"))
		b.WriteString("\"")
	}
	if isVoid(tag) {
		b.WriteString(" />")
		return b.String()
	}
	b.WriteString(">")
	if tag == "style" || tag == "script" {
		b.WriteString("\n")
	}
	for i := 0; i < len(n.Kids); i++ {
		b.WriteString(evalNode(n.Kids[i]))
	}
	if tag == "style" || tag == "script" {
		b.WriteString("\n")
	}
	b.WriteString("</")
	b.WriteString(tag)
	b.WriteString(">")
	return b.String()
}

type tnode struct {
	kind     int
	text     string
	path     string
	iter     string
	filters  []string
	body     []tnode
	elseBody []tnode
}

const (
	tText  = 0
	tAct   = 1
	tIf    = 2
	tRange = 3
	tFor   = 4
)

type ttok struct {
	kind int
	text string
}

const (
	tkText  = 0
	tkGo    = 1
	tkJinja = 2
)

func execTmpl(src string, state map[string]any, vars map[string]any) string {
	toks := tokenizeTmpl(src)
	nodes, _, _ := buildTmpl(toks, 0)
	if vars == nil {
		vars = map[string]any{}
	}
	return runTmpl(nodes, state, state, vars)
}

func tokenizeTmpl(s string) []ttok {
	var toks []ttok
	i := 0
	for i < len(s) {
		jGo := strings.Index(s[i:], "{{")
		jJi := strings.Index(s[i:], "{%")
		j := -1
		isJi := false
		if jGo < 0 && jJi < 0 {
			toks = append(toks, ttok{kind: tkText, text: s[i:]})
			break
		}
		if jGo < 0 {
			j = jJi
			isJi = true
		} else if jJi < 0 {
			j = jGo
		} else if jJi < jGo {
			j = jJi
			isJi = true
		} else {
			j = jGo
		}
		if j > 0 {
			toks = append(toks, ttok{kind: tkText, text: s[i : i+j]})
		}
		if isJi {
			start := i + j + 2
			k := strings.Index(s[start:], "%}")
			if k < 0 {
				toks = append(toks, ttok{kind: tkText, text: s[i+j:]})
				break
			}
			toks = append(toks, ttok{kind: tkJinja, text: strings.TrimSpace(s[start : start+k])})
			i = start + k + 2
		} else {
			start := i + j + 2
			k := strings.Index(s[start:], "}}")
			if k < 0 {
				toks = append(toks, ttok{kind: tkText, text: s[i+j:]})
				break
			}
			toks = append(toks, ttok{kind: tkGo, text: strings.TrimSpace(s[start : start+k])})
			i = start + k + 2
		}
	}
	return toks
}

func buildTmpl(toks []ttok, pos int) ([]tnode, int, string) {
	var nodes []tnode
	for pos < len(toks) {
		t := toks[pos]
		if t.kind == tkText {
			nodes = append(nodes, tnode{kind: tText, text: t.text})
			pos = pos + 1
			continue
		}
		content := t.text
		if t.kind == tkGo {
			if content == "end" {
				return nodes, pos + 1, "end"
			}
			if content == "else" {
				return nodes, pos + 1, "else"
			}
			if strings.HasPrefix(content, "else if ") {
				return nodes, pos, "else"
			}
			if strings.HasPrefix(content, "if ") {
				path := strings.TrimSpace(content[3:])
				body, np, stop := buildTmpl(toks, pos+1)
				var elseBody []tnode
				if stop == "else" {
					if np < len(toks) && toks[np].kind == tkGo && strings.HasPrefix(toks[np].text, "else if ") {
						elif := toks[np]
						elifPath := strings.TrimSpace(elif.text[8:])
						inner, np2, _ := buildTmpl(toks, np+1)
						elseBody = []tnode{{kind: tIf, path: elifPath, body: inner, elseBody: nil}}
						if np2 <= len(toks) {
							rest, np3, st3 := buildTmpl(toks, np2)
							if st3 == "else" || len(rest) > 0 {
								if len(elseBody) == 1 {
									elseBody[0].elseBody = rest
								}
							}
							np = np3
						} else {
							np = np2
						}
					} else {
						elseBody, np, stop = buildTmpl(toks, np)
					}
				}
				nodes = append(nodes, tnode{kind: tIf, path: path, body: body, elseBody: elseBody})
				pos = np
				continue
			}
			if strings.HasPrefix(content, "range ") {
				path := strings.TrimSpace(content[6:])
				body, np, _ := buildTmpl(toks, pos+1)
				nodes = append(nodes, tnode{kind: tRange, path: path, body: body})
				pos = np
				continue
			}
			path, filters := splitPipe(content)
			nodes = append(nodes, tnode{kind: tAct, path: path, filters: filters})
			pos = pos + 1
			continue
		}
		if content == "endif" || content == "endfor" || content == "end" {
			return nodes, pos + 1, content
		}
		if content == "else" {
			return nodes, pos + 1, "else"
		}
		if strings.HasPrefix(content, "elif ") || strings.HasPrefix(content, "else if ") {
			return nodes, pos, "else"
		}
		if strings.HasPrefix(content, "if ") {
			path := strings.TrimSpace(content[3:])
			body, np, stop := buildTmpl(toks, pos+1)
			var elseBody []tnode
			if stop == "else" {
				if np < len(toks) && toks[np].kind == tkJinja && (strings.HasPrefix(toks[np].text, "elif ") || strings.HasPrefix(toks[np].text, "else if ")) {
					raw := toks[np].text
					elifPath := ""
					if strings.HasPrefix(raw, "elif ") {
						elifPath = strings.TrimSpace(raw[5:])
					} else {
						elifPath = strings.TrimSpace(raw[8:])
					}
					inner, np2, _ := buildTmpl(toks, np+1)
					elseBody = []tnode{{kind: tIf, path: elifPath, body: inner}}
					np = np2
				} else {
					elseBody, np, stop = buildTmpl(toks, np)
				}
			}
			nodes = append(nodes, tnode{kind: tIf, path: path, body: body, elseBody: elseBody})
			pos = np
			continue
		}
		if strings.HasPrefix(content, "for ") {
			rest := strings.TrimSpace(content[4:])
			iter := rest
			coll := rest
			idx := strings.Index(rest, " in ")
			if idx >= 0 {
				iter = strings.TrimSpace(rest[0:idx])
				coll = strings.TrimSpace(rest[idx+4:])
			}
			body, np, _ := buildTmpl(toks, pos+1)
			nodes = append(nodes, tnode{kind: tFor, path: coll, iter: iter, body: body})
			pos = np
			continue
		}
		path, filters := splitPipe(content)
		nodes = append(nodes, tnode{kind: tAct, path: path, filters: filters})
		pos = pos + 1
	}
	return nodes, pos, ""
}

func splitPipe(s string) (string, []string) {
	parts := strings.Split(s, "|")
	path := strings.TrimSpace(parts[0])
	var fs []string
	for i := 1; i < len(parts); i++ {
		fs = append(fs, strings.TrimSpace(parts[i]))
	}
	return path, fs
}

func runTmpl(nodes []tnode, state map[string]any, dot any, vars map[string]any) string {
	var b strings.Builder
	for i := 0; i < len(nodes); i++ {
		n := nodes[i]
		if n.kind == tText {
			b.WriteString(n.text)
			continue
		}
		if n.kind == tAct {
			v := lookupTmpl(n.path, state, dot, vars)
			v, safe := applyFilters(v, n.filters)
			s := stringifyRaw(v)
			if !safe {
				s = html.EscapeString(s)
			}
			b.WriteString(s)
			continue
		}
		if n.kind == tIf {
			v := lookupTmpl(n.path, state, dot, vars)
			if truthy(v) {
				b.WriteString(runTmpl(n.body, state, dot, vars))
			} else {
				b.WriteString(runTmpl(n.elseBody, state, dot, vars))
			}
			continue
		}
		if n.kind == tRange {
			v := lookupTmpl(n.path, state, dot, vars)
			arr, ok := v.([]any)
			if ok {
				for j := 0; j < len(arr); j++ {
					b.WriteString(runTmpl(n.body, state, arr[j], vars))
				}
			}
			continue
		}
		if n.kind == tFor {
			v := lookupTmpl(n.path, state, dot, vars)
			arr, ok := v.([]any)
			if ok {
				for j := 0; j < len(arr); j++ {
					nv := map[string]any{}
					for k, val := range vars {
						nv[k] = val
					}
					nv[n.iter] = arr[j]
					b.WriteString(runTmpl(n.body, state, arr[j], nv))
				}
			}
		}
	}
	return b.String()
}

func lookupTmpl(path string, state map[string]any, dot any, vars map[string]any) any {
	path = strings.TrimSpace(path)
	if path == "" || path == "." {
		return dot
	}
	if strings.HasPrefix(path, ".") {
		return walkAny(dot, strings.TrimPrefix(path, "."))
	}
	if vars != nil {
		if v, ok := vars[path]; ok {
			return v
		}
	}
	if state != nil {
		if v, ok := state[path]; ok {
			return v
		}
	}
	return walkAny(dot, path)
}

func walkAny(v any, path string) any {
	if path == "" {
		return v
	}
	parts := strings.Split(path, ".")
	cur := v
	for i := 0; i < len(parts); i++ {
		key := parts[i]
		if m, ok := cur.(map[string]any); ok {
			cur = m[key]
			continue
		}
		return nil
	}
	return cur
}

func applyFilters(v any, filters []string) (any, bool) {
	safe := false
	for i := 0; i < len(filters); i++ {
		f := filters[i]
		if f == "safe" {
			safe = true
			continue
		}
		if f == "upper" {
			v = strings.ToUpper(stringifyRaw(v))
			continue
		}
		if f == "lower" {
			v = strings.ToLower(stringifyRaw(v))
			continue
		}
		if f == "length" || f == "len" {
			v = valLen(v)
			continue
		}
		if f == "escape" {
			v = html.EscapeString(stringifyRaw(v))
			safe = true
			continue
		}
	}
	return v, safe
}

func valLen(v any) int {
	if s, ok := v.(string); ok {
		return len(s)
	}
	if a, ok := v.([]any); ok {
		return len(a)
	}
	if m, ok := v.(map[string]any); ok {
		return len(m)
	}
	return 0
}

func truthy(v any) bool {
	if v == nil {
		return false
	}
	if b, ok := v.(bool); ok {
		return b
	}
	if s, ok := v.(string); ok {
		return s != ""
	}
	if n, ok := v.(int); ok {
		return n != 0
	}
	if f, ok := v.(float64); ok {
		return f != 0
	}
	if a, ok := v.([]any); ok {
		return len(a) > 0
	}
	if m, ok := v.(map[string]any); ok {
		return len(m) > 0
	}
	return true
}

type execEnv struct {
	comp    *Component
	payload map[string]string
	locals  map[string]any
	dml     []DML
	ret     any
	didRet  bool
}

func runStmts(ex *execEnv, stmts []Stmt) {
	for i := 0; i < len(stmts); i++ {
		if ex.didRet {
			return
		}
		runStmt(ex, stmts[i])
	}
}

func runStmt(ex *execEnv, st Stmt) {
	if st.Kind == sAssign {
		rhs := evalExpr(ex, st.X)
		if st.Op == "=" {
			ex.setPath(st.Path, rhs)
			return
		}
		lhs := ex.resolvePath(st.Path)
		l := toInt(lhs)
		r := toInt(rhs)
		if st.Op == "+=" {
			ex.setPath(st.Path, l+r)
		} else if st.Op == "-=" {
			ex.setPath(st.Path, l-r)
		}
		return
	}
	if st.Kind == sIf {
		if truthy(evalExpr(ex, st.X)) {
			runStmts(ex, st.Body)
		} else {
			runStmts(ex, st.Else)
		}
		return
	}
	if st.Kind == sFor {
		col := ex.resolvePath(st.Coll)
		arr, ok := col.([]any)
		if !ok {
			return
		}
		for i := 0; i < len(arr); i++ {
			ex.locals[st.Iter] = arr[i]
			runStmts(ex, st.Body)
			if ex.didRet {
				return
			}
		}
		delete(ex.locals, st.Iter)
		return
	}
	if st.Kind == sReturn {
		ex.ret = evalExpr(ex, st.X)
		ex.didRet = true
		return
	}
	if st.Kind == sCall {
		evalExpr(ex, &Expr{Kind: xCall, Name: st.CallName, Args: st.Args})
	}
}

func evalExpr(ex *execEnv, x *Expr) any {
	if x == nil {
		return nil
	}
	if x.Kind == xLit {
		return x.Val
	}
	if x.Kind == xVar {
		return ex.resolvePath(x.Path)
	}
	if x.Kind == xUn {
		v := evalExpr(ex, x.Left)
		if x.Op == "!" {
			return !truthy(v)
		}
		if x.Op == "-" {
			return -toInt(v)
		}
		return v
	}
	if x.Kind == xBin {
		if x.Op == "&&" {
			return truthy(evalExpr(ex, x.Left)) && truthy(evalExpr(ex, x.Right))
		}
		if x.Op == "||" {
			return truthy(evalExpr(ex, x.Left)) || truthy(evalExpr(ex, x.Right))
		}
		l := evalExpr(ex, x.Left)
		r := evalExpr(ex, x.Right)
		if x.Op == "+" {
			li, lok := asInt(l)
			ri, rok := asInt(r)
			if lok && rok {
				return li + ri
			}
			return stringifyRaw(l) + stringifyRaw(r)
		}
		li := toInt(l)
		ri := toInt(r)
		if x.Op == "-" {
			return li - ri
		}
		if x.Op == "*" {
			return li * ri
		}
		if x.Op == "/" {
			if ri == 0 {
				return 0
			}
			return li / ri
		}
		if x.Op == "==" {
			return stringifyRaw(l) == stringifyRaw(r)
		}
		if x.Op == "!=" {
			return stringifyRaw(l) != stringifyRaw(r)
		}
		if x.Op == "<" {
			return li < ri
		}
		if x.Op == ">" {
			return li > ri
		}
		if x.Op == "<=" {
			return li <= ri
		}
		if x.Op == ">=" {
			return li >= ri
		}
		return nil
	}
	if x.Kind == xCall {
		return evalCall(ex, x.Name, x.Args)
	}
	return nil
}

func evalCall(ex *execEnv, name string, args []*Expr) any {
	var vals []any
	for i := 0; i < len(args); i++ {
		vals = append(vals, evalExpr(ex, args[i]))
	}
	if fn, ok := ex.comp.Funcs[name]; ok {
		child := &execEnv{
			comp:    ex.comp,
			payload: ex.payload,
			locals:  map[string]any{},
		}
		for i := 0; i < len(fn.Params); i++ {
			if i < len(vals) {
				child.locals[fn.Params[i]] = vals[i]
			}
		}
		runStmts(child, fn.Body)
		ex.dml = append(ex.dml, child.dml...)
		return child.ret
	}
	if name == "len" && len(vals) == 1 {
		return valLen(vals[0])
	}
	if name == "append" && len(vals) == 2 {
		arr, ok := vals[0].([]any)
		if !ok {
			return vals[0]
		}
		return append(arr, vals[1])
	}
	if name == "dml.append" && len(vals) == 2 {
		ex.dml = append(ex.dml, DML{Action: "append", TargetID: stringifyRaw(vals[0]), Value: stringifyRaw(vals[1])})
		return nil
	}
	if name == "dml.remove" && len(vals) == 1 {
		ex.dml = append(ex.dml, DML{Action: "remove", TargetID: stringifyRaw(vals[0])})
		return nil
	}
	if name == "dml.setValue" && len(vals) == 2 {
		ex.dml = append(ex.dml, DML{Action: "setValue", TargetID: stringifyRaw(vals[0]), Value: stringifyRaw(vals[1])})
		return nil
	}
	if name == "dml.addClass" && len(vals) == 2 {
		ex.dml = append(ex.dml, DML{Action: "addClass", TargetID: stringifyRaw(vals[0]), Value: stringifyRaw(vals[1])})
		return nil
	}
	if name == "dml.removeClass" && len(vals) == 2 {
		ex.dml = append(ex.dml, DML{Action: "removeClass", TargetID: stringifyRaw(vals[0]), Value: stringifyRaw(vals[1])})
		return nil
	}
	return nil
}

func (ex *execEnv) resolvePath(path string) any {
	parts := strings.Split(path, ".")
	if len(parts) == 0 {
		return nil
	}
	if parts[0] == "data" {
		if len(parts) == 2 {
			return ex.payload[parts[1]]
		}
		cur := any(ex.payload)
		for i := 1; i < len(parts); i++ {
			if m, ok := cur.(map[string]string); ok {
				cur = m[parts[i]]
				continue
			}
			return nil
		}
		return cur
	}
	if v, ok := ex.locals[parts[0]]; ok {
		return walkAny(v, strings.Join(parts[1:], "."))
	}
	if ex.comp != nil && ex.comp.State != nil {
		if v, ok := ex.comp.State[parts[0]]; ok {
			return walkAny(v, strings.Join(parts[1:], "."))
		}
	}
	return nil
}

func (ex *execEnv) setPath(path string, val any) {
	parts := strings.Split(path, ".")
	if len(parts) == 1 {
		if _, ok := ex.locals[parts[0]]; ok {
			ex.locals[parts[0]] = val
			return
		}
		if ex.comp != nil {
			if ex.comp.State == nil {
				ex.comp.State = map[string]any{}
			}
			ex.comp.State[parts[0]] = val
		}
		return
	}
	var cur any
	if v, ok := ex.locals[parts[0]]; ok {
		cur = v
	} else if ex.comp != nil {
		cur = ex.comp.State[parts[0]]
	}
	for i := 1; i < len(parts)-1; i++ {
		if m, ok := cur.(map[string]any); ok {
			cur = m[parts[i]]
		} else {
			return
		}
	}
	if m, ok := cur.(map[string]any); ok {
		m[parts[len(parts)-1]] = val
	}
}

func asInt(v any) (int, bool) {
	if n, ok := v.(int); ok {
		return n, true
	}
	if f, ok := v.(float64); ok {
		return int(f), true
	}
	if s, ok := v.(string); ok {
		n, err := strconv.Atoi(s)
		if err != nil {
			return 0, false
		}
		return n, true
	}
	return 0, false
}

func toInt(v any) int {
	n, _ := asInt(v)
	return n
}
