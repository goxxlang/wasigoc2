// Port of RmlUi Source/Core/PropertySpecification.cpp and
// Include/RmlUi/Core/PropertySpecification.h.
package rmlui

// ShorthandType is Rml::ShorthandType.
type ShorthandType = int

const (
	// Properties that fail to parse fall through to the next until they parse
	// correctly; undeclared ones are not set.
	ShorthandTypeFallThrough ShorthandType = iota
	// A single failed parse aborts; undeclared ones replicate the last value.
	ShorthandTypeReplicate
	// 'padding', 'margin', ...: up to four values.
	ShorthandTypeBox
	// The full value string is applied to each property or shorthand.
	ShorthandTypeRecursiveRepeat
	// Comma-separated list of properties or shorthands.
	ShorthandTypeRecursiveCommaSeparated
	// 'flex': special defaults, otherwise FallThrough.
	ShorthandTypeFlex
)

type ShorthandItemType = int

const (
	ShorthandItemTypeInvalid ShorthandItemType = iota
	ShorthandItemTypeProperty
	ShorthandItemTypeShorthand
)

// ShorthandItem is Rml::ShorthandItem.
type ShorthandItem struct {
	Type               ShorthandItemType
	PropertyId         PropertyId
	ShorthandId        ShorthandId
	PropertyDef        *PropertyDefinition
	ShorthandDef       *ShorthandDefinition
	Optional           bool
	Repeats            bool
}

// ShorthandDefinition is Rml::ShorthandDefinition.
type ShorthandDefinition struct {
	Id        ShorthandId
	Items     []ShorthandItem
	Type      ShorthandType
	Inherited bool
}

type parsePropertyResult = int

const (
	parsePropertySuccess parsePropertyResult = iota
	parsePropertyContainsVariable
	parsePropertyError
)

type splitOption = int

const (
	splitOptionNone splitOption = iota
	splitOptionWhitespace
	splitOptionComma
)

// PropertySpecification is Rml::PropertySpecification.
type PropertySpecification struct {
	properties               []*PropertyDefinition
	shorthands               []*ShorthandDefinition
	propertyMap              *IdNameMap
	shorthandMap             *IdNameMap
	propertyIds              PropertyIdSet
	propertyIdsInherited     PropertyIdSet
	propertyIdsForcingLayout PropertyIdSet
}

func NewPropertySpecification(reserveNumProperties int, reserveNumShorthands int) *PropertySpecification {
	s := &PropertySpecification{}
	s.properties = make([]*PropertyDefinition, reserveNumProperties+1)
	s.shorthands = make([]*ShorthandDefinition, reserveNumShorthands+1)
	s.propertyMap = NewIdNameMap(reserveNumProperties + 1)
	s.shorthandMap = NewIdNameMap(reserveNumShorthands + 1)
	return s
}

// RegisterProperty registers a property; id Invalid allocates a custom id.
func (s *PropertySpecification) RegisterProperty(propertyName string, defaultValue string, inherited bool, forcesLayout bool, id PropertyId) *PropertyDefinition {
	if id == PropertyIdInvalid {
		id = s.propertyMap.GetOrCreateId(propertyName)
	} else {
		s.propertyMap.AddPair(id, propertyName)
	}
	if id >= PropertyIdMaxNumIds {
		LogMessage(LogError, "Fatal error while registering property '"+propertyName+"': Maximum number of allowed properties exceeded.")
		return NewPropertyDefinition(PropertyIdInvalid, defaultValue, inherited, forcesLayout)
	}
	if id < len(s.properties) {
		if s.properties[id] != nil {
			LogMessage(LogError, "While registering property '"+propertyName+"': The property is already registered.")
			return s.properties[id]
		}
	} else {
		for len(s.properties) <= id {
			s.properties = append(s.properties, nil)
		}
	}
	def := NewPropertyDefinition(id, defaultValue, inherited, forcesLayout)
	s.properties[id] = def
	s.propertyIds.Insert(id)
	if inherited {
		s.propertyIdsInherited.Insert(id)
	}
	if forcesLayout {
		s.propertyIdsForcingLayout.Insert(id)
	}
	return def
}

func (s *PropertySpecification) GetProperty(id PropertyId) *PropertyDefinition {
	if id == PropertyIdInvalid || id < 0 || id >= len(s.properties) {
		return nil
	}
	return s.properties[id]
}

func (s *PropertySpecification) GetPropertyByName(name string) *PropertyDefinition {
	return s.GetProperty(s.propertyMap.GetId(name))
}

func (s *PropertySpecification) GetRegisteredProperties() PropertyIdSet { return s.propertyIds }
func (s *PropertySpecification) GetRegisteredInheritedProperties() PropertyIdSet {
	return s.propertyIdsInherited
}
func (s *PropertySpecification) GetRegisteredPropertiesForcingLayout() PropertyIdSet {
	return s.propertyIdsForcingLayout
}

func (s *PropertySpecification) GetPropertyId(name string) PropertyId { return s.propertyMap.GetId(name) }
func (s *PropertySpecification) GetShorthandId(name string) ShorthandId {
	return s.shorthandMap.GetId(name)
}
func (s *PropertySpecification) GetPropertyName(id PropertyId) string   { return s.propertyMap.GetName(id) }
func (s *PropertySpecification) GetShorthandName(id ShorthandId) string { return s.shorthandMap.GetName(id) }

// RegisterShorthand registers a shorthand over comma-separated property
// names; a trailing '?' marks an item optional, '#' repeating.
func (s *PropertySpecification) RegisterShorthand(shorthandName string, propertyNames string, shType ShorthandType, id ShorthandId) ShorthandId {
	if id == ShorthandIdInvalid {
		id = s.shorthandMap.GetOrCreateId(shorthandName)
	} else {
		s.shorthandMap.AddPair(id, shorthandName)
	}
	list := StringExpandList(StringToLower(propertyNames))
	def := &ShorthandDefinition{}
	for _, rawName := range list {
		item := ShorthandItem{}
		optional := false
		repeats := false
		name := rawName
		if name != "" && name[len(name)-1] == '?' {
			optional = true
			name = name[:len(name)-1]
		}
		if name != "" && name[len(name)-1] == '#' {
			repeats = true
			name = name[:len(name)-1]
		}
		propertyId := s.propertyMap.GetId(name)
		if propertyId != PropertyIdInvalid {
			if p := s.GetProperty(propertyId); p != nil {
				item = ShorthandItem{Type: ShorthandItemTypeProperty, PropertyId: propertyId, PropertyDef: p, Optional: optional, Repeats: repeats}
			}
		} else {
			shId := s.shorthandMap.GetId(name)
			if shId != ShorthandIdInvalid && (shType == ShorthandTypeRecursiveRepeat || shType == ShorthandTypeRecursiveCommaSeparated) {
				if sh := s.GetShorthand(shId); sh != nil {
					item = ShorthandItem{Type: ShorthandItemTypeShorthand, ShorthandId: shId, ShorthandDef: sh, Optional: optional, Repeats: repeats}
				}
			}
		}
		if item.Type == ShorthandItemTypeInvalid {
			LogMessage(LogError, "Shorthand property '"+shorthandName+"' was registered with invalid property '"+name+"'.")
			return ShorthandIdInvalid
		}
		def.Items = append(def.Items, item)
	}
	def.Id = id
	def.Type = shType
	rng1 := def.Items
	for _, item := range rng1 {
		if item.Type == ShorthandItemTypeProperty && item.PropertyDef.IsInherited() {
			def.Inherited = true
		}
		if item.Type == ShorthandItemTypeShorthand && item.ShorthandDef.Inherited {
			def.Inherited = true
		}
	}
	if id >= ShorthandIdMaxNumIds {
		LogMessage(LogError, "Error while registering shorthand '"+shorthandName+"': Maximum number of allowed shorthands exceeded.")
		return ShorthandIdInvalid
	}
	if id < len(s.shorthands) {
		if s.shorthands[id] != nil {
			LogMessage(LogError, "The shorthand '"+shorthandName+"' already exists, ignoring.")
			return ShorthandIdInvalid
		}
	} else {
		for len(s.shorthands) <= id {
			s.shorthands = append(s.shorthands, nil)
		}
	}
	s.shorthands[id] = def
	return id
}

func (s *PropertySpecification) GetShorthand(id ShorthandId) *ShorthandDefinition {
	if id == ShorthandIdInvalid || id < 0 || id >= len(s.shorthands) {
		return nil
	}
	return s.shorthands[id]
}

func (s *PropertySpecification) GetShorthandByName(name string) *ShorthandDefinition {
	return s.GetShorthand(s.shorthandMap.GetId(name))
}

// ParsePropertyDeclaration parses "name: value" into the dictionary as a
// property, a shorthand, or a custom (--name) property.
func (s *PropertySpecification) ParsePropertyDeclaration(dictionary *PropertyDictionary, propertyName string, propertyValue string) bool {
	propertyId := s.propertyMap.GetId(propertyName)
	if propertyId != PropertyIdInvalid {
		return s.ParsePropertyDeclarationId(dictionary, propertyId, propertyValue)
	}
	shorthandId := s.shorthandMap.GetId(propertyName)
	if shorthandId != ShorthandIdInvalid {
		return s.ParseShorthandDeclaration(dictionary, shorthandId, propertyValue)
	}
	if StringStartsWith(propertyName, "--") {
		unit := UnitUNKNOWN
		result := parsePropertySuccess
		if propertyValue != "" {
			_, result = s.parsePropertyValues(propertyValue, splitOptionNone)
		}
		switch result {
		case parsePropertySuccess:
			unit = UnitSTRING
		case parsePropertyContainsVariable:
			unit = UnitVAR_EXPRESSION
		default:
			return false
		}
		dictionary.SetCustomProperty(propertyName, PropertyString(propertyValue, unit))
		return true
	}
	return false
}

func (s *PropertySpecification) ParsePropertyDeclarationId(dictionary *PropertyDictionary, propertyId PropertyId, propertyValue string) bool {
	def := s.GetProperty(propertyId)
	if def == nil {
		return false
	}
	values, result := s.parsePropertyValues(propertyValue, splitOptionNone)
	switch result {
	case parsePropertyContainsVariable:
		dictionary.SetProperty(propertyId, PropertyString(propertyValue, UnitVAR_EXPRESSION))
		return true
	case parsePropertyError:
		return false
	}
	newProperty := NewProperty()
	if !def.ParseValue(&newProperty, values[0]) {
		return false
	}
	dictionary.SetProperty(propertyId, newProperty)
	return true
}

func (s *PropertySpecification) setShorthandPropertiesToPendingSubstitution(dictionary *PropertyDictionary, def *ShorthandDefinition) {
	rng2 := def.Items
	for _, item := range rng2 {
		if item.Type == ShorthandItemTypeProperty {
			dictionary.SetProperty(item.PropertyId, PropertyOf(NewVariant(), UnitSHORTHAND_PLACEHOLDER))
		} else if item.Type == ShorthandItemTypeShorthand {
			s.setShorthandPropertiesToPendingSubstitution(dictionary, item.ShorthandDef)
		}
	}
}

func (s *PropertySpecification) ParseShorthandDeclaration(dictionary *PropertyDictionary, shorthandId ShorthandId, propertyValue string) bool {
	def := s.GetShorthand(shorthandId)
	if def == nil {
		return false
	}
	split := splitOptionWhitespace
	if def.Type == ShorthandTypeRecursiveCommaSeparated {
		split = splitOptionComma
	}
	values, result := s.parsePropertyValues(propertyValue, split)
	switch result {
	case parsePropertyContainsVariable:
		dictionary.SetVarShorthand(shorthandId, PropertyString(propertyValue, UnitVAR_EXPRESSION))
		s.setShorthandPropertiesToPendingSubstitution(dictionary, def)
		return true
	case parsePropertyError:
		return false
	}

	if def.Type == ShorthandTypeFlex && len(values) > 0 {
		if values[0] == "none" {
			values = []string{"0", "0", "auto"}
		} else {
			defaults := []string{"1", "1", "0"}
			for i := 0; i < 3; i++ {
				item := def.Items[i]
				np := NewProperty()
				item.PropertyDef.ParseValue(&np, defaults[i])
				dictionary.SetProperty(item.PropertyId, np)
			}
		}
	}

	if def.Type == ShorthandTypeBox && len(values) < 4 {
		sideToValue := []int{0, 0, 0, 0}
		switch len(values) {
		case 1:
			sideToValue = []int{0, 0, 0, 0}
		case 2:
			sideToValue = []int{0, 1, 0, 1}
		case 3:
			sideToValue = []int{0, 1, 2, 1}
		}
		for i := 0; i < 4; i++ {
			np := NewProperty()
			if !def.Items[i].PropertyDef.ParseValue(&np, values[sideToValue[i]]) {
				return false
			}
			dictionary.SetProperty(def.Items[i].PropertyDef.GetId(), np)
		}
	} else if def.Type == ShorthandTypeRecursiveRepeat {
		ok := true
		rng3 := def.Items
		for _, item := range rng3 {
			if item.Type == ShorthandItemTypeProperty {
				if !s.ParsePropertyDeclarationId(dictionary, item.PropertyId, propertyValue) {
					ok = false
				}
			} else if item.Type == ShorthandItemTypeShorthand {
				if !s.ParseShorthandDeclaration(dictionary, item.ShorthandId, propertyValue) {
					ok = false
				}
			} else {
				ok = false
			}
		}
		if !ok {
			return false
		}
	} else if def.Type == ShorthandTypeRecursiveCommaSeparated {
		numOptional := 0
		rng4 := def.Items
		for _, item := range rng4 {
			if item.Optional {
				numOptional++
			}
		}
		if len(values)+numOptional < len(def.Items) {
			return false
		}
		subvalueI := 0
		for i := 0; i < len(def.Items) && subvalueI < len(values); i++ {
			ok := false
			subvalue := values[subvalueI]
			item := def.Items[i]
			if item.Repeats {
				values = values[subvalueI:]
				subvalue = StringJoin(values, ',')
			}
			if item.Type == ShorthandItemTypeProperty {
				ok = s.ParsePropertyDeclarationId(dictionary, item.PropertyId, subvalue)
			} else if item.Type == ShorthandItemTypeShorthand {
				ok = s.ParseShorthandDeclaration(dictionary, item.ShorthandId, subvalue)
			}
			if ok {
				subvalueI++
			} else if item.Repeats || !item.Optional {
				return false
			}
			if item.Repeats {
				break
			}
		}
	} else {
		if len(values) > len(def.Items) {
			return false
		}
		valueIndex := 0
		propertyIndex := 0
		for valueIndex < len(values) && propertyIndex < len(def.Items) {
			np := NewProperty()
			if !def.Items[propertyIndex].PropertyDef.ParseValue(&np, values[valueIndex]) {
				if def.Type == ShorthandTypeFallThrough || def.Type == ShorthandTypeFlex {
					if propertyIndex+1 < len(def.Items) {
						propertyIndex++
						continue
					}
				}
				return false
			}
			dictionary.SetProperty(def.Items[propertyIndex].PropertyId, np)
			if def.Type != ShorthandTypeReplicate || valueIndex < len(values)-1 {
				valueIndex++
			}
			propertyIndex++
		}
		if def.Type != ShorthandTypeReplicate && valueIndex < len(values) && propertyIndex >= len(def.Items) {
			return false
		}
	}
	return true
}

// SetPropertyDefaults sets the default of every property not already set.
func (s *PropertySpecification) SetPropertyDefaults(dictionary *PropertyDictionary) {
	rng5 := s.properties
	for _, p := range rng5 {
		if p != nil && dictionary.GetProperty(p.GetId()) == nil {
			dictionary.SetProperty(p.GetId(), *p.GetDefaultValue())
		}
	}
}

// PropertiesToString prints properties in increasing id order.
func (s *PropertySpecification) PropertiesToString(dictionary *PropertyDictionary, includeName bool, delimiter byte) string {
	props := dictionary.GetProperties()
	result := ""
	for id := 1; id < PropertyIdMaxNumIds; id++ {
		p, ok := props[id]
		if !ok {
			continue
		}
		if includeName {
			result = result + s.propertyMap.GetName(id) + ": "
		}
		result = result + p.ToString() + string([]byte{delimiter})
	}
	if result != "" {
		result = result[:len(result)-1]
	}
	return result
}

func isIdentifierChar(c byte) bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'
}

func endsWithVariable(s string) bool {
	return StringEndsWith(s, "var") && (len(s) == 3 || !isIdentifierChar(s[len(s)-4]))
}

func isAllWhitespace(s string) bool {
	for i := 0; i < len(s); i++ {
		if !StringIsWhitespace(s[i]) {
			return false
		}
	}
	return true
}

const (
	stValue = iota
	stParenthesis
	stQuote
	stQuoteEscapeNext
)

// parsePropertyValues splits a declaration value per the RCSS rules,
// detecting var() references.
func (s *PropertySpecification) parsePropertyValues(values string, option splitOption) ([]string, parsePropertyResult) {
	splitValues := option != splitOptionNone
	splitByComma := option == splitOptionComma
	splitByWhitespace := option == splitOptionWhitespace
	list := []string{}
	value := []byte{}

	state := stValue
	openParentheses := 0
	var openQuote byte = 0

	for i := 0; i < len(values); i++ {
		c := values[i]
		switch state {
		case stValue:
			if c == ';' {
				if len(value) > 0 {
					list = append(list, string(value))
					value = []byte{}
				}
			} else if (splitByComma && c == ',') || (splitByWhitespace && StringIsWhitespace(c)) {
				v := StringStripWhitespace(string(value))
				if v != "" {
					list = append(list, v)
				}
				value = []byte{}
			} else if c == '"' || c == '\'' {
				state = stQuote
				openQuote = c
				if splitByWhitespace {
					v := StringStripWhitespace(string(value))
					if v != "" {
						list = append(list, v)
					}
					value = []byte{}
				} else if splitByComma {
					value = append(value, c)
				} else if isAllWhitespace(string(value)) {
					value = []byte{}
				} else {
					return []string{}, parsePropertyError
				}
			} else if c == '(' {
				if endsWithVariable(string(value)) {
					return []string{}, parsePropertyContainsVariable
				}
				openParentheses = 1
				value = append(value, c)
				state = stParenthesis
			} else {
				value = append(value, c)
			}
		case stParenthesis:
			if c == '(' {
				if endsWithVariable(string(value)) {
					return []string{}, parsePropertyContainsVariable
				}
				openParentheses++
			} else if c == ')' {
				openParentheses--
				if openParentheses == 0 {
					state = stValue
				}
			} else if c == '"' || c == '\'' {
				state = stQuote
				openQuote = c
			}
			value = append(value, c)
		case stQuote:
			if c == openQuote {
				if openParentheses == 0 {
					state = stValue
					if splitByComma {
						value = append(value, c)
					} else {
						list = append(list, string(value))
						value = []byte{}
					}
				} else {
					state = stParenthesis
					value = append(value, c)
				}
			} else if c == '\\' {
				state = stQuoteEscapeNext
			} else {
				value = append(value, c)
			}
		case stQuoteEscapeNext:
			if c == '"' || c == '\'' || c == '\\' {
				value = append(value, c)
			} else {
				value = append(value, '\\', c)
			}
			state = stQuote
		}
	}
	if state == stValue {
		v := StringStripWhitespace(string(value))
		if v != "" {
			list = append(list, v)
		}
	}
	if !splitValues && len(list) > 1 {
		return []string{}, parsePropertyError
	}
	if len(list) == 0 {
		return []string{}, parsePropertyError
	}
	return list, parsePropertySuccess
}
