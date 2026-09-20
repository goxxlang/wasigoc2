package wasmbin

import (
	"fmt"
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
	for i, t := range ts {
		parts[i] = t.String()
	}
	return strings.Join(parts, ", ")
}

func formatLimits(l Limits) string {
	if l.Max != nil {
		return fmt.Sprintf("memory {%d..%d pages}", l.Min, *l.Max)
	}
	return fmt.Sprintf("memory {%d.. pages}", l.Min)
}

func HexPrefix(b []byte, n int) string {
	if n > len(b) {
		n = len(b)
	}
	parts := make([]string, n)
	for i := 0; i < n; i++ {
		parts[i] = fmt.Sprintf("%02x", b[i])
	}
	return strings.Join(parts, " ")
}
