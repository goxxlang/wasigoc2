// Port of RmlUi Source/Core/Tween.cpp, Include/RmlUi/Core/Tween.h,
// Animation.h, and Source/Core/PropertyParserAnimation.cpp.
package rmlui

// TweenType is Tween::Type.
type TweenType = int

const (
	TweenNone TweenType = iota
	TweenBack
	TweenBounce
	TweenCircular
	TweenCubic
	TweenElastic
	TweenExponential
	TweenLinear
	TweenQuadratic
	TweenQuartic
	TweenQuintic
	TweenSine
	TweenCallback
	TweenCount
)

// TweenDirection is Tween::Direction.
type TweenDirection = int

const (
	TweenIn    TweenDirection = 1
	TweenOut   TweenDirection = 2
	TweenInOut TweenDirection = 3
)

// TweenCallbackFunc is Tween::CallbackFnc.
type TweenCallbackFunc interface {
	Tween(t float32) float32
}

// Tween is Rml::Tween.
type Tween struct {
	typeIn   TweenType
	typeOut  TweenType
	callback TweenCallbackFunc
}

// NewTween is Tween(type, direction). The zero Tween{} is (None, None);
// use DefaultTween for the C++ default constructor.
func NewTween(t TweenType, direction TweenDirection) Tween {
	tw := Tween{}
	if direction&TweenIn != 0 {
		tw.typeIn = t
	}
	if direction&TweenOut != 0 {
		tw.typeOut = t
	}
	return tw
}

// DefaultTween is Tween() = Tween(Linear, Out).
func DefaultTween() Tween { return NewTween(TweenLinear, TweenOut) }

func NewTweenInOut(typeIn TweenType, typeOut TweenType) Tween {
	return Tween{typeIn: typeIn, typeOut: typeOut}
}

func NewTweenCallback(cb TweenCallbackFunc, direction TweenDirection) Tween {
	tw := Tween{callback: cb}
	if direction&TweenIn != 0 {
		tw.typeIn = TweenCallback
	}
	if direction&TweenOut != 0 {
		tw.typeOut = TweenCallback
	}
	return tw
}

func tweenSquare(t float32) float32 { return t * t }

func tweenFunction(typ TweenType, t float32, cb TweenCallbackFunc) float32 {
	switch typ {
	case TweenBack:
		return t * t * (2.70158*t - 1.70158)
	case TweenBounce:
		if t > 1.0-1.0/2.75 {
			return 1.0 - 7.5625*tweenSquare(1.0-t)
		} else if t > 1.0-2.0/2.75 {
			return 1.0 - (7.5625*tweenSquare(1.0-t-1.5/2.75) + 0.75)
		} else if t > 1.0-2.5/2.75 {
			return 1.0 - (7.5625*tweenSquare(1.0-t-2.25/2.75) + 0.9375)
		}
		return 1.0 - (7.5625*tweenSquare(1.0-t-2.625/2.75) + 0.984375)
	case TweenCircular:
		return 1.0 - MathSquareRoot(1.0-t*t)
	case TweenCubic:
		return t * t * t
	case TweenElastic:
		if t == 0 || t == 1 {
			return t
		}
		return -MathExp(7.24*(t-1.0)) * MathSin((t-1.1)*2.0*Pi/0.4)
	case TweenExponential:
		if t == 0 || t == 1 {
			return t
		}
		return MathExp(7.24 * (t - 1.0))
	case TweenLinear:
		return t
	case TweenQuadratic:
		return t * t
	case TweenQuartic:
		return t * t * t * t
	case TweenQuintic:
		return t * t * t * t * t
	case TweenSine:
		return 1.0 - MathCos(t*Pi*0.5)
	case TweenCallback:
		if cb != nil {
			return cb.Tween(t)
		}
	}
	return t
}

func (tw Tween) in(t float32) float32  { return tweenFunction(tw.typeIn, t, tw.callback) }
func (tw Tween) out(t float32) float32 { return 1.0 - tweenFunction(tw.typeOut, 1.0-t, tw.callback) }
func (tw Tween) inOut(t float32) float32 {
	if t < 0.5 {
		return tweenFunction(tw.typeIn, 2.0*t, tw.callback) * 0.5
	}
	return 0.5 + tw.out(2.0*t-1.0)*0.5
}

// Evaluate is Tween::operator()(t).
func (tw Tween) Evaluate(t float32) float32 {
	if tw.typeIn != TweenNone && tw.typeOut == TweenNone {
		return tw.in(t)
	}
	if tw.typeIn == TweenNone && tw.typeOut != TweenNone {
		return tw.out(t)
	}
	if tw.typeIn != TweenNone && tw.typeOut != TweenNone {
		return tw.inOut(t)
	}
	return t
}

// Reverse is Tween::reverse.
func (tw *Tween) Reverse() { tw.typeIn, tw.typeOut = tw.typeOut, tw.typeIn }

// Equals is Tween::operator==.
func (tw Tween) Equals(o Tween) bool {
	return tw.typeIn == o.typeIn && tw.typeOut == o.typeOut && tw.callback == o.callback
}

var tweenTypeStr = []string{"none", "back", "bounce", "circular", "cubic", "elastic", "exponential", "linear", "quadratic", "quartic", "quintic", "sine", "callback"}

// ToString is Tween::to_string.
func (tw Tween) ToString() string {
	if tw.typeIn < len(tweenTypeStr) && tw.typeOut < len(tweenTypeStr) {
		if tw.typeIn == TweenNone && tw.typeOut == TweenNone {
			return "none"
		} else if tw.typeIn == tw.typeOut {
			return tweenTypeStr[tw.typeIn] + "-in-out"
		} else if tw.typeIn == TweenNone {
			return tweenTypeStr[tw.typeOut] + "-out"
		} else if tw.typeOut == TweenNone {
			return tweenTypeStr[tw.typeIn] + "-in"
		}
		return tweenTypeStr[tw.typeIn] + "-in-" + tweenTypeStr[tw.typeOut] + "-out"
	}
	return "unknown"
}

// Animation is Rml::Animation, data parsed from the 'animation' property.
type Animation struct {
	Duration      float32
	Tween         Tween
	Delay         float32
	Alternate     bool
	Paused        bool
	NumIterations int
	Name          string
}

func NewAnimation() Animation { return Animation{Tween: DefaultTween(), NumIterations: 1} }

func (a Animation) Equals(b Animation) bool {
	return a.Duration == b.Duration && a.Tween.Equals(b.Tween) && a.Delay == b.Delay && a.Alternate == b.Alternate &&
		a.Paused == b.Paused && a.NumIterations == b.NumIterations && a.Name == b.Name
}

// Transition is Rml::Transition.
type Transition struct {
	Id                      PropertyId
	Tween                   Tween
	Duration                float32
	Delay                   float32
	ReverseAdjustmentFactor float32
}

func (a Transition) Equals(b Transition) bool {
	return a.Id == b.Id && a.Tween.Equals(b.Tween) && a.Duration == b.Duration && a.Delay == b.Delay &&
		a.ReverseAdjustmentFactor == b.ReverseAdjustmentFactor
}

// TransitionList is Rml::TransitionList.
type TransitionList struct {
	None        bool
	All         bool
	Transitions []Transition
}

func (a *TransitionList) variantEquals(other VariantPayload) bool {
	b, ok := other.(*TransitionList)
	if !ok || a.None != b.None || a.All != b.All || len(a.Transitions) != len(b.Transitions) {
		return false
	}
	for i := 0; i < len(a.Transitions); i++ {
		if !a.Transitions[i].Equals(b.Transitions[i]) {
			return false
		}
	}
	return true
}

func (a *TransitionList) variantString() string {
	if a.None {
		return "none"
	}
	dest := ""
	for i := 0; i < len(a.Transitions); i++ {
		t := a.Transitions[i]
		dest = dest + GetPropertyName(t.Id) + " "
		dest = dest + t.Tween.ToString() + " "
		dest = dest + FormatFloat(t.Duration) + "s "
		if t.Delay > 0 {
			dest = dest + FormatFloat(t.Delay) + "s "
		}
		if t.ReverseAdjustmentFactor > 0 {
			dest = dest + FormatFloat(t.ReverseAdjustmentFactor) + " "
		}
		if len(dest) > 0 {
			dest = dest[:len(dest)-1]
		}
		if i != len(a.Transitions)-1 {
			dest = dest + ", "
		}
	}
	return dest
}

// AnimationList is Rml::AnimationList.
type AnimationList struct {
	List []Animation
}

func (a *AnimationList) variantEquals(other VariantPayload) bool {
	b, ok := other.(*AnimationList)
	if !ok || len(a.List) != len(b.List) {
		return false
	}
	for i := 0; i < len(a.List); i++ {
		if !a.List[i].Equals(b.List[i]) {
			return false
		}
	}
	return true
}

func (a *AnimationList) variantString() string {
	dest := ""
	for i := 0; i < len(a.List); i++ {
		an := a.List[i]
		dest = dest + FormatFloat(an.Duration) + "s "
		dest = dest + an.Tween.ToString() + " "
		if an.Delay > 0 {
			dest = dest + FormatFloat(an.Delay) + "s "
		}
		if an.Alternate {
			dest = dest + "alternate "
		}
		if an.Paused {
			dest = dest + "paused "
		}
		if an.NumIterations == -1 {
			dest = dest + "infinite "
		} else {
			dest = dest + FormatInt(an.NumIterations) + " "
		}
		dest = dest + an.Name
		if i != len(a.List)-1 {
			dest = dest + ", "
		}
	}
	return dest
}

// ---- PropertyParserAnimation ----

const (
	animationParser  = 0
	transitionParser = 1
)

type animKeywordType = int

const (
	animKwNone animKeywordType = iota
	animKwTween
	animKwAll
	animKwAlternate
	animKwInfinite
	animKwPaused
)

type animKeyword struct {
	typ   animKeywordType
	tween Tween
}

func (k animKeyword) validTransition() bool {
	return k.typ == animKwNone || k.typ == animKwTween || k.typ == animKwAll
}

func (k animKeyword) validAnimation() bool {
	return k.typ == animKwNone || k.typ == animKwTween || k.typ == animKwAlternate || k.typ == animKwInfinite || k.typ == animKwPaused
}

var animationKeywords = buildAnimationKeywords()

func buildAnimationKeywords() map[string]animKeyword {
	m := map[string]animKeyword{
		"none":      {typ: animKwNone},
		"all":       {typ: animKwAll},
		"alternate": {typ: animKwAlternate},
		"infinite":  {typ: animKwInfinite},
		"paused":    {typ: animKwPaused},
	}
	names := []string{"back", "bounce", "circular", "cubic", "elastic", "exponential", "linear", "quadratic", "quartic", "quintic", "sine"}
	types := []TweenType{TweenBack, TweenBounce, TweenCircular, TweenCubic, TweenElastic, TweenExponential, TweenLinear, TweenQuadratic, TweenQuartic, TweenQuintic, TweenSine}
	for i := 0; i < len(names); i++ {
		m[names[i]+"-in"] = animKeyword{typ: animKwTween, tween: NewTween(types[i], TweenIn)}
		m[names[i]+"-out"] = animKeyword{typ: animKwTween, tween: NewTween(types[i], TweenOut)}
		m[names[i]+"-in-out"] = animKeyword{typ: animKwTween, tween: NewTween(types[i], TweenInOut)}
	}
	return m
}

// PropertyParserAnimation is Rml::PropertyParserAnimation.
type PropertyParserAnimation struct {
	typ int
}

func NewPropertyParserAnimation(typ int) *PropertyParserAnimation {
	return &PropertyParserAnimation{typ: typ}
}

func (p *PropertyParserAnimation) ParseValue(property *Property, value string, parameters map[string]int) bool {
	list := StringExpand(value, ',', false)
	if p.typ == animationParser {
		return parseAnimationList(property, list)
	}
	return parseTransitionList(property, list)
}

// scanSeconds is sscanf("%fs%n"): matched reports a leading number;
// seconds reports that an 's' followed it.
func scanSeconds(argument string) (float32, bool, bool) {
	f, rest, ok := Strtof(argument)
	if !ok {
		return 0, false, false
	}
	return f, true, len(rest) > 0 && rest[0] == 's'
}

func parseAnimationList(property *Property, values []string) bool {
	list := &AnimationList{}
	for _, single := range values {
		animation := NewAnimation()
		arguments := StringExpand(single, ' ', false)
		durationFound := false
		delayFound := false
		numIterationsFound := false
		for _, argument := range arguments {
			if argument == "" {
				continue
			}
			kw, ok := animationKeywords[StringToLower(argument)]
			if ok && kw.validAnimation() {
				switch kw.typ {
				case animKwNone:
					if len(list.List) > 0 {
						return false
					}
					*property = PropertyOf(VariantPointer(VariantANIMATIONLIST, &AnimationList{}), UnitANIMATION)
					return true
				case animKwTween:
					animation.Tween = kw.tween
				case animKwAlternate:
					animation.Alternate = true
				case animKwInfinite:
					if numIterationsFound {
						return false
					}
					animation.NumIterations = -1
					numIterationsFound = true
				case animKwPaused:
					animation.Paused = true
				}
			} else {
				number, matched, seconds := scanSeconds(argument)
				if matched {
					if seconds {
						if !durationFound {
							durationFound = true
							animation.Duration = number
						} else if !delayFound {
							delayFound = true
							animation.Delay = number
						} else {
							return false
						}
					} else {
						if !numIterationsFound {
							animation.NumIterations = MathRoundToInteger(number)
							numIterationsFound = true
						} else {
							return false
						}
					}
				} else {
					animation.Name = argument
				}
			}
		}
		if animation.Name == "" || animation.Duration <= 0 || animation.NumIterations < -1 || animation.NumIterations == 0 {
			return false
		}
		list.List = append(list.List, animation)
	}
	property.Value = VariantPointer(VariantANIMATIONLIST, list)
	property.Unit = UnitANIMATION
	return true
}

func parseTransitionList(property *Property, values []string) bool {
	list := &TransitionList{}
	for _, single := range values {
		transition := Transition{Tween: DefaultTween()}
		targets := PropertyIdSet{}
		arguments := StringExpand(single, ' ', false)
		durationFound := false
		delayFound := false
		reverseFound := false
		for _, argument := range arguments {
			if argument == "" {
				continue
			}
			kw, ok := animationKeywords[argument]
			if ok && kw.validTransition() {
				if kw.typ == animKwNone {
					if len(list.Transitions) > 0 {
						return false
					}
					*property = PropertyOf(VariantPointer(VariantTRANSITIONLIST, &TransitionList{None: true}), UnitTRANSITION)
					return true
				} else if kw.typ == animKwAll {
					if len(list.Transitions) > 0 {
						return false
					}
					list.All = true
				} else if kw.typ == animKwTween {
					transition.Tween = kw.tween
				}
			} else {
				number, matched, seconds := scanSeconds(argument)
				if matched {
					if seconds {
						if !durationFound {
							durationFound = true
							transition.Duration = number
						} else if !delayFound {
							delayFound = true
							transition.Delay = number
						} else {
							return false
						}
					} else {
						if !reverseFound {
							reverseFound = true
							transition.ReverseAdjustmentFactor = number
						} else {
							return false
						}
					}
				} else {
					if sh := GetShorthandDefinitionByName(argument); sh != nil {
						targets.UnionWith(GetShorthandUnderlyingProperties(sh.Id))
					} else if def := GetPropertyDefinitionByName(argument); def != nil {
						targets.Insert(def.GetId())
					} else {
						return false
					}
				}
			}
		}
		if (list.All && !targets.Empty()) || (!list.All && targets.Empty()) || transition.Duration <= 0 ||
			transition.ReverseAdjustmentFactor < 0 || transition.ReverseAdjustmentFactor > 1 {
			return false
		}
		if list.All {
			transition.Id = PropertyIdInvalid
			list.Transitions = append(list.Transitions, transition)
		} else {
			rng1 := targets.Ids()
			for _, id := range rng1 {
				transition.Id = id
				list.Transitions = append(list.Transitions, transition)
			}
		}
	}
	property.Value = VariantPointer(VariantTRANSITIONLIST, list)
	property.Unit = UnitTRANSITION
	return true
}
