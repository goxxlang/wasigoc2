// Port of RmlUi Source/Core/StyleSheetContainer.cpp and
// Include/RmlUi/Core/StyleSheetContainer.h.
package rmlui

// StyleSheetContainer is Rml::StyleSheetContainer: every media block of a
// style sheet, compiled into one StyleSheet for the active media queries.
type StyleSheetContainer struct {
	mediaBlocks              []*MediaBlock
	activeMediaBlockIndices  []int
	compiledStyleSheet       *StyleSheet
	hasCompiled              bool
}

func NewStyleSheetContainer() *StyleSheetContainer { return &StyleSheetContainer{} }

// LoadStyleSheetContainer parses stream into media blocks.
func (c *StyleSheetContainer) LoadStyleSheetContainer(stream *Stream, beginLineNumber int) bool {
	parser := NewStyleSheetParser()
	blocks, ok := parser.Parse(stream, beginLineNumber)
	c.mediaBlocks = append(c.mediaBlocks, blocks...)
	return ok
}

// UpdateCompiledStyleSheet recompiles when the set of matching media
// blocks changed; returns true if the compiled sheet changed.
func (c *StyleSheetContainer) UpdateCompiledStyleSheet(context *Context) bool {
	dpRatio := context.GetDensityIndependentPixelRatio()
	vpi := context.GetDimensions()
	vp := vpi.ToFloat()
	fontSize := DefaultComputedValues().FontSize()
	newActive := []int{}
	for index := 0; index < len(c.mediaBlocks); index++ {
		block := c.mediaBlocks[index]
		allMatch := true
		expected := block.Modifier != MediaQueryModifierNot
		props := block.Properties.GetProperties()
		for id := MediaQueryIdWidth; id < MediaQueryIdNumDefinedIds; id++ {
			property, has := props[id]
			if !has {
				continue
			}
			switch id {
			case MediaQueryIdWidth:
				if vp.X != ComputeLength(property.GetNumericValue(), fontSize, fontSize, dpRatio, vp) {
					allMatch = false
				}
			case MediaQueryIdMinWidth:
				if vp.X < ComputeLength(property.GetNumericValue(), fontSize, fontSize, dpRatio, vp) {
					allMatch = false
				}
			case MediaQueryIdMaxWidth:
				if vp.X > ComputeLength(property.GetNumericValue(), fontSize, fontSize, dpRatio, vp) {
					allMatch = false
				}
			case MediaQueryIdHeight:
				if vp.Y != ComputeLength(property.GetNumericValue(), fontSize, fontSize, dpRatio, vp) {
					allMatch = false
				}
			case MediaQueryIdMinHeight:
				if vp.Y < ComputeLength(property.GetNumericValue(), fontSize, fontSize, dpRatio, vp) {
					allMatch = false
				}
			case MediaQueryIdMaxHeight:
				if vp.Y > ComputeLength(property.GetNumericValue(), fontSize, fontSize, dpRatio, vp) {
					allMatch = false
				}
			case MediaQueryIdAspectRatio:
				ratio := property.Value.GetVector2f().ToInt()
				if vpi.X*ratio.Y != vpi.Y*ratio.X {
					allMatch = false
				}
			case MediaQueryIdMinAspectRatio:
				ratio := property.Value.GetVector2f().ToInt()
				if vpi.X*ratio.Y < vpi.Y*ratio.X {
					allMatch = false
				}
			case MediaQueryIdMaxAspectRatio:
				ratio := property.Value.GetVector2f().ToInt()
				if vpi.X*ratio.Y > vpi.Y*ratio.X {
					allMatch = false
				}
			case MediaQueryIdResolution:
				if dpRatio != property.Value.GetFloat() {
					allMatch = false
				}
			case MediaQueryIdMinResolution:
				if dpRatio < property.Value.GetFloat() {
					allMatch = false
				}
			case MediaQueryIdMaxResolution:
				if dpRatio > property.Value.GetFloat() {
					allMatch = false
				}
			case MediaQueryIdOrientation:
				// Landscape (x > y) = 0, portrait (x <= y) = 1.
				if (vp.X <= vp.Y) != property.Value.GetBool() {
					allMatch = false
				}
			case MediaQueryIdTheme:
				if !context.IsThemeActive(property.Value.GetString()) {
					allMatch = false
				}
			}
			if allMatch != expected {
				break
			}
		}
		if allMatch == expected {
			newActive = append(newActive, index)
		}
	}

	changed := !c.hasCompiled || !intListsEqual(newActive, c.activeMediaBlockIndices)
	if changed {
		var first *StyleSheet
		var combined *StyleSheet
		for _, index := range newActive {
			block := c.mediaBlocks[index]
			if first == nil {
				first = block.Stylesheet
			} else if combined == nil {
				combined = first.CombineStyleSheet(block.Stylesheet)
			} else {
				combined.MergeStyleSheet(block.Stylesheet)
			}
		}
		if first == nil {
			first = NewStyleSheet()
		}
		if combined != nil {
			c.compiledStyleSheet = combined
		} else {
			c.compiledStyleSheet = first
		}
		c.compiledStyleSheet.BuildNodeIndex()
		c.hasCompiled = true
	}
	c.activeMediaBlockIndices = newActive
	return changed
}

func intListsEqual(a []int, b []int) bool {
	if len(a) != len(b) {
		return false
	}
	for i := 0; i < len(a); i++ {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func (c *StyleSheetContainer) GetCompiledStyleSheet() *StyleSheet { return c.compiledStyleSheet }

// CombineStyleSheetContainer returns a new container with other's media
// blocks appended to this one's.
func (c *StyleSheetContainer) CombineStyleSheetContainer(other *StyleSheetContainer) *StyleSheetContainer {
	n := NewStyleSheetContainer()
	blocks := c.mediaBlocks
	for _, b := range blocks {
		n.mediaBlocks = append(n.mediaBlocks, &MediaBlock{Properties: b.Properties, Stylesheet: b.Stylesheet, Modifier: b.Modifier})
	}
	n.MergeStyleSheetContainer(other)
	return n
}

func (c *StyleSheetContainer) MergeStyleSheetContainer(other *StyleSheetContainer) {
	blocks := other.mediaBlocks
	for _, b := range blocks {
		c.mediaBlocks = append(c.mediaBlocks, &MediaBlock{Properties: b.Properties, Stylesheet: b.Stylesheet, Modifier: b.Modifier})
	}
}
