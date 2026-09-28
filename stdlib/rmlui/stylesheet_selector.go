// Port of RmlUi Source/Core/StyleSheetSelector.cpp and StyleSheetSelector.h.
package rmlui

// Selector specificity constants (SelectorSpecificity).
const (
	SpecificityTag         = 10000
	SpecificityClass       = 100000
	SpecificityAttribute   = SpecificityClass
	SpecificityPseudoClass = SpecificityClass
	SpecificityID          = 1000000
)

// SelectorCombinator is Rml::SelectorCombinator.
type SelectorCombinator = int

const (
	CombinatorDescendant SelectorCombinator = iota
	CombinatorChild
	CombinatorNextSibling
	CombinatorSubsequentSibling
)

// AttributeSelectorType is Rml::AttributeSelectorType; values are the
// operator characters.
type AttributeSelectorType = int

const (
	AttributeAlways               AttributeSelectorType = 0
	AttributeEqual                AttributeSelectorType = '='
	AttributeInList               AttributeSelectorType = '~'
	AttributeBeginsWithThenHyphen AttributeSelectorType = '|'
	AttributeBeginsWith           AttributeSelectorType = '^'
	AttributeEndsWith             AttributeSelectorType = '$'
	AttributeContains             AttributeSelectorType = '*'
)

// AttributeSelector is Rml::AttributeSelector.
type AttributeSelector struct {
	Type  AttributeSelectorType
	Name  string
	Value string
}

func (a AttributeSelector) Equals(b AttributeSelector) bool {
	return a.Type == b.Type && a.Name == b.Name && a.Value == b.Value
}

func (a AttributeSelector) Less(b AttributeSelector) bool {
	if a.Type != b.Type {
		return a.Type < b.Type
	}
	if a.Name != b.Name {
		return a.Name < b.Name
	}
	return a.Value < b.Value
}

// SelectorTree is Rml::SelectorTree, the inner selectors of :not().
type SelectorTree struct {
	Root  *StyleSheetNode
	Leafs []*StyleSheetNode
	id    int
}

var selectorTreeCounter = 0

// StructuralSelectorType is Rml::StructuralSelectorType.
type StructuralSelectorType = int

const (
	StructuralInvalid StructuralSelectorType = iota
	StructuralNthChild
	StructuralNthLastChild
	StructuralNthOfType
	StructuralNthLastOfType
	StructuralFirstChild
	StructuralLastChild
	StructuralFirstOfType
	StructuralLastOfType
	StructuralOnlyChild
	StructuralOnlyOfType
	StructuralEmpty
	StructuralNot
	StructuralScope
)

// StructuralSelector is Rml::StructuralSelector.
type StructuralSelector struct {
	Type         StructuralSelectorType
	A            int
	B            int
	Specificity  int
	SelectorTree *SelectorTree
}

func NewStructuralSelector(t StructuralSelectorType, a int, b int) StructuralSelector {
	return StructuralSelector{Type: t, A: a, B: b, Specificity: SpecificityPseudoClass}
}

func selectorTreeId(t *SelectorTree) int {
	if t == nil {
		return 0
	}
	return t.id
}

// Equals compares sub-selector trees by identity, as the C++ does.
func (a StructuralSelector) Equals(b StructuralSelector) bool {
	return a.Type == b.Type && a.A == b.A && a.B == b.B && selectorTreeId(a.SelectorTree) == selectorTreeId(b.SelectorTree)
}

func (a StructuralSelector) Less(b StructuralSelector) bool {
	if a.Type != b.Type {
		return a.Type < b.Type
	}
	if a.A != b.A {
		return a.A < b.A
	}
	if a.B != b.B {
		return a.B < b.B
	}
	return selectorTreeId(a.SelectorTree) < selectorTreeId(b.SelectorTree)
}

// CompoundSelector is Rml::CompoundSelector: all basic selectors for one
// node, e.g. div#foo.bar:nth-child(2).
type CompoundSelector struct {
	Tag                 string
	Id                  string
	ClassNames          []string
	PseudoClassNames    []string
	Attributes          []AttributeSelector
	StructuralSelectors []StructuralSelector
	Combinator          SelectorCombinator
}

func stringListsEqual(a []string, b []string) bool {
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

func (a *CompoundSelector) Equals(b *CompoundSelector) bool {
	if a.Tag != b.Tag || a.Id != b.Id || a.Combinator != b.Combinator {
		return false
	}
	if !stringListsEqual(a.ClassNames, b.ClassNames) || !stringListsEqual(a.PseudoClassNames, b.PseudoClassNames) {
		return false
	}
	if len(a.Attributes) != len(b.Attributes) || len(a.StructuralSelectors) != len(b.StructuralSelectors) {
		return false
	}
	for i := 0; i < len(a.Attributes); i++ {
		if !a.Attributes[i].Equals(b.Attributes[i]) {
			return false
		}
	}
	for i := 0; i < len(a.StructuralSelectors); i++ {
		if !a.StructuralSelectors[i].Equals(b.StructuralSelectors[i]) {
			return false
		}
	}
	return true
}

// Clone deep-copies the selector's lists.
func (a *CompoundSelector) Clone() CompoundSelector {
	c := CompoundSelector{Tag: a.Tag, Id: a.Id, Combinator: a.Combinator}
	c.ClassNames = append([]string{}, a.ClassNames...)
	c.PseudoClassNames = append([]string{}, a.PseudoClassNames...)
	c.Attributes = append([]AttributeSelector{}, a.Attributes...)
	c.StructuralSelectors = append([]StructuralSelector{}, a.StructuralSelectors...)
	return c
}

func sortStrings(list []string) {
	for i := 1; i < len(list); i++ {
		j := i
		for j > 0 && list[j] < list[j-1] {
			list[j], list[j-1] = list[j-1], list[j]
			j--
		}
	}
}

func sortAttributeSelectors(list []AttributeSelector) {
	for i := 1; i < len(list); i++ {
		j := i
		for j > 0 && list[j].Less(list[j-1]) {
			list[j], list[j-1] = list[j-1], list[j]
			j--
		}
	}
}

func sortStructuralSelectors(list []StructuralSelector) {
	for i := 1; i < len(list); i++ {
		j := i
		for j > 0 && list[j].Less(list[j-1]) {
			list[j], list[j-1] = list[j-1], list[j]
			j--
		}
	}
}

func isTextElement(element *Element) bool { return element.GetTagName() == "#text" }

// isNth reports whether a positive integer n solves a*n + b = count.
func isNth(a int, b int, count int) bool {
	x := count - b
	if a != 0 {
		x = x / a
	}
	return x >= 0 && x*a+b == count
}

// IsSelectorApplicable is Rml::IsSelectorApplicable.
func IsSelectorApplicable(element *Element, selector StructuralSelector, scope *Element) bool {
	switch selector.Type {
	case StructuralNthChild:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		index := 1
		for i := 0; i < parent.GetNumChildren(false); i++ {
			child := parent.GetChild(i)
			if isTextElement(child) {
				continue
			}
			if child == element {
				break
			}
			index++
		}
		return isNth(selector.A, selector.B, index)
	case StructuralNthLastChild:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		index := 1
		for i := parent.GetNumChildren(false) - 1; i >= 0; i-- {
			child := parent.GetChild(i)
			if isTextElement(child) {
				continue
			}
			if child == element {
				break
			}
			index++
		}
		return isNth(selector.A, selector.B, index)
	case StructuralNthOfType:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		index := 1
		n := parent.GetNumChildren(false)
		for i := 0; i < n; i++ {
			child := parent.GetChild(i)
			if child == element {
				break
			}
			if child.GetTagName() != element.GetTagName() {
				continue
			}
			index++
		}
		return isNth(selector.A, selector.B, index)
	case StructuralNthLastOfType:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		index := 1
		for i := parent.GetNumChildren(false) - 1; i >= 0; i-- {
			child := parent.GetChild(i)
			if child == element {
				break
			}
			if child.GetTagName() != element.GetTagName() {
				continue
			}
			index++
		}
		return isNth(selector.A, selector.B, index)
	case StructuralFirstChild:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		for i := 0; i < parent.GetNumChildren(false); i++ {
			child := parent.GetChild(i)
			if child == element {
				return true
			}
			if !isTextElement(child) {
				return false
			}
		}
		return false
	case StructuralLastChild:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		for i := parent.GetNumChildren(false) - 1; i >= 0; i-- {
			child := parent.GetChild(i)
			if child == element {
				return true
			}
			if !isTextElement(child) {
				return false
			}
		}
		return false
	case StructuralFirstOfType:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		for i := 0; i < parent.GetNumChildren(false); i++ {
			child := parent.GetChild(i)
			if child == element {
				return true
			}
			if child.GetTagName() == element.GetTagName() {
				return false
			}
		}
		return false
	case StructuralLastOfType:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		for i := parent.GetNumChildren(false) - 1; i >= 0; i-- {
			child := parent.GetChild(i)
			if child == element {
				return true
			}
			if child.GetTagName() == element.GetTagName() {
				return false
			}
		}
		return false
	case StructuralOnlyChild:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		n := parent.GetNumChildren(false)
		for i := 0; i < n; i++ {
			child := parent.GetChild(i)
			if child == element || isTextElement(child) {
				continue
			}
			return false
		}
		return true
	case StructuralOnlyOfType:
		parent := element.GetParentNode()
		if parent == nil {
			return false
		}
		n := parent.GetNumChildren(false)
		for i := 0; i < n; i++ {
			child := parent.GetChild(i)
			if child == element {
				continue
			}
			if child.GetTagName() != element.GetTagName() {
				continue
			}
			return false
		}
		return true
	case StructuralEmpty:
		return element.GetNumChildren(false) == 0
	case StructuralNot:
		if selector.SelectorTree == nil {
			return false
		}
		leafs := selector.SelectorTree.Leafs
		for _, node := range leafs {
			if node.IsApplicable(element, scope) {
				return false
			}
		}
		return true
	case StructuralScope:
		return scope != nil && element == scope
	}
	return false
}
