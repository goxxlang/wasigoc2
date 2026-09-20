package wasmbin

import (
	"strconv"
	"strings"
)

func formatFuncType(t FuncType) string {
	return "(" + joinTypes(t.Params) + ") -> (" + joinTypes(t.Results) + ")"
}

func joinTypes(ts []ValType) string {
	if len(ts) == 0 {
		return ""
	}
	parts := make([]string, len(ts))
	for i := 0; i < len(ts); i++ {
		parts[i] = ts[i].String()
	}
	return strings.Join(parts, ", ")
}

func formatLimits(l Limits) string {
	s := "memory {" + strconv.FormatUint(uint64(l.Min), 10)
	if l.HasMax {
		s = s + ".." + strconv.FormatUint(uint64(l.Max), 10)
	} else {
		s = s + ".."
	}
	s = s + " pages}"
	if l.Shared {
		s = s + " shared"
	}
	if l.Mem64 {
		s = s + " i64"
	}
	return s
}

func HexPrefix(b []byte, n int) string {
	if n > len(b) {
		n = len(b)
	}
	digits := "0123456789abcdef"
	out := ""
	for i := 0; i < n; i++ {
		if i > 0 {
			out = out + " "
		}
		v := b[i]
		out = out + string([]byte{digits[v>>4], digits[v&15]})
	}
	return out
}

func padRight(s string, n int) string {
	for len(s) < n {
		s = s + " "
	}
	return s
}

func padLeft(s string, n int) string {
	for len(s) < n {
		s = " " + s
	}
	return s
}

func InspectText(label string, raw []byte) (string, error) {
	img, err := Parse(raw)
	if err != nil {
		return "", err
	}
	core := GuestCore(raw)
	if len(core) < 8 {
		core = raw
	}
	out := label + "\n"
	out = out + "  magic    " + HexPrefix(core, 8) + "\n"
	out = out + "  version  " + strconv.FormatUint(uint64(img.Version), 10) + "\n"
	out = out + "  size     " + strconv.Itoa(img.Size) + " bytes\n"
	if img.ModuleName != "" {
		out = out + "  name     " + img.ModuleName + "\n"
	}
	out = out + "  sections\n"
	for i := 0; i < len(img.Sections); i++ {
		s := img.Sections[i]
		out = out + "    " + padRight(s.Name, 10) + " " + padLeft(strconv.FormatUint(uint64(s.Size), 10), 5) + " bytes @ " + strconv.Itoa(s.Off) + "\n"
	}
	if len(img.Imports) > 0 {
		out = out + "  imports\n"
		for i := 0; i < len(img.Imports); i++ {
			imp := img.Imports[i]
			out = out + "    " + imp.Module + "." + imp.Name + "  " + imp.Knd.String() + "  " + img.ImportType(imp) + "\n"
		}
	}
	if len(img.Exports) > 0 {
		out = out + "  exports\n"
		for i := 0; i < len(img.Exports); i++ {
			ex := img.Exports[i]
			out = out + "    " + ex.Name + "  " + ex.Knd.String() + "  " + img.ExportType(ex) + "\n"
		}
	}
	if img.HasStart {
		out = out + "  start    func " + strconv.FormatUint(uint64(img.Start), 10) + "\n"
	}
	named := 0
	for i := 0; i < len(img.FuncNames); i++ {
		if img.FuncNames[i] != "" {
			named++
		}
	}
	if named > 0 {
		out = out + "  functions\n"
		for i := 0; i < len(img.FuncNames); i++ {
			if img.FuncNames[i] == "" {
				continue
			}
			out = out + "    " + strconv.Itoa(i) + "  " + img.FuncNames[i] + "\n"
		}
	}
	for i := 0; i < len(img.Customs); i++ {
		c := img.Customs[i]
		out = out + "  custom   \"" + c.Name + "\" (" + strconv.Itoa(c.Size) + " bytes)\n"
	}
	return out, nil
}
