// Port of RmlUi Source/Core/ElementAnimation.cpp and ElementAnimation.h.
package rmlui

// AnimationKey is Rml::AnimationKey.
type AnimationKey struct {
	Time     float32 // local animation time
	Property Property
	Tween    Tween // tweening from the previous key; ignored for the first
}

// ElementAnimationOrigin is Rml::ElementAnimationOrigin.
type ElementAnimationOrigin = int

const (
	ElementAnimationOriginUser ElementAnimationOrigin = iota
	ElementAnimationOriginAnimation
	ElementAnimationOriginTransition
)

// ElementAnimation is Rml::ElementAnimation: the keys and progress of one
// animated property.
type ElementAnimation struct {
	propertyId              PropertyId
	duration                float32
	numIterations           int
	alternateDirection      bool
	keys                    []*AnimationKey
	lastUpdateWorldTime     float64
	timeSinceIterationStart float32
	currentIteration        int
	reverseDirection        bool
	animationComplete       bool
	origin                  ElementAnimationOrigin
}

func NewElementAnimation(id PropertyId, origin ElementAnimationOrigin, currentValue Property, element *Element, startWorldTime float64, duration float32, numIterations int, alternateDirection bool) *ElementAnimation {
	a := &ElementAnimation{propertyId: id, duration: duration, numIterations: numIterations, alternateDirection: alternateDirection, lastUpdateWorldTime: startWorldTime, origin: origin}
	if currentValue.Definition == nil {
		LogMessage(LogWarning, "Property in animation key did not have a definition (while adding key '"+currentValue.ToString()+"').")
	}
	a.internalAddKey(0, currentValue, element, DefaultTween())
	return a
}

func (a *ElementAnimation) GetPropertyId() PropertyId            { return a.propertyId }
func (a *ElementAnimation) GetDuration() float32                 { return a.duration }
func (a *ElementAnimation) IsComplete() bool                     { return a.animationComplete }
func (a *ElementAnimation) IsTransition() bool                   { return a.origin == ElementAnimationOriginTransition }
func (a *ElementAnimation) IsInitialized() bool                  { return len(a.keys) > 0 }
func (a *ElementAnimation) GetOrigin() ElementAnimationOrigin    { return a.origin }
func (a *ElementAnimation) GetInterpolationFactor() float32 {
	f, _, _ := a.interpolationFactorAndKeys()
	return f
}

// mixFloat is Mix(v0, v1, alpha).
func mixFloat(v0 float32, v1 float32, alpha float32) float32 { return v0*(1.0-alpha) + v1*alpha }

func colourToLinearSpace(c Colourb) Colourf {
	r := float32(c.Red) / 255.0
	g := float32(c.Green) / 255.0
	b := float32(c.Blue) / 255.0
	return Colourf{r * r, g * g, b * b, float32(c.Alpha) / 255.0}
}

func colourFromLinearSpace(c Colourf) Colourb {
	return Colourb{
		byte(MathClamp(MathSquareRoot(c.Red)*255.0, 0, 255)),
		byte(MathClamp(MathSquareRoot(c.Green)*255.0, 0, 255)),
		byte(MathClamp(MathSquareRoot(c.Blue)*255.0, 0, 255)),
		byte(MathClamp(c.Alpha*255.0, 0, 255)),
	}
}

func interpolateColour(c0 Colourb, c1 Colourb, alpha float32) Colourb {
	a := colourToLinearSpace(c0)
	b := colourToLinearSpace(c1)
	return colourFromLinearSpace(a.Mul(1.0 - alpha).Add(b.Mul(alpha)))
}

// combineAndDecompose merges all primitives into one DecomposedMatrix4.
func combineAndDecompose(t *Transform, e *Element) bool {
	m := Matrix4Identity()
	prims := t.GetPrimitives()
	for _, p := range prims {
		m = m.Mul(TransformResolve(p, e))
	}
	d, ok := TransformDecompose(m)
	if !ok {
		return false
	}
	t.ClearPrimitives()
	t.AddPrimitive(TransformPrimitive{Type: TransformDECOMPOSEDMATRIX4, Decomposed: d})
	return true
}

// effectDeclarationRef abstracts decorator and filter declarations.
type effectDeclarationRef struct {
	spec       *PropertySpecification
	instancerD DecoratorInstancer
	instancerF FilterInstancer
	typ        string
	properties *PropertyDictionary
	paintArea  BoxArea
	valid      bool
}

func interpolateEffectProperties(out *PropertyDictionary, d0 effectDeclarationRef, d1 effectDeclarationRef, alpha float32, element *Element) bool {
	if d0.valid && d1.valid {
		if d0.spec == nil || d0.spec != d1.spec || d0.typ != d1.typ || d0.properties.GetNumProperties() != d1.properties.GetNumProperties() || d0.paintArea != d1.paintArea {
			return false
		}
		props0 := d0.properties.GetProperties()
		props1 := d1.properties.GetProperties()
		ids := sortedPropertyIds(props0)
		for _, id := range ids {
			prop0 := props0[id]
			prop1, ok := props1[id]
			if !ok {
				return false
			}
			p := interpolateProperties(*prop0, *prop1, alpha, element, prop0.Definition)
			p.Definition = prop0.Definition
			out.SetProperty(id, p)
		}
		return true
	} else if d0.valid || d1.valid {
		filled := d0
		if !d0.valid {
			filled = d1
		}
		props := filled.properties.GetProperties()
		ids := sortedPropertyIds(props)
		for _, id := range ids {
			def := filled.spec.GetProperty(id)
			if def == nil {
				return false
			}
			pFilled := *props[id]
			pDefault := *def.GetDefaultValue()
			p0 := pDefault
			p1 := pDefault
			if d0.valid {
				p0 = pFilled
			}
			if d1.valid {
				p1 = pFilled
			}
			p := interpolateProperties(p0, p1, alpha, element, pFilled.Definition)
			p.Definition = pFilled.Definition
			out.SetProperty(id, p)
		}
		return true
	}
	return false
}

func interpolateNumericValue(v0 NumericValue, v1 NumericValue, alpha float32, element *Element, definition *PropertyDefinition) NumericValue {
	if v0.Unit == v1.Unit {
		return NumericValue{mixFloat(v0.Number, v1.Number, alpha), v0.Unit}
	}
	if AnyUnit(v0.Unit&UnitNUMBER_LENGTH_PERCENT) && AnyUnit(v1.Unit&UnitNUMBER_LENGTH_PERCENT) && definition != nil {
		f0 := element.GetStyle().ResolveRelativeLength(v0, definition.GetRelativeTarget())
		f1 := element.GetStyle().ResolveRelativeLength(v1, definition.GetRelativeTarget())
		return NumericValue{mixFloat(f0, f1, alpha), UnitPX}
	}
	if AnyUnit(v0.Unit&UnitLENGTH) && AnyUnit(v1.Unit&UnitLENGTH) {
		// Upstream RmlUi 6.3 resolves v0 twice here; kept for parity.
		f0 := element.ResolveLength(v0)
		f1 := element.ResolveLength(v0)
		return NumericValue{mixFloat(f0, f1, alpha), UnitPX}
	}
	if AnyUnit(v0.Unit&UnitANGLE) && AnyUnit(v1.Unit&UnitANGLE) {
		return NumericValue{mixFloat(ComputeAngle(v0), ComputeAngle(v1), alpha), UnitRAD}
	}
	if alpha < 0.5 {
		return v0
	}
	return v1
}

func decoratorDeclarationRef(list []DecoratorDeclaration, i int, element *Element) effectDeclarationRef {
	if i >= len(list) {
		return effectDeclarationRef{}
	}
	d := list[i]
	if d.Instancer != nil {
		return effectDeclarationRef{spec: d.Instancer.GetPropertySpecification(), instancerD: d.Instancer, typ: d.Type, properties: d.Properties, paintArea: d.PaintArea, valid: true}
	}
	sheet := element.GetStyleSheet()
	if sheet == nil {
		return effectDeclarationRef{}
	}
	named := sheet.GetNamedDecorator(d.Type)
	if named == nil {
		LogMessage(LogWarning, "Could not find a named @decorator '"+d.Type+"'.")
		return effectDeclarationRef{}
	}
	return effectDeclarationRef{spec: named.Instancer.GetPropertySpecification(), instancerD: named.Instancer, typ: named.Type, properties: named.Properties, paintArea: BoxAreaAuto, valid: true}
}

func filterDeclarationRef(list []FilterDeclaration, i int) effectDeclarationRef {
	if i >= len(list) {
		return effectDeclarationRef{}
	}
	d := list[i]
	return effectDeclarationRef{spec: d.Instancer.GetPropertySpecification(), instancerF: d.Instancer, typ: d.Type, properties: d.Properties, paintArea: BoxAreaAuto, valid: true}
}

func interpolateProperties(p0 Property, p1 Property, alpha float32, element *Element, definition *PropertyDefinition) Property {
	discrete := p1
	if alpha < 0.5 {
		discrete = p0
	}
	if AnyUnit(p0.Unit&UnitNUMERIC) && AnyUnit(p1.Unit&UnitNUMERIC) {
		v := interpolateNumericValue(p0.GetNumericValue(), p1.GetNumericValue(), alpha, element, definition)
		return PropertyFloat(v.Number, v.Unit)
	}
	if p0.Unit == UnitKEYWORD && p1.Unit == UnitKEYWORD {
		if definition != nil && definition.GetId() == PropertyIdVisibility {
			if p0.Value.GetInt() == VisibilityVisible {
				if alpha < 1 {
					return p0
				}
				return p1
			} else if p1.Value.GetInt() == VisibilityVisible {
				if alpha <= 0 {
					return p0
				}
				return p1
			}
		}
		if definition != nil && definition.GetId() == PropertyIdDisplay {
			if p0.Value.GetInt() == DisplayNone {
				if alpha <= 0 {
					return p0
				}
				return p1
			} else if p1.Value.GetInt() == DisplayNone {
				if alpha < 1 {
					return p0
				}
				return p1
			}
		}
		return discrete
	}
	if p0.Unit == UnitCOLOUR && p1.Unit == UnitCOLOUR {
		return PropertyOf(VariantColourb(interpolateColour(p0.Value.GetColourb(), p1.Value.GetColourb(), alpha)), UnitCOLOUR)
	}
	if p0.Unit == UnitTRANSFORM && p1.Unit == UnitTRANSFORM {
		t0, _ := p0.Value.Pointer().(*Transform)
		t1, _ := p1.Value.Pointer().(*Transform)
		if t0 == nil || t1 == nil || t0.GetNumPrimitives() != t1.GetNumPrimitives() {
			return PropertyOf(VariantPointer(VariantTRANSFORMPTR, t0), UnitTRANSFORM)
		}
		t := NewTransform(nil)
		for i := 0; i < t0.GetNumPrimitives(); i++ {
			p := t0.GetPrimitive(i)
			if !TransformInterpolateWith(&p, t1.GetPrimitive(i), alpha) {
				return PropertyOf(VariantPointer(VariantTRANSFORMPTR, t0), UnitTRANSFORM)
			}
			t.AddPrimitive(p)
		}
		return PropertyOf(VariantPointer(VariantTRANSFORMPTR, t), UnitTRANSFORM)
	}
	if p0.Unit == UnitDECORATOR && p1.Unit == UnitDECORATOR {
		ptr0, ok0 := p0.Value.Pointer().(*DecoratorDeclarationList)
		ptr1, ok1 := p1.Value.Pointer().(*DecoratorDeclarationList)
		if !ok0 || !ok1 || ptr0 == nil || ptr1 == nil {
			return discrete
		}
		p0Bigger := len(ptr0.List) > len(ptr1.List)
		bigLen := len(ptr1.List)
		if p0Bigger {
			bigLen = len(ptr0.List)
		}
		result := &DecoratorDeclarationList{}
		for i := 0; i < bigLen; i++ {
			d0 := decoratorDeclarationRef(ptr0.List, i, element)
			d1 := decoratorDeclarationRef(ptr1.List, i, element)
			d := d1
			if p0Bigger {
				d = d0
			}
			decl := DecoratorDeclaration{Type: d.typ, Instancer: d.instancerD, Properties: NewPropertyDictionary(), PaintArea: d.paintArea}
			if !interpolateEffectProperties(decl.Properties, d0, d1, alpha, element) {
				return discrete
			}
			result.List = append(result.List, decl)
		}
		return PropertyOf(VariantPointer(VariantDECORATORSPTR, result), UnitDECORATOR)
	}
	if p0.Unit == UnitFILTER && p1.Unit == UnitFILTER {
		ptr0, ok0 := p0.Value.Pointer().(*FilterDeclarationList)
		ptr1, ok1 := p1.Value.Pointer().(*FilterDeclarationList)
		if !ok0 || !ok1 || ptr0 == nil || ptr1 == nil {
			return discrete
		}
		p0Bigger := len(ptr0.List) > len(ptr1.List)
		bigLen := len(ptr1.List)
		if p0Bigger {
			bigLen = len(ptr0.List)
		}
		result := &FilterDeclarationList{}
		for i := 0; i < bigLen; i++ {
			d0 := filterDeclarationRef(ptr0.List, i)
			d1 := filterDeclarationRef(ptr1.List, i)
			d := d1
			if p0Bigger {
				d = d0
			}
			decl := FilterDeclaration{Type: d.typ, Instancer: d.instancerF, Properties: NewPropertyDictionary()}
			if !interpolateEffectProperties(decl.Properties, d0, d1, alpha, element) {
				return discrete
			}
			result.List = append(result.List, decl)
		}
		return PropertyOf(VariantPointer(VariantFILTERSPTR, result), UnitFILTER)
	}
	if p0.Unit == UnitCOLORSTOPLIST && p1.Unit == UnitCOLORSTOPLIST {
		c0, ok0 := p0.Value.Pointer().(*ColorStopList)
		c1, ok1 := p1.Value.Pointer().(*ColorStopList)
		if !ok0 || !ok1 || len(c0.Stops) != len(c1.Stops) {
			return discrete
		}
		result := &ColorStopList{}
		for i := 0; i < len(c0.Stops); i++ {
			stop := ColorStop{}
			stop.Color = interpolateColour(c0.Stops[i].Color.ToNonPremultiplied(), c1.Stops[i].Color.ToNonPremultiplied(), alpha).ToPremultiplied()
			stop.Position = interpolateNumericValue(c0.Stops[i].Position, c1.Stops[i].Position, alpha, element, nil)
			result.Stops = append(result.Stops, stop)
		}
		return PropertyOf(VariantPointer(VariantCOLORSTOPLIST, result), UnitCOLORSTOPLIST)
	}
	return discrete
}

const (
	prepareUnchanged      = 0
	prepareChangedT0      = 1
	prepareChangedT1      = 2
	prepareChangedT0andT1 = 3
	prepareInvalid        = 4
)

func prepareTransformPair(t0 *Transform, t1 *Transform, element *Element) int {
	prims0 := t0.primitives
	prims1 := t1.primitives
	if len(prims0) == len(prims1) {
		result := prepareUnchanged
		same := true
		for i := 0; i < len(prims0); i++ {
			type0 := prims0[i].Type
			type1 := prims1[i].Type
			if TransformTryConvertToMatchingGenericType(&prims0[i], &prims1[i]) {
				if prims0[i].Type != type0 {
					result = result | prepareChangedT0
				}
				if prims1[i].Type != type1 {
					result = result | prepareChangedT1
				}
			} else {
				same = false
				break
			}
		}
		if same {
			return result
		}
	}
	if len(t0.primitives) != len(t1.primitives) {
		prims0Smallest := len(t0.primitives) < len(t1.primitives)
		small := t1.primitives
		big := t0.primitives
		if prims0Smallest {
			small = t0.primitives
			big = t1.primitives
		}
		matching := []int{}
		iBig := 0
		matchSuccess := true
		changedBig := false
		for iSmall := 0; iSmall < len(small); iSmall++ {
			matchSuccess = false
			for iBig < len(big) {
				bigType := big[iBig].Type
				if TransformTryConvertToMatchingGenericType(&small[iSmall], &big[iBig]) {
					if big[iBig].Type != bigType {
						changedBig = true
					}
					matching = append(matching, iBig)
					matchSuccess = true
					iBig++
					break
				}
				iBig++
			}
			if !matchSuccess {
				break
			}
		}
		if matchSuccess {
			matching = append(matching, len(big))
			newSmall := []TransformPrimitive{}
			i0 := 0
			smallIndex := 0
			for _, matchIndex := range matching {
				for i := i0; i < matchIndex; i++ {
					p := big[i]
					TransformSetIdentity(&p)
					newSmall = append(newSmall, p)
				}
				if matchIndex < len(big) && smallIndex < len(small) {
					newSmall = append(newSmall, small[smallIndex])
					smallIndex++
				}
				i0 = matchIndex + 1
			}
			if prims0Smallest {
				t0.primitives = newSmall
			} else {
				t1.primitives = newSmall
			}
			if changedBig {
				return prepareChangedT0andT1
			}
			if prims0Smallest {
				return prepareChangedT0
			}
			return prepareChangedT1
		}
	}
	if !combineAndDecompose(t0, element) {
		return prepareInvalid
	}
	if !combineAndDecompose(t1, element) {
		return prepareInvalid
	}
	return prepareChangedT0andT1
}

func prepareTransforms(keys []*AnimationKey, element *Element, startIndex int) bool {
	result := true
	for i := startIndex; i < len(keys); i++ {
		property := &keys[i].Property
		t, _ := property.Value.Pointer().(*Transform)
		if t == nil {
			t = NewTransform(nil)
			property.Value = VariantPointer(VariantTRANSFORMPTR, t)
		}
		mustDecompose := false
		for j := 0; j < len(t.primitives); j++ {
			if !TransformPrepareForInterpolation(&t.primitives[j], element) {
				mustDecompose = true
				break
			}
		}
		if mustDecompose {
			result = combineAndDecompose(t, element) && result
		}
	}
	if !result {
		return false
	}
	if len(keys) < 2 || startIndex < 1 {
		return true
	}
	n := len(keys)
	countIterations := -1
	maxIterations := 3 * n
	dirty := make([]int, n+1)
	dirty[startIndex] = 1
	i := startIndex
	for i < n && countIterations < maxIterations {
		countIterations++
		if dirty[i] == 0 {
			i++
			continue
		}
		prop0 := keys[i-1].Property
		prop1 := keys[i].Property
		if prop0.Unit != UnitTRANSFORM || prop1.Unit != UnitTRANSFORM {
			return false
		}
		t0, _ := prop0.Value.Pointer().(*Transform)
		t1, _ := prop1.Value.Pointer().(*Transform)
		r := prepareTransformPair(t0, t1, element)
		if r == prepareInvalid {
			return false
		}
		changedT0 := r&prepareChangedT0 != 0
		changedT1 := r&prepareChangedT1 != 0
		dirty[i] = 0
		if changedT0 {
			dirty[i-1] = 1
		}
		if changedT1 {
			dirty[i+1] = 1
		}
		if changedT0 && i > 1 {
			i--
		} else {
			i++
		}
	}
	return countIterations < maxIterations
}

func (a *ElementAnimation) internalAddKey(time float32, in Property, element *Element, tween Tween) bool {
	valid := UnitNUMBER_LENGTH_PERCENT | UnitANGLE | UnitCOLOUR | UnitTRANSFORM | UnitKEYWORD | UnitDECORATOR | UnitFILTER
	if !AnyUnit(in.Unit & valid) {
		kind := "Property value does not"
		if in.Unit == UnitBOXSHADOWLIST {
			kind = "Box shadows do not"
		}
		LogMessage(LogWarning, kind+" support animations or transitions. Value: "+in.ToString())
		return false
	}
	key := &AnimationKey{Time: time, Property: in, Tween: tween}
	// Keys own their transforms: preparing them rewrites primitives in place.
	if in.Unit == UnitTRANSFORM {
		if t, ok := in.Value.Pointer().(*Transform); ok && t != nil {
			key.Property.Value = VariantPointer(VariantTRANSFORMPTR, NewTransform(append([]TransformPrimitive{}, t.primitives...)))
		}
	}
	a.keys = append(a.keys, key)
	result := true
	if key.Property.Unit == UnitTRANSFORM {
		result = prepareTransforms(a.keys, element, len(a.keys)-1)
	} else if key.Property.Unit == UnitDECORATOR {
		if key.Property.Value.Pointer() == nil {
			key.Property.Value = VariantPointer(VariantDECORATORSPTR, &DecoratorDeclarationList{})
		}
	} else if key.Property.Unit == UnitFILTER {
		if key.Property.Value.Pointer() == nil {
			key.Property.Value = VariantPointer(VariantFILTERSPTR, &FilterDeclarationList{})
		}
	}
	if !result {
		LogMessage(LogWarning, "Could not add animation key with property '"+in.ToString()+"'.")
		a.keys = a.keys[:len(a.keys)-1]
	}
	return result
}

// AddKey adds a key at targetTime (optionally extending the duration).
func (a *ElementAnimation) AddKey(targetTime float32, in Property, element *Element, tween Tween, extendDuration bool) bool {
	if !a.IsInitialized() {
		LogMessage(LogWarning, "Element animation was not initialized properly, can't add key.")
		return false
	}
	if !a.internalAddKey(targetTime, in, element, tween) {
		return false
	}
	if extendDuration {
		a.duration = targetTime
	}
	return true
}

func (a *ElementAnimation) interpolationFactorAndKeys() (float32, int, int) {
	t := a.timeSinceIterationStart
	if a.reverseDirection {
		t = a.duration - t
	}
	key1 := -1
	for i := 0; i < len(a.keys); i++ {
		if a.keys[i].Time >= t {
			key1 = i
			break
		}
	}
	if key1 < 0 {
		key1 = len(a.keys) - 1
	}
	key0 := 0
	if key1 != 0 {
		key0 = key1 - 1
	}
	var alpha float32 = 0
	t0 := a.keys[key0].Time
	t1 := a.keys[key1].Time
	if t1-t0 > 1e-3 {
		alpha = (t - t0) / (t1 - t0)
	}
	alpha = MathClamp(alpha, 0, 1)
	alpha = a.keys[key1].Tween.Evaluate(alpha)
	return alpha, key0, key1
}

// UpdateAndGetProperty advances to worldTime and returns the interpolated
// value (Unit UNKNOWN when nothing changes).
func (a *ElementAnimation) UpdateAndGetProperty(worldTime float64, element *Element) Property {
	dt := float32(worldTime - a.lastUpdateWorldTime)
	if len(a.keys) < 2 || a.animationComplete || dt <= 0 {
		return NewProperty()
	}
	dt = MathMin(dt, 0.1)
	a.lastUpdateWorldTime = worldTime
	a.timeSinceIterationStart += dt
	if a.timeSinceIterationStart >= a.duration {
		a.currentIteration++
		if a.numIterations == -1 || (a.currentIteration >= 0 && a.currentIteration < a.numIterations) {
			a.timeSinceIterationStart -= a.duration
			if a.alternateDirection {
				a.reverseDirection = !a.reverseDirection
			}
		} else {
			a.animationComplete = true
			a.timeSinceIterationStart = a.duration
		}
	}
	alpha, key0, key1 := a.interpolationFactorAndKeys()
	return interpolateProperties(a.keys[key0].Property, a.keys[key1].Property, alpha, element, a.keys[0].Property.Definition)
}
