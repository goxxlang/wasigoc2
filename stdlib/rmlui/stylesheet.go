// Port of RmlUi Source/Core/StyleSheet.cpp, ElementDefinition.cpp, and
// Include/RmlUi/Core/StyleSheet.h, StyleSheetTypes.h, ElementDefinition.h.
package rmlui

// KeyframeBlock is Rml::KeyframeBlock.
type KeyframeBlock struct {
	NormalizedTime float32 // [0, 1]
	Properties     *PropertyDictionary
}

// Keyframes is Rml::Keyframes.
type Keyframes struct {
	PropertyIds []PropertyId
	Blocks      []*KeyframeBlock
}

func (k *Keyframes) clone() *Keyframes {
	c := &Keyframes{PropertyIds: append([]PropertyId{}, k.PropertyIds...)}
	blocks := k.Blocks
	for _, b := range blocks {
		c.Blocks = append(c.Blocks, &KeyframeBlock{NormalizedTime: b.NormalizedTime, Properties: b.Properties.Clone()})
	}
	return c
}

// NamedDecorator is Rml::NamedDecorator (from an @decorator rule).
type NamedDecorator struct {
	Type       string
	Instancer  DecoratorInstancer
	Properties *PropertyDictionary
}

// MediaQueryModifier is Rml::MediaQueryModifier.
type MediaQueryModifier = int

const (
	MediaQueryModifierNone MediaQueryModifier = iota
	MediaQueryModifierNot
)

// MediaBlock is Rml::MediaBlock.
type MediaBlock struct {
	Properties *PropertyDictionary // media query properties
	Stylesheet *StyleSheet
	Modifier   MediaQueryModifier
}

// StyleSheetIndex is Rml::StyleSheetIndex: styled nodes indexed by their
// most specific requirement, in priority order ids, classes, tags, other.
type StyleSheetIndex struct {
	ids     map[string][]*StyleSheetNode
	classes map[string][]*StyleSheetNode
	tags    map[string][]*StyleSheetNode
	other   []*StyleSheetNode
}

func newStyleSheetIndex() *StyleSheetIndex {
	return &StyleSheetIndex{ids: map[string][]*StyleSheetNode{}, classes: map[string][]*StyleSheetNode{}, tags: map[string][]*StyleSheetNode{}}
}

func (x *StyleSheetIndex) insert(index map[string][]*StyleSheetNode, key string, node *StyleSheetNode) {
	nodes := index[key]
	for _, n := range nodes {
		if n == node {
			return
		}
	}
	index[key] = append(nodes, node)
}

// StyleSheet is Rml::StyleSheet.
type StyleSheet struct {
	root              *StyleSheetNode
	specificityOffset int
	keyframes         map[string]*Keyframes
	namedDecoratorMap map[string]*NamedDecorator
	spritesheetList   *SpritesheetList
	styledNodeIndex   *StyleSheetIndex
	nodeCache         map[string]*ElementDefinition
	decoratorCache    map[string][]Decorator
}

func NewStyleSheet() *StyleSheet {
	return &StyleSheet{
		root:              NewStyleSheetRootNode(),
		keyframes:         map[string]*Keyframes{},
		namedDecoratorMap: map[string]*NamedDecorator{},
		spritesheetList:   NewSpritesheetList(),
		styledNodeIndex:   newStyleSheetIndex(),
		nodeCache:         map[string]*ElementDefinition{},
		decoratorCache:    map[string][]Decorator{},
	}
}

// CombineStyleSheet returns a new sheet with other merged into a copy of s.
func (s *StyleSheet) CombineStyleSheet(other *StyleSheet) *StyleSheet {
	n := NewStyleSheet()
	n.root = s.root.DeepCopy(nil)
	n.specificityOffset = s.specificityOffset
	keyframes := s.keyframes
	for name, k := range keyframes {
		n.keyframes[name] = k.clone()
	}
	decorators := s.namedDecoratorMap
	for name, d := range decorators {
		n.namedDecoratorMap[name] = d
	}
	n.spritesheetList = s.spritesheetList.Clone()
	n.MergeStyleSheet(other)
	n.BuildNodeIndex()
	return n
}

func (s *StyleSheet) MergeStyleSheet(other *StyleSheet) {
	s.root.MergeHierarchy(other.root, s.specificityOffset)
	s.specificityOffset += other.specificityOffset
	keyframes := other.keyframes
	for name, k := range keyframes {
		s.keyframes[name] = k
	}
	decorators := other.namedDecoratorMap
	for name, d := range decorators {
		s.namedDecoratorMap[name] = d
	}
	s.spritesheetList.Merge(other.spritesheetList)
}

func (s *StyleSheet) BuildNodeIndex() {
	s.styledNodeIndex = newStyleSheetIndex()
	s.root.BuildIndex(s.styledNodeIndex)
}

func (s *StyleSheet) GetNamedDecorator(name string) *NamedDecorator {
	if d, ok := s.namedDecoratorMap[name]; ok {
		return d
	}
	return nil
}

func (s *StyleSheet) GetKeyframes(name string) *Keyframes {
	if k, ok := s.keyframes[name]; ok {
		return k
	}
	return nil
}

func (s *StyleSheet) GetSprite(name string) *Sprite { return s.spritesheetList.GetSprite(name) }

// InstanceDecorators is StyleSheet::InstanceDecorators, cached by the
// declaration value and source path.
func (s *StyleSheet) InstanceDecorators(renderManager *RenderManager, declarationList *DecoratorDeclarationList, source *PropertySource) []Decorator {
	enableCache := declarationList.Value != ""
	key := ""
	if enableCache {
		key = declarationList.Value + ";"
		if source != nil {
			key = key + source.Path
		}
		if cached, ok := s.decoratorCache[key]; ok {
			return cached
		}
	}
	decorators := []Decorator{}
	sourcePath := ""
	sourceLine := -1
	if source != nil {
		sourcePath = source.Path
		sourceLine = source.LineNumber
	}
	iface := &DecoratorInstancerInterface{renderManager: renderManager, styleSheet: s, propertySource: source}
	list := declarationList.List
	for _, declaration := range list {
		var decorator Decorator
		if declaration.Instancer != nil {
			decorator = declaration.Instancer.InstanceDecorator(declaration.Type, declaration.Properties, iface)
			if decorator == nil {
				LogMessage(LogWarning, "Decorator '"+declaration.Type+"' in '"+declarationList.Value+"' could not be instanced, declared at "+sourcePath+":"+FormatInt(sourceLine))
			}
		} else {
			if named, ok := s.namedDecoratorMap[declaration.Type]; ok {
				decorator = named.Instancer.InstanceDecorator(named.Type, named.Properties, iface)
			}
			if decorator == nil {
				LogMessage(LogWarning, "Decorator name '"+declaration.Type+"' could not be found in any @decorator rule, declared at "+sourcePath+":"+FormatInt(sourceLine))
			}
		}
		if decorator == nil {
			decorators = []Decorator{}
			break
		}
		decorators = append(decorators, decorator)
	}
	if enableCache {
		s.decoratorCache[key] = decorators
	}
	return decorators
}

// GetElementDefinition returns the merged definition of every style node
// applying to element, or nil when none apply.
func (s *StyleSheet) GetElementDefinition(element *Element) *ElementDefinition {
	tag := element.GetTagName()
	if tag == "#text" {
		return nil
	}
	applicable := []*StyleSheetNode{}
	addApplicable := func(index map[string][]*StyleSheetNode, key string) {
		nodes, ok := index[key]
		if !ok {
			return
		}
		for _, node := range nodes {
			if node.IsApplicable(element, nil) {
				applicable = append(applicable, node)
			}
		}
	}
	id := element.GetId()
	if id != "" {
		addApplicable(s.styledNodeIndex.ids, id)
	}
	classNames := element.GetStyle().GetClassNameList()
	for _, name := range classNames {
		addApplicable(s.styledNodeIndex.classes, name)
	}
	addApplicable(s.styledNodeIndex.tags, tag)
	others := s.styledNodeIndex.other
	for _, node := range others {
		if node.IsApplicable(element, nil) {
			applicable = append(applicable, node)
		}
	}
	if len(applicable) == 0 {
		return nil
	}
	// Sort by specificity, then creation order where the C++ uses pointer order.
	for i := 1; i < len(applicable); i++ {
		j := i
		for j > 0 && nodeLess(applicable[j], applicable[j-1]) {
			applicable[j], applicable[j-1] = applicable[j-1], applicable[j]
			j--
		}
	}
	key := ""
	for _, node := range applicable {
		key = key + FormatInt(node.serial) + ","
	}
	if def, ok := s.nodeCache[key]; ok {
		return def
	}
	def := NewElementDefinition(applicable)
	s.nodeCache[key] = def
	return def
}

func nodeLess(a *StyleSheetNode, b *StyleSheetNode) bool {
	if a.specificity == b.specificity {
		return a.serial < b.serial
	}
	return a.specificity < b.specificity
}

// ElementDefinition is Rml::ElementDefinition: the properties an element
// gets from its style sheet, merged in specificity order.
type ElementDefinition struct {
	properties  *PropertyDictionary
	propertyIds PropertyIdSet
}

func NewElementDefinition(nodes []*StyleSheetNode) *ElementDefinition {
	d := &ElementDefinition{properties: NewPropertyDictionary()}
	for _, node := range nodes {
		d.properties.Merge(node.GetProperties(), 0)
	}
	props := d.properties.GetProperties()
	for id := range props {
		d.propertyIds.Insert(id)
	}
	return d
}

func (d *ElementDefinition) GetProperty(id PropertyId) *Property { return d.properties.GetProperty(id) }
func (d *ElementDefinition) GetCustomProperty(name string) *Property {
	return d.properties.GetCustomProperty(name)
}
func (d *ElementDefinition) GetPropertyIds() PropertyIdSet { return d.propertyIds }
func (d *ElementDefinition) GetProperties() *PropertyDictionary { return d.properties }
