// Port of the block and inline half of RmlUi Source/Core/Layout
// (LayoutDetails::BuildBox and the block formatting context). Flex, grid,
// and table formatting contexts are not reproduced: those displays lay
// out as blocks (flex and table) or shrink-to-fit inline blocks.
package rmlui

// BuildBoxMode is LayoutDetails::BuildBoxMode.
const (
	BuildBoxModeBlock = iota
	BuildBoxModeInline
	BuildBoxModeUnalignedBlock
)

func displayIsBlock(display int) bool {
	switch display {
	case DisplayBlock, DisplayFlowRoot, DisplayFlex, DisplayTable, DisplayTableRow, DisplayTableRowGroup, DisplayTableColumnGroup:
		return true
	}
	return false
}

func displayShrinks(display int) bool {
	switch display {
	case DisplayInlineBlock, DisplayInlineFlex, DisplayInlineTable:
		return true
	}
	return false
}

// LayoutDetailsBuildBox is LayoutDetails::BuildBox.
func LayoutDetailsBuildBox(containingBlock Vector2f, element *Element, mode int) Box {
	computed := element.GetComputedValues()
	box := NewBox((Vector2f{-1, -1}))
	box.SetEdge(BoxAreaBorder, BoxEdgeTop, computed.BorderTopWidth())
	box.SetEdge(BoxAreaBorder, BoxEdgeRight, computed.BorderRightWidth())
	box.SetEdge(BoxAreaBorder, BoxEdgeBottom, computed.BorderBottomWidth())
	box.SetEdge(BoxAreaBorder, BoxEdgeLeft, computed.BorderLeftWidth())
	box.SetEdge(BoxAreaPadding, BoxEdgeTop, ResolveValue(computed.PaddingTop(), containingBlock.Y))
	box.SetEdge(BoxAreaPadding, BoxEdgeRight, ResolveValue(computed.PaddingRight(), containingBlock.X))
	box.SetEdge(BoxAreaPadding, BoxEdgeBottom, ResolveValue(computed.PaddingBottom(), containingBlock.Y))
	box.SetEdge(BoxAreaPadding, BoxEdgeLeft, ResolveValue(computed.PaddingLeft(), containingBlock.X))

	resolveMargin := func(edge BoxEdge, length LengthPercentageAuto, base float32) {
		if length.Type == LengthPercentageAutoAuto {
			box.SetEdge(BoxAreaMargin, edge, 0)
			return
		}
		box.SetEdge(BoxAreaMargin, edge, ResolveValueAuto(length, base))
	}
	resolveMargin(BoxEdgeTop, computed.MarginTop(), containingBlock.Y)
	resolveMargin(BoxEdgeRight, computed.MarginRight(), containingBlock.X)
	resolveMargin(BoxEdgeBottom, computed.MarginBottom(), containingBlock.Y)
	resolveMargin(BoxEdgeLeft, computed.MarginLeft(), containingBlock.X)

	if mode == BuildBoxModeInline {
		box.SetContent((Vector2f{0, 0}))
		return box
	}

	borderPadX := box.GetEdge(BoxAreaBorder, BoxEdgeLeft) + box.GetEdge(BoxAreaBorder, BoxEdgeRight) +
		box.GetEdge(BoxAreaPadding, BoxEdgeLeft) + box.GetEdge(BoxAreaPadding, BoxEdgeRight)
	borderPadY := box.GetEdge(BoxAreaBorder, BoxEdgeTop) + box.GetEdge(BoxAreaBorder, BoxEdgeBottom) +
		box.GetEdge(BoxAreaPadding, BoxEdgeTop) + box.GetEdge(BoxAreaPadding, BoxEdgeBottom)

	contentW := float32(-1)
	if computed.Width().Type != LengthPercentageAutoAuto && containingBlock.X >= 0 {
		contentW = ResolveValueAuto(computed.Width(), containingBlock.X)
		if computed.BoxSizing() == BoxSizingBorderBox {
			contentW = contentW - borderPadX
		}
	}
	minW := float32(0)
	maxW := FltMax
	if containingBlock.X >= 0 {
		minW = ResolveValue(computed.MinWidth(), containingBlock.X)
		maxW = ResolveValueOr(computed.MaxWidth(), containingBlock.X, FltMax)
		if computed.BoxSizing() == BoxSizingBorderBox {
			minW = minW - borderPadX
			if maxW < FltMax {
				maxW = maxW - borderPadX
			}
		}
		if minW < 0 {
			minW = 0
		}
	}
	if contentW >= 0 {
		contentW = MathClamp(contentW, minW, maxW)
	} else if mode == BuildBoxModeBlock && containingBlock.X >= 0 {
		contentW = containingBlock.X - box.GetEdge(BoxAreaMargin, BoxEdgeLeft) - box.GetEdge(BoxAreaMargin, BoxEdgeRight) - borderPadX
		if contentW < minW {
			contentW = minW
		}
		if contentW > maxW {
			contentW = maxW
		}
		if contentW < 0 {
			contentW = 0
		}
	}

	contentH := float32(-1)
	if computed.Height().Type != LengthPercentageAutoAuto && containingBlock.Y >= 0 {
		contentH = ResolveValueAuto(computed.Height(), containingBlock.Y)
		if computed.BoxSizing() == BoxSizingBorderBox {
			contentH = contentH - borderPadY
		}
		minH := ResolveValue(computed.MinHeight(), containingBlock.Y)
		maxH := ResolveValueOr(computed.MaxHeight(), containingBlock.Y, FltMax)
		if computed.BoxSizing() == BoxSizingBorderBox {
			minH = minH - borderPadY
			if maxH < FltMax {
				maxH = maxH - borderPadY
			}
		}
		if minH < 0 {
			minH = 0
		}
		contentH = MathClamp(contentH, minH, maxH)
	}
	box.SetContent((Vector2f{contentW, contentH}))
	return box
}

func lineHeightOf(element *Element) float32 {
	computed := element.GetComputedValues()
	height := computed.LineHeight().Value
	if height <= 0 {
		height = computed.FontSize() * 1.2
	}
	if height <= 0 {
		height = 14.4
	}
	return height
}

func faceHandleOf(element *Element) (FontEngineInterface, FontFaceHandle) {
	computed := element.GetComputedValues()
	engine := GetFontEngineInterface()
	if engine == nil {
		return nil, 0
	}
	handle := computed.FontFaceHandle()
	if handle == 0 {
		handle = engine.GetFontFaceHandle(computed.FontFamily(), computed.FontStyle(), computed.FontWeight(), int(computed.FontSize()))
	}
	return engine, handle
}

func fontAscentOf(element *Element) float32 {
	engine, handle := faceHandleOf(element)
	if engine == nil {
		return element.GetComputedValues().FontSize() * 0.8
	}
	return engine.Ascent(handle)
}

func stringWidthOf(element *Element, text string) float32 {
	engine, handle := faceHandleOf(element)
	if engine == nil {
		return element.GetComputedValues().FontSize() * 0.5 * float32(len(text))
	}
	return engine.MeasureText(handle, text)
}

// LayoutDocument formats the document against the context dimensions.
func LayoutDocument(doc *ElementDocument) {
	if doc == nil || doc.context == nil {
		return
	}
	vp := doc.context.GetDimensions().ToFloat()
	layoutElement(doc.element, vp, nil, (Vector2f{}))
}

// layoutElement places element so its margin box starts at marginPos
// relative to offsetParent's border box. The returned vector is the
// margin-box size.
func layoutElement(element *Element, containing Vector2f, offsetParent *Element, marginPos Vector2f) Vector2f {
	if element == nil {
		return Vector2f{}
	}
	computed := element.GetComputedValues()
	display := computed.Display()
	if display == DisplayNone {
		element.SetBox(NewBox((Vector2f{})))
		element.SetOffset(marginPos, offsetParent, false)
		return Vector2f{}
	}

	mode := BuildBoxModeInline
	if displayIsBlock(display) {
		mode = BuildBoxModeBlock
	} else if displayShrinks(display) {
		mode = BuildBoxModeUnalignedBlock
	}
	box := LayoutDetailsBuildBox(containing, element, mode)

	text := ElementTextOf(element)
	if text != nil {
		width := stringWidthOf(element, text.GetText())
		height := lineHeightOf(element)
		box.SetContent((Vector2f{width, height}))
	}

	element.SetBox(box)
	borderPos := Vector2f{
		marginPos.X + box.GetEdge(BoxAreaMargin, BoxEdgeLeft),
		marginPos.Y + box.GetEdge(BoxAreaMargin, BoxEdgeTop),
	}
	element.SetOffset(borderPos, offsetParent, computed.Position() == PositionFixed)

	if text == nil {
		inner := Vector2f{box.GetContentSize().X, box.GetContentSize().Y}
		if inner.X < 0 {
			inner.X = containing.X
			if inner.X < 0 {
				inner.X = 0
			}
		}
		contentOrigin := box.GetPosition(BoxAreaContent)
		y := float32(0)
		x := float32(0)
		lineH := float32(0)
		usedW := float32(0)
		n := element.GetNumChildren(false)
		for i := 0; i < n; i++ {
			child := element.GetChild(i)
			childDisplay := child.GetComputedValues().Display()
			if childDisplay == DisplayNone {
				continue
			}
			if displayIsBlock(childDisplay) {
				if x > 0 {
					y = y + lineH
					if x > usedW {
						usedW = x
					}
					x = 0
					lineH = 0
				}
				childContaining := Vector2f{inner.X, inner.Y}
				size := layoutElement(child, childContaining, element, contentOrigin.Add((Vector2f{0, y})))
				y = y + size.Y
			} else {
				childContaining := Vector2f{inner.X, -1}
				size := layoutElement(child, childContaining, element, contentOrigin.Add((Vector2f{x, y})))
				if inner.X > 0 && x > 0 && x+size.X > inner.X {
					y = y + lineH
					if x > usedW {
						usedW = x
					}
					x = 0
					lineH = 0
					size = layoutElement(child, childContaining, element, contentOrigin.Add((Vector2f{0, y})))
				}
				if size.Y > lineH {
					lineH = size.Y
				}
				x = x + size.X
			}
		}
		if x > 0 {
			y = y + lineH
			if x > usedW {
				usedW = x
			}
		}
		content := box.GetContentSize()
		changed := false
		if content.Y < 0 {
			content.Y = y
			changed = true
		}
		if content.X < 0 {
			content.X = usedW
			changed = true
		}
		if changed {
			box.SetContent(content)
			element.SetBox(box)
		}
	}

	element.SetScrollableOverflowRectangle(element.GetBox().GetContentSize(), true)
	if text != nil {
		element.SetBaseline(fontAscentOf(element))
	} else {
		element.SetBaseline(lineHeightOf(element))
	}
	return element.GetBox().GetSize(BoxAreaMargin)
}
