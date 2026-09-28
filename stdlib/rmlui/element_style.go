// Port of RmlUi Source/Core/ElementStyle.cpp and ElementStyle.h.
package rmlui

// Pseudo-class state bits (PseudoClassState).
const (
	pseudoClassClear    = 0
	pseudoClassSet      = 1
	pseudoClassOverride = 2
)

type propertySources struct {
	element          *Element
	definition       *ElementDefinition
	inlineProperties *PropertyDictionary
}

type propertyElementPair struct {
	property *Property
	element  *Element
}

// stringSet is SmallUnorderedSet<String>.
type stringSet struct {
	m map[string]bool
}

func newStringSet() *stringSet { return &stringSet{m: map[string]bool{}} }

func (s *stringSet) insert(v string) bool {
	if s.m[v] {
		return false
	}
	s.m[v] = true
	return true
}

func (s *stringSet) erase(v string)         { delete(s.m, v) }
func (s *stringSet) has(v string) bool      { return s.m[v] }
func (s *stringSet) clear()                 { s.m = map[string]bool{} }
func (s *stringSet) empty() bool            { return len(s.m) == 0 }
func (s *stringSet) keys() []string {
	out := []string{}
	m := s.m
	for k := range m {
		out = append(out, k)
	}
	sortStrings(out)
	return out
}

// ElementStyle is Rml::ElementStyle: an element's classes, pseudo-classes,
// inline properties, and style sheet definition, and the cascade that
// turns them into computed values.
type ElementStyle struct {
	element            *Element
	classes            []string
	pseudoClasses      map[string]int
	definition         *ElementDefinition
	inlineProperties   *PropertyDictionary
	expandedShorthands *PropertyDictionary
	dirtyProperties    PropertyIdSet
	dirtyVariables     *stringSet
	dirtyVarShorthands map[ShorthandId]bool
}

func NewElementStyle(element *Element) *ElementStyle {
	return &ElementStyle{
		element:            element,
		pseudoClasses:      map[string]int{},
		inlineProperties:   NewPropertyDictionary(),
		expandedShorthands: NewPropertyDictionary(),
		dirtyVariables:     newStringSet(),
		dirtyVarShorthands: map[ShorthandId]bool{},
	}
}

var emptyPropertyDictionary = NewPropertyDictionary()

func styleLocalProperty(id PropertyId, inline *PropertyDictionary, definition *ElementDefinition) *Property {
	if p := inline.GetProperty(id); p != nil {
		return p
	}
	if definition != nil {
		return definition.GetProperty(id)
	}
	return nil
}

func styleLocalCustomProperty(name string, inline *PropertyDictionary, definition *ElementDefinition) *Property {
	if p := inline.GetCustomProperty(name); p != nil {
		return p
	}
	if definition != nil {
		return definition.GetCustomProperty(name)
	}
	return nil
}

func styleLocalShorthand(id ShorthandId, inline *PropertyDictionary, definition *ElementDefinition) *Property {
	if p := inline.GetVarShorthand(id); p != nil {
		return p
	}
	if definition != nil {
		return definition.GetProperties().GetVarShorthand(id)
	}
	return nil
}

func getSpecifiedProperty(sources propertySources, id PropertyId) propertyElementPair {
	if local := styleLocalProperty(id, sources.inlineProperties, sources.definition); local != nil {
		return propertyElementPair{local, sources.element}
	}
	def := GetPropertyDefinition(id)
	if def == nil {
		return propertyElementPair{}
	}
	if def.IsInherited() {
		parent := sources.element.GetParentNode()
		for parent != nil {
			if p := parent.GetStyle().GetLocalProperty(id); p != nil {
				return propertyElementPair{p, parent}
			}
			parent = parent.GetParentNode()
		}
	}
	return propertyElementPair{def.GetDefaultValue(), nil}
}

func getSpecifiedCustomProperty(sources propertySources, name string) propertyElementPair {
	if local := styleLocalCustomProperty(name, sources.inlineProperties, sources.definition); local != nil {
		return propertyElementPair{local, sources.element}
	}
	parent := sources.element.GetParentNode()
	for parent != nil {
		ps := parent.GetStyle()
		if p := styleLocalCustomProperty(name, ps.inlineProperties, ps.definition); p != nil {
			return propertyElementPair{p, parent}
		}
		parent = parent.GetParentNode()
	}
	return propertyElementPair{}
}

const (
	substitutionSubstituted = iota
	substitutionNoVariablesFound
	substitutionError
)

func findFirstOf(s string, chars string, from int) int {
	for i := from; i < len(s); i++ {
		if indexByte(chars, s[i]) >= 0 {
			return i
		}
	}
	return -1
}

func substituteVariableOnce(sources propertySources, value string, deps *stringSet, cycleChain *stringSet) (int, string) {
	begin := stringFindFrom(value, "var(", 0)
	endName := -1
	if begin >= 0 {
		endName = findFirstOf(value, ",)", begin)
	}
	if begin < 0 || endName < 0 {
		return substitutionNoVariablesFound, ""
	}
	hasFallback := value[endName] == ','
	endParenthesis := endName
	if hasFallback {
		depth := 1
		endParenthesis = endName + 1
		for endParenthesis < len(value) {
			c := value[endParenthesis]
			if c == '(' {
				depth++
			} else if c == ')' {
				depth--
			}
			if depth == 0 {
				break
			}
			endParenthesis++
		}
	}
	if endParenthesis >= len(value) {
		return substitutionNoVariablesFound, ""
	}
	varName := StringStripWhitespace(value[begin+4 : endName])
	if !cycleChain.insert(varName) {
		LogMessage(LogError, "Invalid substitution, cycle detected with variable: "+varName+". In element: "+sources.element.GetAddress(false, true))
		return substitutionError, ""
	}
	pair := getSpecifiedCustomProperty(sources, varName)
	valid := pair.property != nil && pair.element != nil
	substitution := ""
	if !valid && hasFallback {
		substitution = StringStripWhitespace(value[endName+1 : endParenthesis])
	} else if !valid {
		LogMessage(LogError, "Invalid substitution, variable '"+varName+"' not defined. In element: "+sources.element.GetAddress(false, true))
		return substitutionError, ""
	} else if pair.property.Unit == UnitVAR_EXPRESSION {
		// The cycle chain restarts when moving up to another element.
		nextChain := cycleChain
		nextSources := sources
		if pair.element != sources.element {
			nextChain = newStringSet()
			nextSources = pair.element.GetStyle().getPropertySources()
		}
		result, ok := substituteVariables(nextSources, pair.property.Value.GetString(), deps, nextChain)
		if !ok {
			return substitutionError, ""
		}
		substitution = result
	} else {
		substitution = pair.property.Value.GetString()
	}
	cycleChain.erase(varName)
	deps.insert(varName)
	return substitutionSubstituted, value[:begin] + substitution + value[endParenthesis+1:]
}

func substituteVariables(sources propertySources, value string, deps *stringSet, cycleChain *stringSet) (string, bool) {
	for i := 0; ; i++ {
		kind, result := substituteVariableOnce(sources, value, deps, cycleChain)
		switch kind {
		case substitutionSubstituted:
			value = result
		case substitutionNoVariablesFound:
			if i == 0 {
				LogMessage(LogError, "Invalid substitution, expected 'var()': "+value+". In element: "+sources.element.GetAddress(false, true))
				return "", false
			}
			return value, true
		default:
			return "", false
		}
	}
}

func resolveVariables(sources propertySources, substitutedShorthands *PropertyDictionary, id PropertyId, property *Property, deps *stringSet) *Property {
	if property.Unit == UnitSHORTHAND_PLACEHOLDER {
		fromShorthand := substitutedShorthands.GetProperty(id)
		if fromShorthand == nil {
			LogMessage(LogError, "Property '"+GetPropertyName(id)+"' could not be resolved: Pending substitution from a shorthand.")
			return nil
		}
		return fromShorthand
	}
	if property.Unit != UnitVAR_EXPRESSION {
		return property
	}
	result, ok := substituteVariables(sources, property.Value.GetString(), deps, newStringSet())
	if !ok {
		return nil
	}
	def := GetPropertyDefinition(id)
	if def == nil {
		LogMessage(LogError, "Invalid substitution, property '"+GetPropertyName(id)+"' not defined.")
		return nil
	}
	storage := new(Property)
	*storage = NewProperty()
	if !def.ParseValue(storage, result) {
		LogMessage(LogError, "Invalid substitution, property '"+GetPropertyName(id)+"' has invalid value: "+result)
		return nil
	}
	return storage
}

// resolveVariablesWithShorthandExpansion lazily expands shorthands into
// *cache (nil until needed).
func resolveVariablesWithShorthandExpansion(sources propertySources, id PropertyId, property *Property, cache **PropertyDictionary, deps *stringSet) *Property {
	if property == nil {
		return nil
	}
	if property.Unit != UnitSHORTHAND_PLACEHOLDER && property.Unit != UnitVAR_EXPRESSION {
		return property
	}
	if property.Unit == UnitSHORTHAND_PLACEHOLDER && *cache == nil {
		expanded := NewPropertyDictionary()
		expandVarShorthands(expanded, sources, nil)
		*cache = expanded
	}
	deps.clear()
	shorthands := *cache
	if shorthands == nil {
		shorthands = emptyPropertyDictionary
	}
	return resolveVariables(sources, shorthands, id, property, deps)
}

type dirtyPropertiesRef struct {
	properties    *PropertyIdSet
	variables     *stringSet
	varShorthands map[ShorthandId]bool
}

func expandVarShorthands(out *PropertyDictionary, sources propertySources, dirty *dirtyPropertiesRef) {
	out.Clear()
	deps := newStringSet()
	dictionaries := []*PropertyDictionary{}
	if sources.definition != nil {
		dictionaries = append(dictionaries, sources.definition.GetProperties())
	}
	dictionaries = append(dictionaries, sources.inlineProperties)
	for _, properties := range dictionaries {
		shorthands := properties.GetVarShorthands()
		for shorthandId, property := range shorthands {
			deps.clear()
			result, ok := substituteVariables(sources, property.Value.GetString(), deps, newStringSet())
			if !ok {
				continue
			}
			if !GetPropertySpecification().ParseShorthandDeclaration(out, shorthandId, result) {
				continue
			}
			if dirty != nil {
				usesDirty := false
				depKeys := deps.keys()
				for _, name := range depKeys {
					if dirty.variables.has(name) {
						usesDirty = true
					}
				}
				if usesDirty || dirty.varShorthands[shorthandId] {
					ids := GetShorthandUnderlyingProperties(shorthandId)
					dirty.properties.UnionWith(ids)
				}
			}
		}
	}
}

func transitionPropertyChanges(sources propertySources, changed *PropertyIdSet, newDefinition *ElementDefinition) {
	if sources.definition == nil || newDefinition == nil || changed.Empty() {
		return
	}
	transitionProperty := styleLocalProperty(PropertyIdTransition, sources.inlineProperties, newDefinition)
	if transitionProperty == nil || transitionProperty.Value.GetType() != VariantTRANSITIONLIST {
		return
	}
	list, ok := transitionProperty.Value.Pointer().(*TransitionList)
	if !ok || list.None {
		return
	}
	deps := newStringSet()
	var oldCache *PropertyDictionary
	var newCache *PropertyDictionary
	newSources := propertySources{sources.element, newDefinition, emptyPropertyDictionary}
	addTransition := func(transition Transition) bool {
		start := getSpecifiedProperty(sources, transition.Id).property
		start = resolveVariablesWithShorthandExpansion(sources, transition.Id, start, &oldCache, deps)
		target := getSpecifiedProperty(newSources, transition.Id).property
		target = resolveVariablesWithShorthandExpansion(newSources, transition.Id, target, &newCache, deps)
		if start != nil && target != nil && !start.Equals(target) {
			return sources.element.StartTransition(transition, *start, *target)
		}
		return false
	}
	if list.All {
		transition := list.Transitions[0]
		ids := changed.Ids()
		for _, id := range ids {
			transition.Id = id
			if addTransition(transition) {
				changed.Erase(id)
			}
		}
	} else {
		transitions := list.Transitions
		for _, transition := range transitions {
			if changed.Contains(transition.Id) {
				if addTransition(transition) {
					changed.Erase(transition.Id)
				}
			}
		}
	}
}

func (s *ElementStyle) getPropertySources() propertySources {
	return propertySources{s.element, s.definition, s.inlineProperties}
}

// UpdateDefinition re-fetches the style sheet definition and dirties every
// property whose declared value may have changed.
func (s *ElementStyle) UpdateDefinition() {
	var newDefinition *ElementDefinition
	if sheet := s.element.GetStyleSheet(); sheet != nil {
		newDefinition = sheet.GetElementDefinition(s.element)
	}
	if newDefinition == s.definition {
		return
	}
	changed := PropertyIdSet{}
	changedVariables := newStringSet()
	changedShorthands := map[ShorthandId]bool{}
	if s.definition != nil {
		changed = s.definition.GetPropertyIds()
		customs := s.definition.GetProperties().GetCustomProperties()
		for name := range customs {
			changedVariables.insert(name)
		}
		shorthands := s.definition.GetProperties().GetVarShorthands()
		for id := range shorthands {
			changedShorthands[id] = true
		}
	}
	if newDefinition != nil {
		changed.UnionWith(newDefinition.GetPropertyIds())
		customs := newDefinition.GetProperties().GetCustomProperties()
		for name := range customs {
			changedVariables.insert(name)
		}
		shorthands := newDefinition.GetProperties().GetVarShorthands()
		for id := range shorthands {
			changedShorthands[id] = true
		}
	}
	if s.definition != nil && newDefinition != nil {
		both := s.definition.GetPropertyIds().Intersection(newDefinition.GetPropertyIds())
		ids := both.Ids()
		for _, id := range ids {
			p0 := s.definition.GetProperty(id)
			p1 := newDefinition.GetProperty(id)
			if p0 != nil && p1 != nil && p0.Equals(p1) && p0.Unit != UnitVAR_EXPRESSION && p1.Unit != UnitSHORTHAND_PLACEHOLDER {
				changed.Erase(id)
			}
		}
		transitionPropertyChanges(s.getPropertySources(), &changed, newDefinition)
	}
	s.definition = newDefinition
	s.dirtyProperties.UnionWith(changed)
	names := changedVariables.keys()
	for _, name := range names {
		s.dirtyVariables.insert(name)
	}
	for id := range changedShorthands {
		s.dirtyVarShorthands[id] = true
	}
}

// SetPseudoClass sets or clears a pseudo-class; overrideClass marks it as
// set from OverridePseudoClass. Returns true if the state changed.
func (s *ElementStyle) SetPseudoClass(pseudoClass string, activate bool, overrideClass bool) bool {
	changed := false
	if activate {
		state := s.pseudoClasses[pseudoClass]
		changed = state == pseudoClassClear
		if overrideClass {
			state = state | pseudoClassOverride
		} else {
			state = state | pseudoClassSet
		}
		s.pseudoClasses[pseudoClass] = state
	} else {
		if state, ok := s.pseudoClasses[pseudoClass]; ok {
			if overrideClass {
				state = state & pseudoClassSet
			} else {
				state = state & pseudoClassOverride
			}
			if state == pseudoClassClear {
				delete(s.pseudoClasses, pseudoClass)
				changed = true
			} else {
				s.pseudoClasses[pseudoClass] = state
			}
		}
	}
	return changed
}

func (s *ElementStyle) IsPseudoClassSet(pseudoClass string) bool {
	_, ok := s.pseudoClasses[pseudoClass]
	return ok
}

// GetActivePseudoClasses returns the active pseudo-class names, sorted.
func (s *ElementStyle) GetActivePseudoClasses() []string {
	names := []string{}
	pcs := s.pseudoClasses
	for name := range pcs {
		names = append(names, name)
	}
	sortStrings(names)
	return names
}

func (s *ElementStyle) SetClass(className string, activate bool) bool {
	index := -1
	for i := 0; i < len(s.classes); i++ {
		if s.classes[i] == className {
			index = i
			break
		}
	}
	if activate {
		if index < 0 {
			s.classes = append(s.classes, className)
			return true
		}
	} else if index >= 0 {
		s.classes = append(s.classes[:index], s.classes[index+1:]...)
		return true
	}
	return false
}

func (s *ElementStyle) IsClassSet(className string) bool {
	for i := 0; i < len(s.classes); i++ {
		if s.classes[i] == className {
			return true
		}
	}
	return false
}

func (s *ElementStyle) SetClassNames(classNames string) {
	s.classes = StringExpand(classNames, ' ', false)
}

func (s *ElementStyle) GetClassNames() string {
	out := ""
	for i := 0; i < len(s.classes); i++ {
		if i != 0 {
			out = out + " "
		}
		out = out + s.classes[i]
	}
	return out
}

func (s *ElementStyle) GetClassNameList() []string { return s.classes }

func (s *ElementStyle) SetProperty(id PropertyId, property Property) bool {
	property.Definition = GetPropertyDefinition(id)
	if property.Definition == nil {
		return false
	}
	s.inlineProperties.SetProperty(id, property)
	s.DirtyProperty(id)
	return true
}

func (s *ElementStyle) SetCustomProperty(name string, property Property) {
	s.inlineProperties.SetCustomProperty(name, property)
	s.dirtyVariables.insert(name)
}

func (s *ElementStyle) SetVarShorthand(id ShorthandId, property Property) {
	s.inlineProperties.SetVarShorthand(id, property)
	s.dirtyVarShorthands[id] = true
}

func (s *ElementStyle) RemoveProperty(id PropertyId) {
	before := s.inlineProperties.GetNumProperties()
	s.inlineProperties.RemoveProperty(id)
	if s.inlineProperties.GetNumProperties() != before {
		s.DirtyProperty(id)
	}
}

func (s *ElementStyle) RemoveCustomProperty(name string) {
	if s.inlineProperties.RemoveCustomProperty(name) {
		s.dirtyVariables.insert(name)
	}
}

func (s *ElementStyle) RemoveVarShorthand(id ShorthandId) {
	if s.inlineProperties.RemoveVarShorthand(id) {
		s.dirtyVarShorthands[id] = true
	}
}

// GetProperty returns the cascaded value of id with variables resolved.
func (s *ElementStyle) GetProperty(id PropertyId) *Property {
	result := getSpecifiedProperty(s.getPropertySources(), id)
	if result.element == nil {
		return result.property
	}
	return result.element.GetStyle().resolveVariablesLocal(id, result.property, newStringSet())
}

func (s *ElementStyle) GetCustomProperty(name string) *Property {
	result := getSpecifiedCustomProperty(s.getPropertySources(), name)
	if result.element == nil || result.property == nil || result.property.Unit != UnitVAR_EXPRESSION {
		return result.property
	}
	value, ok := substituteVariables(result.element.GetStyle().getPropertySources(), result.property.Value.GetString(), newStringSet(), newStringSet())
	if !ok {
		return nil
	}
	p := new(Property)
	*p = PropertyString(value, UnitSTRING)
	return p
}

func (s *ElementStyle) GetLocalProperty(id PropertyId) *Property {
	return styleLocalProperty(id, s.inlineProperties, s.definition)
}

func (s *ElementStyle) GetLocalPropertyWithResolvedVariables(id PropertyId) *Property {
	p := styleLocalProperty(id, s.inlineProperties, s.definition)
	if p == nil {
		return nil
	}
	return s.resolveVariablesLocal(id, p, newStringSet())
}

func (s *ElementStyle) GetLocalCustomProperty(name string) *Property {
	return styleLocalCustomProperty(name, s.inlineProperties, s.definition)
}

func (s *ElementStyle) GetLocalShorthand(id ShorthandId) *Property {
	return styleLocalShorthand(id, s.inlineProperties, s.definition)
}

func (s *ElementStyle) GetLocalStyleProperties() *PropertyDictionary { return s.inlineProperties }

func (s *ElementStyle) GetDefinition() *ElementDefinition { return s.definition }

func computeLengthForElement(value NumericValue, element *Element) float32 {
	var fontSize float32 = 0
	var docFontSize float32 = 0
	var dpRatio float32 = 1
	vp := Vector2f{1, 1}
	if AnyUnit(value.Unit & UnitDP_SCALABLE_LENGTH) {
		if ctx := element.GetContext(); ctx != nil {
			dpRatio = ctx.GetDensityIndependentPixelRatio()
		}
	}
	switch value.Unit {
	case UnitEM:
		fontSize = element.GetComputedValues().FontSize()
	case UnitREM:
		if doc := element.GetOwnerDocument(); doc != nil {
			docFontSize = doc.GetComputedValues().FontSize()
		} else {
			docFontSize = DefaultComputedValues().FontSize()
		}
	case UnitVW, UnitVH:
		if ctx := element.GetContext(); ctx != nil {
			vp = ctx.GetDimensions().ToFloat()
		}
	}
	return ComputeLength(value, fontSize, docFontSize, dpRatio, vp)
}

// ResolveNumericValue resolves value against baseValue for numbers and
// percentages, and to px or rad otherwise.
func (s *ElementStyle) ResolveNumericValue(value NumericValue, baseValue float32) float32 {
	if value.Unit == UnitPX {
		return value.Number
	} else if AnyUnit(value.Unit & UnitLENGTH) {
		return computeLengthForElement(value, s.element)
	}
	switch value.Unit {
	case UnitNUMBER:
		return value.Number * baseValue
	case UnitPERCENT:
		return value.Number * baseValue * 0.01
	case UnitX:
		return value.Number
	case UnitDEG, UnitRAD:
		return ComputeAngle(value)
	}
	return 0
}

func (s *ElementStyle) ResolveRelativeLength(value NumericValue, target RelativeTarget) float32 {
	if AnyUnit(value.Unit&UnitLENGTH) && !(value.Unit == UnitEM && target == RelativeTargetParentFontSize) {
		return computeLengthForElement(value, s.element)
	}
	var base float32 = 0
	switch target {
	case RelativeTargetNone:
		base = 1
	case RelativeTargetContainingBlockWidth:
		base = s.element.GetContainingBlock().X
	case RelativeTargetContainingBlockHeight:
		base = s.element.GetContainingBlock().Y
	case RelativeTargetFontSize:
		base = s.element.GetComputedValues().FontSize()
	case RelativeTargetParentFontSize:
		if p := s.element.GetParentNode(); p != nil {
			base = p.GetComputedValues().FontSize()
		} else {
			base = DefaultComputedValues().FontSize()
		}
	case RelativeTargetLineHeight:
		base = s.element.GetLineHeight()
	}
	var scale float32 = 0
	switch value.Unit {
	case UnitEM, UnitNUMBER:
		scale = value.Number
	case UnitPERCENT:
		scale = value.Number * 0.01
	}
	return base * scale
}

func (s *ElementStyle) DirtyInheritedProperties() {
	s.dirtyProperties.UnionWith(GetRegisteredInheritedProperties())
}

// DirtyPropertiesWithUnits dirties every local property using units.
func (s *ElementStyle) DirtyPropertiesWithUnits(units Unit) {
	ids := s.iterateIds()
	for _, id := range ids {
		p := s.GetLocalProperty(id)
		if p != nil && AnyUnit(p.Unit&units) {
			s.DirtyProperty(id)
		}
	}
}

func (s *ElementStyle) DirtyPropertiesWithUnitsRecursive(units Unit) {
	s.DirtyPropertiesWithUnits(units)
	n := s.element.GetNumChildren(true)
	for i := 0; i < n; i++ {
		s.element.GetChild(i).GetStyle().DirtyPropertiesWithUnitsRecursive(units)
	}
}

func (s *ElementStyle) AnyPropertiesDirty() bool {
	return !s.dirtyProperties.Empty() || !s.dirtyVariables.empty() || len(s.dirtyVarShorthands) > 0
}

// ResolveKeyFrameProperty resolves variables in a keyframe block property.
func (s *ElementStyle) ResolveKeyFrameProperty(id PropertyId, property *Property, blockProperties *PropertyDictionary, cache **PropertyDictionary) *Property {
	sources := propertySources{s.element, s.definition, blockProperties}
	return resolveVariablesWithShorthandExpansion(sources, id, property, cache, newStringSet())
}

// iterateIds is PlainPropertiesIterator: inline properties, then
// definition properties not overridden inline, in id order.
func (s *ElementStyle) iterateIds() []PropertyId {
	set := PropertyIdSet{}
	inline := s.inlineProperties.GetProperties()
	for id := range inline {
		set.Insert(id)
	}
	if s.definition != nil {
		set.UnionWith(s.definition.GetPropertyIds())
	}
	return set.Ids()
}

// IterateLocalProperties lists (id, property) for every local property.
func (s *ElementStyle) IterateLocalProperties() []PropertyId { return s.iterateIds() }

func (s *ElementStyle) DirtyProperty(id PropertyId) { s.dirtyProperties.Insert(id) }

func (s *ElementStyle) DirtyProperties(properties PropertyIdSet) { s.dirtyProperties.UnionWith(properties) }

func (s *ElementStyle) resolveVariablesLocal(id PropertyId, property *Property, deps *stringSet) *Property {
	return resolveVariables(s.getPropertySources(), s.expandedShorthands, id, property, deps)
}

// ComputeValues resolves dirty properties into values, propagating
// inherited dirty properties to children. Returns the dirtied ids.
func (s *ElementStyle) ComputeValues(values *ComputedValues, parentValues *ComputedValues, documentValues *ComputedValues, valuesAreDefaultInitialized bool, dpRatio float32, vpDimensions Vector2f) PropertyIdSet {
	if !s.AnyPropertiesDirty() {
		return PropertyIdSet{}
	}
	if !s.dirtyVariables.empty() || len(s.dirtyVarShorthands) > 0 {
		ref := &dirtyPropertiesRef{properties: &s.dirtyProperties, variables: s.dirtyVariables, varShorthands: s.dirtyVarShorthands}
		expandVarShorthands(s.expandedShorthands, s.getPropertySources(), ref)
	}
	fontSizeBefore := values.FontSize()
	lineHeightBefore := values.LineHeight()
	if !valuesAreDefaultInitialized {
		values.CopyNonInherited(DefaultComputedValues())
	}
	if parentValues != nil {
		values.CopyInherited(parentValues)
	} else if !valuesAreDefaultInitialized {
		values.CopyInherited(DefaultComputedValues())
	}
	deps := newStringSet()
	dirtyEmProperties := false

	if s.dirtyProperties.Contains(PropertyIdFontSize) {
		if p := s.GetLocalProperty(PropertyIdFontSize); p != nil {
			deps.clear()
			p = s.resolveVariablesLocal(PropertyIdFontSize, p, deps)
			if p != nil {
				values.SetFontSize(ComputeFontsize(p.GetNumericValue(), values, parentValues, documentValues, dpRatio, vpDimensions))
			}
		} else if parentValues != nil {
			values.SetFontSize(parentValues.FontSize())
		}
		if fontSizeBefore != values.FontSize() {
			dirtyEmProperties = true
			s.dirtyProperties.Insert(PropertyIdLineHeight)
		}
	} else {
		values.SetFontSize(fontSizeBefore)
	}
	fontSize := values.FontSize()
	documentFontSize := DefaultComputedValues().FontSize()
	if documentValues != nil {
		documentFontSize = documentValues.FontSize()
	}

	if s.dirtyProperties.Contains(PropertyIdLineHeight) {
		if p := s.GetLocalProperty(PropertyIdLineHeight); p != nil {
			deps.clear()
			p = s.resolveVariablesLocal(PropertyIdLineHeight, p, deps)
			if p != nil {
				values.SetLineHeight(ComputeLineHeight(p, fontSize, documentFontSize, dpRatio, vpDimensions))
			}
		} else if parentValues != nil {
			parentLH := parentValues.LineHeight()
			if parentLH.InheritType == LineHeightNumber {
				values.SetLineHeight(StyleLineHeight{fontSize * parentLH.InheritValue, LineHeightNumber, parentLH.InheritValue})
			} else {
				values.SetLineHeight(parentLH)
			}
		}
		newLH := values.LineHeight()
		if lineHeightBefore.Value != newLH.Value || lineHeightBefore.InheritValue != newLH.InheritValue {
			s.dirtyProperties.Insert(PropertyIdVerticalAlign)
		}
	} else {
		values.SetLineHeight(lineHeightBefore)
	}

	dirtyFontFaceHandle := false
	ids := s.iterateIds()
	for _, id := range ids {
		deps.clear()
		local := s.GetLocalProperty(id)
		if local == nil {
			continue
		}
		p := s.resolveVariablesLocal(id, local, deps)
		if p == nil {
			continue
		}
		depKeys := deps.keys()
		for _, name := range depKeys {
			if s.dirtyVariables.has(name) {
				s.dirtyProperties.Insert(id)
			}
		}
		if dirtyEmProperties && p.Unit == UnitEM {
			s.dirtyProperties.Insert(id)
		}
		if s.computeValue(values, dpRatio, vpDimensions, fontSize, documentFontSize, id, p) {
			dirtyFontFaceHandle = true
		}
	}
	if dirtyFontFaceHandle {
		values.SetFontFaceHandle(GetFontEngineInterface().GetFontFaceHandle(values.FontFamily(), values.FontStyle(), values.FontWeight(), int(values.FontSize())))
	}

	dirtyInherited := s.dirtyProperties.Intersection(GetRegisteredInheritedProperties())
	if s.dirtyProperties.Contains(PropertyIdTextOverflow) {
		dirtyInherited.Insert(PropertyIdTextOverflow)
	}
	if !dirtyInherited.Empty() || !s.dirtyVariables.empty() {
		varNames := s.dirtyVariables.keys()
		n := s.element.GetNumChildren(true)
		for i := 0; i < n; i++ {
			childStyle := s.element.GetChild(i).GetStyle()
			childStyle.dirtyProperties.UnionWith(dirtyInherited)
			for _, name := range varNames {
				childStyle.dirtyVariables.insert(name)
			}
		}
	}
	result := s.dirtyProperties
	s.dirtyProperties.Clear()
	s.dirtyVariables.clear()
	s.dirtyVarShorthands = map[ShorthandId]bool{}
	return result
}

func pointerIsSet(v Variant, typ VariantType) bool {
	return v.GetType() == typ && v.Pointer() != nil
}

// computeValue assigns one resolved property; returns true when the font
// face handle must be refetched.
func (s *ElementStyle) computeValue(values *ComputedValues, dpRatio float32, vp Vector2f, fontSize float32, docFontSize float32, id PropertyId, p *Property) bool {
	switch id {
	case PropertyIdMarginTop:
		values.SetMarginTop(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdMarginRight:
		values.SetMarginRight(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdMarginBottom:
		values.SetMarginBottom(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdMarginLeft:
		values.SetMarginLeft(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdPaddingTop:
		values.SetPaddingTop(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdPaddingRight:
		values.SetPaddingRight(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdPaddingBottom:
		values.SetPaddingBottom(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdPaddingLeft:
		values.SetPaddingLeft(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdBorderTopWidth:
		values.SetBorderTopWidth(ComputeBorderWidth(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp)))
	case PropertyIdBorderRightWidth:
		values.SetBorderRightWidth(ComputeBorderWidth(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp)))
	case PropertyIdBorderBottomWidth:
		values.SetBorderBottomWidth(ComputeBorderWidth(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp)))
	case PropertyIdBorderLeftWidth:
		values.SetBorderLeftWidth(ComputeBorderWidth(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp)))
	case PropertyIdBorderTopColor:
		values.SetBorderTopColor(p.Value.GetColourb())
	case PropertyIdBorderRightColor:
		values.SetBorderRightColor(p.Value.GetColourb())
	case PropertyIdBorderBottomColor:
		values.SetBorderBottomColor(p.Value.GetColourb())
	case PropertyIdBorderLeftColor:
		values.SetBorderLeftColor(p.Value.GetColourb())
	case PropertyIdBorderTopLeftRadius:
		values.SetBorderTopLeftRadius(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp))
	case PropertyIdBorderTopRightRadius:
		values.SetBorderTopRightRadius(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp))
	case PropertyIdBorderBottomRightRadius:
		values.SetBorderBottomRightRadius(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp))
	case PropertyIdBorderBottomLeftRadius:
		values.SetBorderBottomLeftRadius(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp))
	case PropertyIdDisplay:
		values.SetDisplay(p.Value.GetInt())
	case PropertyIdPosition:
		values.SetPosition(p.Value.GetInt())
	case PropertyIdTop:
		values.SetTop(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdRight:
		values.SetRight(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdBottom:
		values.SetBottom(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdLeft:
		values.SetLeft(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdFloat:
		values.SetFloat(p.Value.GetInt())
	case PropertyIdClear:
		values.SetClear(p.Value.GetInt())
	case PropertyIdBoxSizing:
		values.SetBoxSizing(p.Value.GetInt())
	case PropertyIdZIndex:
		if p.Unit == UnitKEYWORD {
			values.SetZIndex(NumberAuto{NumberAutoAuto, 0})
		} else {
			values.SetZIndex(NumberAuto{NumberAutoNumber, p.Value.GetFloat()})
		}
	case PropertyIdWidth:
		values.SetWidth(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdMinWidth:
		values.SetMinWidth(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdMaxWidth:
		values.SetMaxWidth(ComputeMaxSize(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdHeight:
		values.SetHeight(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdMinHeight:
		values.SetMinHeight(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdMaxHeight:
		values.SetMaxHeight(ComputeMaxSize(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdVerticalAlign:
		values.SetVerticalAlign(ComputeVerticalAlign(p, values.LineHeight().Value, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdOverflowX:
		values.SetOverflowX(p.Value.GetInt())
	case PropertyIdOverflowY:
		values.SetOverflowY(p.Value.GetInt())
	case PropertyIdClip:
		values.SetClip(ComputeClip(p))
	case PropertyIdVisibility:
		values.SetVisibility(p.Value.GetInt())
	case PropertyIdTextOverflow:
		if p.Unit == UnitKEYWORD {
			values.SetTextOverflow(p.Value.GetInt())
		} else {
			values.SetTextOverflow(TextOverflowString)
		}
	case PropertyIdBackgroundColor:
		values.SetBackgroundColor(p.Value.GetColourb())
	case PropertyIdColor:
		values.SetColor(p.Value.GetColourb())
	case PropertyIdImageColor:
		values.SetImageColor(p.Value.GetColourb())
	case PropertyIdOpacity:
		values.SetOpacity(p.Value.GetFloat())
	case PropertyIdFontFamily:
		return true
	case PropertyIdFontStyle:
		values.SetFontStyle(p.Value.GetInt())
		return true
	case PropertyIdFontWeight:
		values.SetFontWeight(p.Value.GetInt())
		return true
	case PropertyIdFontSize:
		return true
	case PropertyIdFontKerning:
		values.SetFontKerning(p.Value.GetInt())
		return true
	case PropertyIdLetterSpacing:
		values.SetHasLetterSpacing(p.Unit != UnitKEYWORD)
		return true
	case PropertyIdTextAlign:
		values.SetTextAlign(p.Value.GetInt())
	case PropertyIdTextDecoration:
		values.SetTextDecoration(p.Value.GetInt())
	case PropertyIdTextTransform:
		values.SetTextTransform(p.Value.GetInt())
	case PropertyIdWhiteSpace:
		values.SetWhiteSpace(p.Value.GetInt())
	case PropertyIdWordBreak:
		values.SetWordBreak(p.Value.GetInt())
	case PropertyIdRowGap:
		values.SetRowGap(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdColumnGap:
		values.SetColumnGap(ComputeLengthPercentage(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdDrag:
		values.SetDrag(p.Value.GetInt())
	case PropertyIdTabIndex:
		values.SetTabIndex(p.Value.GetInt())
	case PropertyIdFocus:
		values.SetFocus(p.Value.GetInt())
	case PropertyIdScrollbarMargin:
		values.SetScrollbarMargin(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp))
	case PropertyIdOverscrollBehavior:
		values.SetOverscrollBehavior(p.Value.GetInt())
	case PropertyIdPointerEvents:
		values.SetPointerEvents(p.Value.GetInt())
	case PropertyIdPerspective:
		if p.Unit == UnitKEYWORD {
			values.SetPerspective(0)
		} else {
			values.SetPerspective(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp))
		}
		values.SetHasLocalPerspective(values.Perspective() > 0)
	case PropertyIdPerspectiveOriginX:
		values.SetPerspectiveOriginX(ComputeOrigin(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdPerspectiveOriginY:
		values.SetPerspectiveOriginY(ComputeOrigin(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdTransform:
		values.SetHasLocalTransform(p.Value.Pointer() != nil)
	case PropertyIdTransformOriginX:
		values.SetTransformOriginX(ComputeOrigin(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdTransformOriginY:
		values.SetTransformOriginY(ComputeOrigin(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdTransformOriginZ:
		values.SetTransformOriginZ(ComputeLength(p.GetNumericValue(), fontSize, docFontSize, dpRatio, vp))
	case PropertyIdDecorator:
		values.SetHasDecorator(p.Unit == UnitDECORATOR && pointerIsSet(p.Value, VariantDECORATORSPTR))
	case PropertyIdMaskImage:
		values.SetHasMaskImage(p.Unit == UnitDECORATOR && pointerIsSet(p.Value, VariantDECORATORSPTR))
	case PropertyIdFontEffect:
		values.SetHasFontEffect(p.Unit == UnitFONTEFFECT && pointerIsSet(p.Value, VariantFONTEFFECTSPTR))
	case PropertyIdFilter:
		values.SetHasFilter(p.Unit == UnitFILTER && pointerIsSet(p.Value, VariantFILTERSPTR))
	case PropertyIdBackdropFilter:
		values.SetHasBackdropFilter(p.Unit == UnitFILTER && pointerIsSet(p.Value, VariantFILTERSPTR))
	case PropertyIdBoxShadow:
		hasShadow := false
		if p.Unit == UnitBOXSHADOWLIST {
			if list, ok := p.Value.Pointer().(*BoxShadowList); ok && len(list.Shadows) > 0 {
				hasShadow = true
			}
		}
		values.SetHasBoxShadow(hasShadow)
	case PropertyIdFlexBasis:
		values.SetFlexBasis(ComputeLengthPercentageAuto(p, fontSize, docFontSize, dpRatio, vp))
	case PropertyIdRmlUi_Language:
		values.SetLanguage(p.Value.GetString())
	case PropertyIdRmlUi_Direction:
		values.SetDirection(p.Value.GetInt())
	}
	return false
}
