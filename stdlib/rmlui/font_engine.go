// Port of RmlUi Include/RmlUi/Core/FontEngineInterface.h.
//
// Glyph pixels are not produced here. WASMBlinker paints GML and RML text
// with wasmskia::DrawText (GetPaintFonts / FontSet). This interface is the
// metric half of that face — MeasureText, Ascent, Descent, LineGap,
// Advance, Kerning — so layout boxes match the glyphs Blinker draws.
// WASMRasta encodes the snapshot PNG and rasterizes SVG; it does not draw
// type. With no face installed, metrics are blink::FontMetrics::ForSize
// (ascent 0.8em, descent 0.2em) and a fixed advance table.
package rmlui

// FontEngineInterface is Rml::FontEngineInterface, shaped to match
// wasmskia::Font so a host can install Blinker's paint face.
type FontEngineInterface interface {
	LoadFontFace(file string, family string, style int, weight int, fallback bool, faceIndex int) bool
	GetFontFaceHandle(family string, style int, weight int, size int) FontFaceHandle
	GetLineHeight(handle FontFaceHandle) float32
	GetBaseline(handle FontFaceHandle) float32
	GetStringWidth(handle FontFaceHandle, text string) float32
	// MeasureText is wasmskia::MeasureText: total advance of text.
	MeasureText(handle FontFaceHandle, text string) float32
	// Ascent, Descent, and LineGap are wasmskia::Font at this face's size.
	// Ascent and descent are both positive; descent goes down from the baseline.
	Ascent(handle FontFaceHandle) float32
	Descent(handle FontFaceHandle) float32
	LineGap(handle FontFaceHandle) float32
	Advance(handle FontFaceHandle, codepoint int) float32
	Kerning(handle FontFaceHandle, left int, right int) float32
}

type fontFaceRecord struct {
	family   string
	style    int
	weight   int
	size     int
	fallback bool
}

// DefaultFontEngine is the face table used until the host calls
// SetFontEngineInterface.
type DefaultFontEngine struct {
	faces []fontFaceRecord
}

func NewDefaultFontEngine() *DefaultFontEngine { return &DefaultFontEngine{} }

func (f *DefaultFontEngine) LoadFontFace(file string, family string, style int, weight int, fallback bool, faceIndex int) bool {
	if file == "" && family == "" {
		return false
	}
	f.faces = append(f.faces, fontFaceRecord{family: family, style: style, weight: weight, size: 0, fallback: fallback})
	return true
}

func (f *DefaultFontEngine) GetFontFaceHandle(family string, style int, weight int, size int) FontFaceHandle {
	if size <= 0 {
		size = 12
	}
	for i := 0; i < len(f.faces); i++ {
		face := f.faces[i]
		if face.family == family && face.style == style && face.weight == weight && face.size == size {
			return i + 1
		}
	}
	f.faces = append(f.faces, fontFaceRecord{family: family, style: style, weight: weight, size: size})
	return len(f.faces)
}

func (f *DefaultFontEngine) sizeOf(handle FontFaceHandle) float32 {
	if handle <= 0 || handle > len(f.faces) {
		return 12
	}
	size := f.faces[handle-1].size
	if size <= 0 {
		return 12
	}
	return float32(size)
}

func (f *DefaultFontEngine) GetLineHeight(handle FontFaceHandle) float32 {
	return f.Ascent(handle) + f.Descent(handle) + f.LineGap(handle)
}

func (f *DefaultFontEngine) GetBaseline(handle FontFaceHandle) float32 {
	return f.Ascent(handle)
}

func (f *DefaultFontEngine) Ascent(handle FontFaceHandle) float32 {
	return f.sizeOf(handle) * 0.8
}

func (f *DefaultFontEngine) Descent(handle FontFaceHandle) float32 {
	return f.sizeOf(handle) * 0.2
}

func (f *DefaultFontEngine) LineGap(handle FontFaceHandle) float32 { return 0 }

func (f *DefaultFontEngine) Advance(handle FontFaceHandle, codepoint int) float32 {
	size := f.sizeOf(handle)
	if codepoint == ' ' {
		return size * 0.33
	}
	if codepoint > 0 && codepoint < 0x80 {
		return size * 0.5
	}
	return size
}

func (f *DefaultFontEngine) Kerning(handle FontFaceHandle, left int, right int) float32 {
	return 0
}

func (f *DefaultFontEngine) MeasureText(handle FontFaceHandle, text string) float32 {
	width := float32(0)
	prev := 0
	i := 0
	for i < len(text) {
		cp, n := decodeUTF8(text, i)
		if n <= 0 {
			break
		}
		if prev != 0 {
			width += f.Kerning(handle, prev, cp)
		}
		width += f.Advance(handle, cp)
		prev = cp
		i += n
	}
	return width
}

func (f *DefaultFontEngine) GetStringWidth(handle FontFaceHandle, text string) float32 {
	return f.MeasureText(handle, text)
}

func decodeUTF8(s string, i int) (int, int) {
	if i >= len(s) {
		return 0, 0
	}
	c0 := s[i]
	if c0 < 0x80 {
		return int(c0), 1
	}
	need := 0
	cp := 0
	if c0&0xE0 == 0xC0 {
		need = 2
		cp = int(c0 & 0x1F)
	} else if c0&0xF0 == 0xE0 {
		need = 3
		cp = int(c0 & 0x0F)
	} else if c0&0xF8 == 0xF0 {
		need = 4
		cp = int(c0 & 0x07)
	} else {
		return int(c0), 1
	}
	if i+need > len(s) {
		return int(c0), 1
	}
	for k := 1; k < need; k++ {
		ck := s[i+k]
		if ck&0xC0 != 0x80 {
			return int(c0), 1
		}
		cp = (cp << 6) | int(ck&0x3F)
	}
	return cp, need
}

var fontEngine FontEngineInterface

// GetFontEngineInterface is Rml::GetFontEngineInterface.
func GetFontEngineInterface() FontEngineInterface { return fontEngine }

// SetFontEngineInterface is Rml::SetFontEngineInterface.
func SetFontEngineInterface(engine FontEngineInterface) { fontEngine = engine }

// LoadFontFaceFromFile is the @font-face loader the style parser calls.
func LoadFontFaceFromFile(file string, family string, style int, weight int, fallback bool, faceIndex int) bool {
	engine := GetFontEngineInterface()
	if engine == nil {
		return false
	}
	return engine.LoadFontFace(file, family, style, weight, fallback, faceIndex)
}
