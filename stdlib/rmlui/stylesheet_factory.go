// Port of RmlUi Source/Core/StyleSheetFactory.cpp and StyleSheetFactory.h.
package rmlui

type styleSheetFactory struct {
	stylesheets map[string]*StyleSheetContainer
	selectors   map[string]StructuralSelectorType
}

var styleSheetFactoryInstance *styleSheetFactory

// StyleSheetFactoryInitialise is StyleSheetFactory::Initialise.
func StyleSheetFactoryInitialise() bool {
	styleSheetFactoryInstance = &styleSheetFactory{
		stylesheets: map[string]*StyleSheetContainer{},
		selectors: map[string]StructuralSelectorType{
			"nth-child":        StructuralNthChild,
			"nth-last-child":   StructuralNthLastChild,
			"nth-of-type":      StructuralNthOfType,
			"nth-last-of-type": StructuralNthLastOfType,
			"first-child":      StructuralFirstChild,
			"last-child":       StructuralLastChild,
			"first-of-type":    StructuralFirstOfType,
			"last-of-type":     StructuralLastOfType,
			"only-child":       StructuralOnlyChild,
			"only-of-type":     StructuralOnlyOfType,
			"empty":            StructuralEmpty,
			"not":              StructuralNot,
			"scope":            StructuralScope,
		},
	}
	return true
}

func StyleSheetFactoryShutdown() { styleSheetFactoryInstance = nil }

// GetStyleSheetContainer loads (and caches) the style sheet at sheetName.
func GetStyleSheetContainer(sheetName string) *StyleSheetContainer {
	f := styleSheetFactoryInstance
	if c, ok := f.stylesheets[sheetName]; ok {
		return c
	}
	stream, ok := OpenStreamFile(sheetName)
	if !ok {
		return nil
	}
	c := NewStyleSheetContainer()
	if !c.LoadStyleSheetContainer(stream, 0) {
		return nil
	}
	f.stylesheets[sheetName] = c
	return c
}

// ClearStyleSheetCache is StyleSheetFactory::ClearStyleSheetCache.
func ClearStyleSheetCache() { styleSheetFactoryInstance.stylesheets = map[string]*StyleSheetContainer{} }

// GetStructuralSelector is StyleSheetFactory::GetSelector: parses a
// structural pseudo-class such as nth-child(2n+1) or not(.x).
func GetStructuralSelector(name string) StructuralSelector {
	f := styleSheetFactoryInstance
	parameterStart := indexByte(name, '(')
	key := name
	if parameterStart >= 0 {
		key = name[:parameterStart]
	}
	selectorType, ok := f.selectors[key]
	if !ok {
		return NewStructuralSelector(StructuralInvalid, 0, 0)
	}
	requiresParameter := false
	switch selectorType {
	case StructuralNthChild, StructuralNthLastChild, StructuralNthOfType, StructuralNthLastOfType, StructuralNot:
		requiresParameter = true
	}
	parameterEnd := lastIndexByte(name, ')')
	hasParameter := parameterStart >= 0 && parameterEnd >= 0 && parameterStart < parameterEnd
	if requiresParameter != hasParameter {
		expected := "no"
		if requiresParameter {
			expected = "parenthesized"
		}
		LogMessage(LogWarning, "Invalid selector ':"+name+"' encountered, expected "+expected+" parameters")
		return NewStructuralSelector(StructuralInvalid, 0, 0)
	}
	a := 1
	b := 0
	if hasParameter {
		parameters := StringStripWhitespace(name[parameterStart+1 : parameterEnd])
		if selectorType == StructuralNot {
			selectorTreeCounter++
			tree := &SelectorTree{Root: NewStyleSheetRootNode(), id: selectorTreeCounter}
			tree.Leafs = ConstructSelectorNodes(tree.Root, parameters)
			specificity := 0
			leafs := tree.Leafs
			for _, node := range leafs {
				specificity = MathMaxInt(specificity, node.GetSpecificity())
			}
			return StructuralSelector{Type: selectorType, Specificity: specificity, SelectorTree: tree}
		}
		if parameters == "even" {
			a = 2
			b = 0
		} else if parameters == "odd" {
			a = 2
			b = 1
		} else {
			nIndex := indexByte(parameters, 'n')
			if nIndex < 0 {
				a = 0
				b = Atoi(parameters)
			} else {
				if nIndex == 0 {
					a = 1
				} else {
					aParameter := parameters[:nIndex]
					if StringStripWhitespace(aParameter) == "-" {
						a = -1
					} else {
						a = Atoi(aParameter)
					}
				}
				pmIndex := stringFindFrom(parameters, "+", nIndex+1)
				if pmIndex >= 0 {
					b = 1
				} else {
					pmIndex = stringFindFrom(parameters, "-", nIndex+1)
					if pmIndex >= 0 {
						b = -1
					}
				}
				if nIndex == len(parameters)-1 || pmIndex < 0 {
					b = 0
				} else {
					b = b * Atoi(parameters[pmIndex+1:])
				}
			}
		}
	}
	return NewStructuralSelector(selectorType, a, b)
}
