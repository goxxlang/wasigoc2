// Port of RmlUi Source/Core/PropertyDefinition.cpp and
// Include/RmlUi/Core/PropertyDefinition.h.
package rmlui

// RelativeTarget is Rml::RelativeTarget.
type RelativeTarget = int

const (
	RelativeTargetNone RelativeTarget = iota
	RelativeTargetContainingBlockWidth
	RelativeTargetContainingBlockHeight
	RelativeTargetFontSize
	RelativeTargetParentFontSize
	RelativeTargetLineHeight
)

type propertyParserState struct {
	parser     PropertyParser
	name       string
	parameters map[string]int
	// order keeps the declaration order of keyword names, so GetValue finds
	// the first name for a keyword value deterministically.
	order []string
}

// PropertyDefinition is Rml::PropertyDefinition.
type PropertyDefinition struct {
	id             PropertyId
	defaultValue   Property
	inherited      bool
	forcesLayout   bool
	parsers        []propertyParserState
	relativeTarget RelativeTarget
}

func NewPropertyDefinition(id PropertyId, defaultValue string, inherited bool, forcesLayout bool) *PropertyDefinition {
	d := &PropertyDefinition{id: id, inherited: inherited, forcesLayout: forcesLayout, relativeTarget: RelativeTargetNone}
	d.defaultValue = PropertyString(defaultValue, UnitUNKNOWN)
	return d
}

// AddParser registers a parser by name, with an optional comma-separated
// parameter list such as "normal=400, bold=700".
func (d *PropertyDefinition) AddParser(parserName string, parserParameters string) *PropertyDefinition {
	state := propertyParserState{name: parserName, parameters: map[string]int{}}
	state.parser = GetPropertyParser(parserName)
	if state.parser == nil {
		LogMessage(LogError, "Property was registered with invalid parser '"+parserName+"'.")
		return d
	}
	if parserParameters != "" {
		list := StringExpandList(parserParameters)
		parameterValue := 0
		for _, parameter := range list {
			eq := indexByte(parameter, '=')
			name := parameter
			if eq >= 0 {
				v, ok := ScanInt(parameter[eq+1:])
				if !ok {
					LogMessage(LogError, "Parser was added with invalid parameter '"+parameter+"'.")
					return d
				}
				parameterValue = v
				name = parameter[:eq]
			}
			state.parameters[name] = parameterValue
			state.order = append(state.order, name)
			parameterValue++
		}
	}
	parserIndex := len(d.parsers)
	d.parsers = append(d.parsers, state)
	if d.defaultValue.Unit == UnitUNKNOWN {
		unparsed := d.defaultValue.Value.GetString()
		if state.parser.ParseValue(&d.defaultValue, unparsed, state.parameters) {
			d.defaultValue.ParserIndex = parserIndex
		} else {
			d.defaultValue.Value = VariantString(unparsed)
			d.defaultValue.Unit = UnitUNKNOWN
		}
	}
	return d
}

// AddParserPlain is AddParser(name) with no parameters.
func (d *PropertyDefinition) AddParserPlain(parserName string) *PropertyDefinition {
	return d.AddParser(parserName, "")
}

// ParseValue tries each registered parser in order.
func (d *PropertyDefinition) ParseValue(property *Property, value string) bool {
	for i := 0; i < len(d.parsers); i++ {
		if d.parsers[i].parser.ParseValue(property, value, d.parsers[i].parameters) {
			property.Definition = d
			property.ParserIndex = i
			return true
		}
	}
	property.Unit = UnitUNKNOWN
	return false
}

// GetValue converts a parsed property back into a string. Keywords yield
// their name; the boolean mirrors the C++ (false for keywords).
func (d *PropertyDefinition) GetValue(property *Property) (string, bool) {
	value := property.Value.GetString()
	if property.Unit == UnitKEYWORD {
		parserIndex := property.ParserIndex
		if parserIndex < 0 || parserIndex >= len(d.parsers) {
			parserIndex = -1
			for i := 0; i < len(d.parsers); i++ {
				if d.parsers[i].name == "keyword" {
					parserIndex = i
					break
				}
			}
			if parserIndex < 0 {
				return value, false
			}
		}
		keyword := property.Value.GetInt()
		rng1 := d.parsers[parserIndex].order
		for _, name := range rng1 {
			if d.parsers[parserIndex].parameters[name] == keyword {
				value = name
				break
			}
		}
		return value, false
	}
	return value + UnitSuffix(property.Unit), true
}

func (d *PropertyDefinition) IsInherited() bool                  { return d.inherited }
func (d *PropertyDefinition) IsLayoutForced() bool               { return d.forcesLayout }
func (d *PropertyDefinition) GetDefaultValue() *Property         { return &d.defaultValue }
func (d *PropertyDefinition) GetRelativeTarget() RelativeTarget  { return d.relativeTarget }
func (d *PropertyDefinition) GetId() PropertyId                  { return d.id }

func (d *PropertyDefinition) SetRelativeTarget(target RelativeTarget) *PropertyDefinition {
	d.relativeTarget = target
	return d
}
