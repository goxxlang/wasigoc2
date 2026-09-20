package loader

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"strconv"
	"strings"

	"github.com/tetratelabs/wazero"
	"github.com/tetratelabs/wazero/api"
)

// stringImport is one WithStringImport registration.
type stringImport struct {
	name  string
	arity int
	fn    func(args []string) (string, error)
}

func instantiateEnv(ctx context.Context, rt wazero.Runtime, stdout io.Writer, imports []stringImport) error {
	if stdout == nil {
		stdout = io.Discard
	}
	b := rt.NewHostModuleBuilder("env").
		NewFunctionBuilder().
		WithFunc(func(_ context.Context, m api.Module, ptr, length uint32) {
			if mem := m.Memory(); mem != nil {
				if buf, ok := mem.Read(ptr, length); ok {
					_, _ = stdout.Write(buf)
				}
			}
		}).
		Export("log").
		NewFunctionBuilder().
		WithFunc(func(ctx context.Context, m api.Module, code uint32) {
			_ = m.CloseWithExitCode(ctx, code)
		}).
		Export("abort")

	for _, si := range imports {
		si := si
		paramTypes := make([]api.ValueType, si.arity*2)
		for i := range paramTypes {
			paramTypes[i] = api.ValueTypeI32
		}
		b = b.NewFunctionBuilder().
			WithGoModuleFunction(
				api.GoModuleFunc(func(ctx context.Context, mod api.Module, stack []uint64) {
					args := make([]string, si.arity)
					mem := mod.Memory()
					for i := 0; i < si.arity; i++ {
						ptr, length := uint32(stack[i*2]), uint32(stack[i*2+1])
						if length == 0 || mem == nil {
							continue
						}
						if buf, ok := mem.Read(ptr, length); ok {
							args[i] = string(buf)
						}
					}
					result, err := si.fn(args)
					stack[0] = writeGuestEnvelope(ctx, mod, result, err)
				}),
				paramTypes, []api.ValueType{api.ValueTypeI64},
			).
			Export(si.name)
	}

	_, err := b.Instantiate(ctx)
	return err
}

type wireEnvelope struct {
	OK     bool   `json:"ok"`
	Result string `json:"result,omitempty"`
	Error  string `json:"error,omitempty"`
}

// writeGuestEnvelope encodes (result, err) as the wire envelope and writes
// it into a buffer obtained by calling the CALLING guest's own "alloc"
// export, returning the packed (ptr<<32|len) result every string import
// must produce. The guest is responsible for freeing that buffer.
func writeGuestEnvelope(ctx context.Context, mod api.Module, result string, err error) uint64 {
	env := wireEnvelope{OK: err == nil, Result: result}
	if err != nil {
		env.Error = err.Error()
	}
	body, _ := json.Marshal(env)

	allocFn := mod.ExportedFunction("alloc")
	if allocFn == nil {
		return 0
	}
	res, callErr := allocFn.Call(ctx, uint64(len(body)))
	if callErr != nil || len(res) != 1 {
		return 0
	}
	ptr := uint32(res[0])
	if len(body) > 0 {
		if mem := mod.Memory(); mem != nil {
			mem.Write(ptr, body)
		}
	}
	return uint64(ptr)<<32 | uint64(len(body))
}

func parseValue(s string, vt api.ValueType) (uint64, error) {
	s = strings.TrimSpace(s)
	switch vt {
	case api.ValueTypeI32:
		n, err := strconv.ParseInt(s, 0, 32)
		if err != nil {
			u, uerr := strconv.ParseUint(s, 0, 32)
			if uerr != nil {
				return 0, err
			}
			return api.EncodeU32(uint32(u)), nil
		}
		return api.EncodeI32(int32(n)), nil
	case api.ValueTypeI64:
		n, err := strconv.ParseInt(s, 0, 64)
		if err != nil {
			u, uerr := strconv.ParseUint(s, 0, 64)
			if uerr != nil {
				return 0, err
			}
			return u, nil
		}
		return api.EncodeI64(n), nil
	case api.ValueTypeF32:
		f, err := strconv.ParseFloat(s, 32)
		if err != nil {
			return 0, err
		}
		return api.EncodeF32(float32(f)), nil
	case api.ValueTypeF64:
		f, err := strconv.ParseFloat(s, 64)
		if err != nil {
			return 0, err
		}
		return api.EncodeF64(f), nil
	default:
		return 0, fmt.Errorf("unsupported value type %s", api.ValueTypeName(vt))
	}
}

func formatValue(v uint64, vt api.ValueType) string {
	switch vt {
	case api.ValueTypeI32:
		return strconv.FormatInt(int64(api.DecodeI32(v)), 10)
	case api.ValueTypeI64:
		return strconv.FormatInt(int64(v), 10)
	case api.ValueTypeF32:
		return strconv.FormatFloat(float64(api.DecodeF32(v)), 'g', -1, 32)
	case api.ValueTypeF64:
		return strconv.FormatFloat(api.DecodeF64(v), 'g', -1, 64)
	default:
		return strconv.FormatUint(v, 10)
	}
}
