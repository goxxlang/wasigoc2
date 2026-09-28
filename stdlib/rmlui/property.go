// Port of RmlUi Source/Core/Property.cpp, PropertyDictionary.cpp, and
// Include/RmlUi/Core/Property.h, PropertyDictionary.h, PropertyIdSet.h.
package rmlui

// PropertySource is Rml::PropertySource: where a declaration came from.
type PropertySource struct {
	Path       string
	LineNumber int
	RuleName   string
}

// Property is Rml::Property.
type Property struct {
	Value       Variant
	Unit        Unit
	Specificity int
	Definition  *PropertyDefinition
	ParserIndex int
	Source      *PropertySource
}

func NewProperty() Property {
	return Property{Value: NewVariant(), Unit: UnitUNKNOWN, Specificity: -1, ParserIndex: -1}
}

// PropertyOf is Property(value, unit, specificity = -1).
func PropertyOf(value Variant, unit Unit) Property {
	return Property{Value: value, Unit: unit, Specificity: -1, ParserIndex: -1}
}

func PropertyFloat(value float32, unit Unit) Property { return PropertyOf(VariantFloat(value), unit) }

func PropertyString(value string, unit Unit) Property { return PropertyOf(VariantString(value), unit) }

// PropertyKeyword is Property(EnumType value).
func PropertyKeyword(value int) Property { return PropertyOf(VariantInt(value), UnitKEYWORD) }

// ToString is Property::ToString.
func (p *Property) ToString() string {
	if p.Definition == nil {
		return p.Value.GetString() + UnitSuffix(p.Unit)
	}
	s, _ := p.Definition.GetValue(p)
	return s
}

// GetNumericValue is Property::GetNumericValue.
func (p *Property) GetNumericValue() NumericValue {
	result := NumericValue{}
	if AnyUnit(p.Unit & UnitNUMERIC) {
		if f, ok := p.Value.GetFloatOk(); ok {
			result.Number = f
			result.Unit = p.Unit
		}
	}
	return result
}

func (p *Property) GetFloat() float32   { return p.Value.GetFloat() }
func (p *Property) GetInt() int         { return p.Value.GetInt() }
func (p *Property) GetString() string   { return p.Value.GetString() }
func (p *Property) GetColourb() Colourb { return p.Value.GetColourb() }

// Equals is Property::operator==.
func (p *Property) Equals(o *Property) bool { return p.Unit == o.Unit && p.Value.Equals(o.Value) }

// PropertyIdSet is Rml::PropertyIdSet: a bitset over PropertyIdMaxNumIds.
type PropertyIdSet struct {
	bits [2]uint64
}

func (s *PropertyIdSet) Insert(id PropertyId) { s.bits[id>>6] |= uint64(1) << uint(id&63) }
func (s *PropertyIdSet) Erase(id PropertyId)  { s.bits[id>>6] &^= uint64(1) << uint(id&63) }
func (s *PropertyIdSet) Clear()               { s.bits[0] = 0; s.bits[1] = 0 }
func (s PropertyIdSet) Empty() bool           { return s.bits[0] == 0 && s.bits[1] == 0 }
func (s PropertyIdSet) Contains(id PropertyId) bool {
	if id < 0 || id >= PropertyIdMaxNumIds {
		return false
	}
	return s.bits[id>>6]&(uint64(1)<<uint(id&63)) != 0
}

func (s PropertyIdSet) Size() int {
	n := 0
	for i := 0; i < 2; i++ {
		b := s.bits[i]
		for b != 0 {
			b = b & (b - 1)
			n++
		}
	}
	return n
}

func (s *PropertyIdSet) UnionWith(o PropertyIdSet) {
	s.bits[0] |= o.bits[0]
	s.bits[1] |= o.bits[1]
}

func (s PropertyIdSet) Union(o PropertyIdSet) PropertyIdSet {
	r := PropertyIdSet{}
	r.bits[0] = s.bits[0] | o.bits[0]
	r.bits[1] = s.bits[1] | o.bits[1]
	return r
}

func (s PropertyIdSet) Intersection(o PropertyIdSet) PropertyIdSet {
	r := PropertyIdSet{}
	r.bits[0] = s.bits[0] & o.bits[0]
	r.bits[1] = s.bits[1] & o.bits[1]
	return r
}

// Ids lists the contained ids in increasing order (the iterator in C++).
func (s PropertyIdSet) Ids() []PropertyId {
	out := []PropertyId{}
	for id := 1; id < PropertyIdMaxNumIds; id++ {
		if s.Contains(id) {
			out = append(out, id)
		}
	}
	return out
}

// PropertyDictionary is Rml::PropertyDictionary. Properties are stored
// behind pointers so GetProperty can hand out stable references, as the
// C++ returns pointers into its map storage.
type PropertyDictionary struct {
	properties       map[PropertyId]*Property
	customProperties map[string]*Property
	varShorthands    map[ShorthandId]*Property
}

func NewPropertyDictionary() *PropertyDictionary {
	return &PropertyDictionary{properties: map[PropertyId]*Property{}, customProperties: map[string]*Property{}, varShorthands: map[ShorthandId]*Property{}}
}

func heapProperty(property Property) *Property {
	p := new(Property)
	*p = property
	return p
}

func (d *PropertyDictionary) SetProperty(id PropertyId, property Property) {
	d.properties[id] = heapProperty(property)
}

func (d *PropertyDictionary) RemoveProperty(id PropertyId) { delete(d.properties, id) }

// GetProperty returns the property with id, or nil.
func (d *PropertyDictionary) GetProperty(id PropertyId) *Property {
	if p, ok := d.properties[id]; ok {
		return p
	}
	return nil
}

func (d *PropertyDictionary) SetCustomProperty(name string, property Property) {
	d.customProperties[name] = heapProperty(property)
}

func (d *PropertyDictionary) RemoveCustomProperty(name string) bool {
	if _, ok := d.customProperties[name]; ok {
		delete(d.customProperties, name)
		return true
	}
	return false
}

func (d *PropertyDictionary) GetCustomProperty(name string) *Property {
	if p, ok := d.customProperties[name]; ok {
		return p
	}
	return nil
}

func (d *PropertyDictionary) SetVarShorthand(id ShorthandId, property Property) {
	d.varShorthands[id] = heapProperty(property)
}

func (d *PropertyDictionary) RemoveVarShorthand(id ShorthandId) bool {
	if _, ok := d.varShorthands[id]; ok {
		delete(d.varShorthands, id)
		return true
	}
	return false
}

func (d *PropertyDictionary) GetVarShorthand(id ShorthandId) *Property {
	if p, ok := d.varShorthands[id]; ok {
		return p
	}
	return nil
}

func (d *PropertyDictionary) GetVarShorthands() map[ShorthandId]*Property { return d.varShorthands }

func (d *PropertyDictionary) Empty() bool {
	return len(d.properties) == 0 && len(d.customProperties) == 0 && len(d.varShorthands) == 0
}

func (d *PropertyDictionary) GetNumProperties() int { return len(d.properties) }

func (d *PropertyDictionary) GetProperties() map[PropertyId]*Property { return d.properties }

func (d *PropertyDictionary) GetCustomProperties() map[string]*Property { return d.customProperties }

// Import copies other's properties, overriding specificity when > 0.
func (d *PropertyDictionary) Import(other *PropertyDictionary, propertySpecificity int) {
	props := other.properties
	for id, p := range props {
		s := p.Specificity
		if propertySpecificity > 0 {
			s = propertySpecificity
		}
		d.setPropertySpecificity(id, *p, s)
	}
	customs := other.customProperties
	for name, p := range customs {
		s := p.Specificity
		if propertySpecificity > 0 {
			s = propertySpecificity
		}
		d.setCustomPropertySpecificity(name, *p, s)
	}
	shorthands := other.varShorthands
	for id, p := range shorthands {
		s := p.Specificity
		if propertySpecificity > 0 {
			s = propertySpecificity
		}
		d.setVarShorthandSpecificity(id, *p, s)
	}
}

// Merge copies other's properties with their specificity offset.
func (d *PropertyDictionary) Merge(other *PropertyDictionary, specificityOffset int) {
	props := other.properties
	for id, p := range props {
		d.setPropertySpecificity(id, *p, p.Specificity+specificityOffset)
	}
	customs := other.customProperties
	for name, p := range customs {
		d.setCustomPropertySpecificity(name, *p, p.Specificity+specificityOffset)
	}
	shorthands := other.varShorthands
	for id, p := range shorthands {
		d.setVarShorthandSpecificity(id, *p, p.Specificity+specificityOffset)
	}
}

// Clone is the C++ copy constructor.
func (d *PropertyDictionary) Clone() *PropertyDictionary {
	c := NewPropertyDictionary()
	c.Merge(d, 0)
	return c
}

func (d *PropertyDictionary) Clear() {
	d.properties = map[PropertyId]*Property{}
	d.customProperties = map[string]*Property{}
	d.varShorthands = map[ShorthandId]*Property{}
}

func (d *PropertyDictionary) SetSourceOfAllProperties(source *PropertySource) {
	props := d.properties
	for _, p := range props {
		p.Source = source
	}
	customs := d.customProperties
	for _, p := range customs {
		p.Source = source
	}
	shorthands := d.varShorthands
	for _, p := range shorthands {
		p.Source = source
	}
}

func (d *PropertyDictionary) setPropertySpecificity(id PropertyId, property Property, specificity int) {
	if cur, ok := d.properties[id]; ok && cur.Specificity > specificity {
		return
	}
	property.Specificity = specificity
	d.properties[id] = heapProperty(property)
}

func (d *PropertyDictionary) setCustomPropertySpecificity(name string, property Property, specificity int) {
	if cur, ok := d.customProperties[name]; ok && cur.Specificity > specificity {
		return
	}
	property.Specificity = specificity
	d.customProperties[name] = heapProperty(property)
}

func (d *PropertyDictionary) setVarShorthandSpecificity(id ShorthandId, property Property, specificity int) {
	if cur, ok := d.varShorthands[id]; ok && cur.Specificity > specificity {
		return
	}
	property.Specificity = specificity
	d.varShorthands[id] = heapProperty(property)
}
