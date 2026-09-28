// Port of RmlUi Source/Core/StyleSheetNode.cpp and StyleSheetNode.h.
package rmlui

// StyleSheetNode is Rml::StyleSheetNode: one compound selector in the
// style sheet tree, with the properties of every rule ending at it.
type StyleSheetNode struct {
	parent      *StyleSheetNode
	selector    CompoundSelector
	specificity int
	properties  *PropertyDictionary
	children    []*StyleSheetNode
	// serial breaks specificity ties deterministically where the C++ sorts
	// by pointer value.
	serial int
}

var styleSheetNodeSerial = 0

func NewStyleSheetNode(parent *StyleSheetNode, selector CompoundSelector) *StyleSheetNode {
	styleSheetNodeSerial++
	n := &StyleSheetNode{parent: parent, selector: selector, properties: NewPropertyDictionary(), serial: styleSheetNodeSerial}
	n.calculateAndSetSpecificity()
	return n
}

// NewStyleSheetRootNode is the default constructor (root node).
func NewStyleSheetRootNode() *StyleSheetNode {
	return NewStyleSheetNode(nil, CompoundSelector{})
}

func (n *StyleSheetNode) GetOrCreateChildNode(other CompoundSelector) *StyleSheetNode {
	children := n.children
	for _, child := range children {
		if child.selector.Equals(&other) {
			return child
		}
	}
	child := NewStyleSheetNode(n, other)
	n.children = append(n.children, child)
	return child
}

func (n *StyleSheetNode) MergeHierarchy(node *StyleSheetNode, specificityOffset int) {
	n.properties.Merge(node.properties, specificityOffset)
	children := node.children
	for _, other := range children {
		local := n.GetOrCreateChildNode(other.selector.Clone())
		local.MergeHierarchy(other, specificityOffset)
	}
}

func (n *StyleSheetNode) DeepCopy(parent *StyleSheetNode) *StyleSheetNode {
	node := NewStyleSheetNode(parent, n.selector.Clone())
	node.properties = n.properties.Clone()
	children := n.children
	for _, child := range children {
		node.children = append(node.children, child.DeepCopy(node))
	}
	return node
}

func (n *StyleSheetNode) BuildIndex(index *StyleSheetIndex) {
	if !n.properties.Empty() {
		if n.selector.Id != "" {
			index.insert(index.ids, n.selector.Id, n)
		} else if len(n.selector.ClassNames) > 0 {
			index.insert(index.classes, n.selector.ClassNames[0], n)
		} else if n.selector.Tag != "" {
			index.insert(index.tags, n.selector.Tag, n)
		} else {
			index.other = append(index.other, n)
		}
	}
	children := n.children
	for _, child := range children {
		child.BuildIndex(index)
	}
}

func (n *StyleSheetNode) GetSpecificity() int { return n.specificity }

func (n *StyleSheetNode) ImportProperties(properties *PropertyDictionary, ruleSpecificity int) {
	n.properties.Import(properties, n.specificity+ruleSpecificity)
}

func (n *StyleSheetNode) GetProperties() *PropertyDictionary { return n.properties }

func (n *StyleSheetNode) match(element *Element, scope *Element) bool {
	if n.selector.Tag != "" && n.selector.Tag != element.GetTagName() {
		return false
	}
	if n.selector.Id != "" && n.selector.Id != element.GetId() {
		return false
	}
	classes := n.selector.ClassNames
	for _, name := range classes {
		if !element.IsClassSet(name) {
			return false
		}
	}
	pseudos := n.selector.PseudoClassNames
	for _, name := range pseudos {
		if !element.IsPseudoClassSet(name) {
			return false
		}
	}
	if len(n.selector.Attributes) > 0 && !n.matchAttributes(element) {
		return false
	}
	if len(n.selector.StructuralSelectors) > 0 && !n.matchStructuralSelector(element, scope) {
		return false
	}
	return true
}

func (n *StyleSheetNode) matchStructuralSelector(element *Element, scope *Element) bool {
	selectors := n.selector.StructuralSelectors
	for _, s := range selectors {
		if !IsSelectorApplicable(element, s, scope) {
			return false
		}
	}
	return true
}

func (n *StyleSheetNode) matchAttributes(element *Element) bool {
	attributes := n.selector.Attributes
	for _, attribute := range attributes {
		variant := element.GetAttribute(attribute.Name)
		if variant == nil {
			return false
		}
		if attribute.Type == AttributeAlways {
			continue
		}
		elementValue := variant.GetString()
		cssValue := attribute.Value
		switch attribute.Type {
		case AttributeEqual:
			if elementValue != cssValue {
				return false
			}
		case AttributeInList:
			found := false
			index := stringFindFrom(elementValue, cssValue, 0)
			for index >= 0 {
				right := index + len(cssValue)
				wsLeft := index == 0 || elementValue[index-1] == ' '
				wsRight := right == len(elementValue) || elementValue[right] == ' '
				if wsLeft && wsRight {
					found = true
					break
				}
				index = stringFindFrom(elementValue, cssValue, index+1)
			}
			if !found {
				return false
			}
		case AttributeBeginsWithThenHyphen:
			if !StringStartsWith(elementValue, cssValue) || (len(elementValue) != len(cssValue) && elementValue[len(cssValue)] != '-') {
				return false
			}
		case AttributeBeginsWith:
			if !StringStartsWith(elementValue, cssValue) {
				return false
			}
		case AttributeEndsWith:
			if !StringEndsWith(elementValue, cssValue) {
				return false
			}
		case AttributeContains:
			if stringFindFrom(elementValue, cssValue, 0) < 0 {
				return false
			}
		}
	}
	return true
}

// stringFindFrom is std::string::find(needle, from); -1 for npos.
func stringFindFrom(haystack string, needle string, from int) int {
	if from > len(haystack) {
		return -1
	}
	for i := from; i+len(needle) <= len(haystack); i++ {
		if haystack[i:i+len(needle)] == needle {
			return i
		}
	}
	return -1
}

func (n *StyleSheetNode) traverseMatch(element *Element, scope *Element) bool {
	if n.parent.parent == nil {
		return true
	}
	switch n.selector.Combinator {
	case CombinatorDescendant, CombinatorChild:
		ancestor := element.GetParentNode()
		for ancestor != nil {
			if n.parent.match(ancestor, scope) && n.parent.traverseMatch(ancestor, scope) {
				return true
			} else if n.selector.Combinator == CombinatorChild {
				return false
			}
			ancestor = ancestor.GetParentNode()
		}
	case CombinatorNextSibling, CombinatorSubsequentSibling:
		parentElement := element.GetParentNode()
		if parentElement == nil {
			return false
		}
		preceding := -1
		numChildren := parentElement.GetNumChildren(true)
		for i := 0; i < numChildren; i++ {
			if parentElement.GetChild(i) == element {
				preceding = i - 1
				break
			}
		}
		for i := preceding; i >= 0; i-- {
			sibling := parentElement.GetChild(i)
			if isTextElement(sibling) {
				continue
			} else if n.parent.match(sibling, scope) && n.parent.traverseMatch(sibling, scope) {
				return true
			} else if n.selector.Combinator == CombinatorNextSibling {
				return false
			}
		}
	}
	return false
}

// IsApplicable reports whether element (and its hierarchy) matches this
// node's full selector.
func (n *StyleSheetNode) IsApplicable(element *Element, scope *Element) bool {
	pseudos := n.selector.PseudoClassNames
	for _, name := range pseudos {
		if !element.IsPseudoClassSet(name) {
			return false
		}
	}
	if n.selector.Tag != "" && n.selector.Tag != element.GetTagName() {
		return false
	}
	classes := n.selector.ClassNames
	for _, name := range classes {
		if !element.IsClassSet(name) {
			return false
		}
	}
	if n.selector.Id != "" && n.selector.Id != element.GetId() {
		return false
	}
	if len(n.selector.Attributes) > 0 && !n.matchAttributes(element) {
		return false
	}
	if len(n.selector.StructuralSelectors) > 0 && !n.matchStructuralSelector(element, scope) {
		return false
	}
	if n.parent != nil && !n.traverseMatch(element, scope) {
		return false
	}
	return true
}

func (n *StyleSheetNode) calculateAndSetSpecificity() {
	n.specificity = 0
	if n.selector.Tag != "" {
		n.specificity += SpecificityTag
	}
	if n.selector.Id != "" {
		n.specificity += SpecificityID
	}
	n.specificity += SpecificityClass * len(n.selector.ClassNames)
	n.specificity += SpecificityAttribute * len(n.selector.Attributes)
	n.specificity += SpecificityPseudoClass * len(n.selector.PseudoClassNames)
	structural := n.selector.StructuralSelectors
	for _, s := range structural {
		n.specificity += s.Specificity
	}
	if n.parent != nil {
		n.specificity += n.parent.specificity
	}
}
