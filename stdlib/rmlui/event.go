// Port of RmlUi Source/Core/Event.cpp, EventSpecification.cpp, and
// Include/RmlUi/Core/Event.h, EventListener.h.
package rmlui

// EventPhase is Rml::EventPhase.
type EventPhase = int

const (
	EventPhaseNone    EventPhase = 0
	EventPhaseCapture EventPhase = 1
	EventPhaseTarget  EventPhase = 2
	EventPhaseBubble  EventPhase = 4
)

// DefaultActionPhase is Rml::DefaultActionPhase.
type DefaultActionPhase = int

const (
	DefaultActionPhaseNone            DefaultActionPhase = 0
	DefaultActionPhaseTarget          DefaultActionPhase = EventPhaseTarget
	DefaultActionPhaseTargetAndBubble DefaultActionPhase = EventPhaseTarget | EventPhaseBubble
)

// EventListener is Rml::EventListener.
type EventListener interface {
	ProcessEvent(event *Event)
	OnAttach(element *Element)
	OnDetach(element *Element)
}

// Event is Rml::Event.
type Event struct {
	parameters           map[string]Variant
	targetElement        *Element
	currentElement       *Element
	eventType            string
	id                   EventId
	interruptible        bool
	interrupted          bool
	interruptedImmediate bool
	hasMousePosition     bool
	mouseScreenPosition  Vector2f
	phase                EventPhase
	instancer            EventInstancer
}

func NewEvent(target *Element, id EventId, eventType string, parameters map[string]Variant, interruptible bool) *Event {
	ev := &Event{parameters: map[string]Variant{}, targetElement: target, eventType: eventType, id: id, interruptible: interruptible}
	for k, v := range parameters {
		ev.parameters[k] = v
	}
	mx, okx := ev.parameters["mouse_x"]
	my, oky := ev.parameters["mouse_y"]
	if okx && oky {
		ev.hasMousePosition = true
		ev.mouseScreenPosition.X = mx.GetFloat()
		ev.mouseScreenPosition.Y = my.GetFloat()
	}
	return ev
}

func (ev *Event) SetCurrentElement(element *Element) {
	ev.currentElement = element
	if ev.hasMousePosition {
		ev.projectMouse(element)
	}
}

func (ev *Event) GetCurrentElement() *Element { return ev.currentElement }
func (ev *Event) GetTargetElement() *Element  { return ev.targetElement }
func (ev *Event) GetType() string             { return ev.eventType }
func (ev *Event) GetId() EventId              { return ev.id }
func (ev *Event) SetPhase(phase EventPhase)   { ev.phase = phase }
func (ev *Event) GetPhase() EventPhase        { return ev.phase }
func (ev *Event) IsPropagating() bool         { return !ev.interrupted }
func (ev *Event) IsImmediatePropagating() bool { return !ev.interruptedImmediate }
func (ev *Event) IsInterruptible() bool       { return ev.interruptible }

// Is is Event::operator==(type).
func (ev *Event) Is(eventType string) bool { return ev.eventType == eventType }

func (ev *Event) StopPropagation() {
	if ev.interruptible {
		ev.interrupted = true
	}
}

func (ev *Event) StopImmediatePropagation() {
	if ev.interruptible {
		ev.interruptedImmediate = true
		ev.interrupted = true
	}
}

func (ev *Event) GetParameters() map[string]Variant { return ev.parameters }

// GetParameter returns the parameter, or def when missing.
func (ev *Event) GetParameter(key string, def Variant) Variant {
	if v, ok := ev.parameters[key]; ok {
		return v
	}
	return def
}

func (ev *Event) GetParameterFloat(key string, def float32) float32 {
	if v, ok := ev.parameters[key]; ok {
		if f, ok2 := v.GetFloatOk(); ok2 {
			return f
		}
	}
	return def
}

func (ev *Event) GetParameterInt(key string, def int) int {
	if v, ok := ev.parameters[key]; ok {
		if n, ok2 := v.GetIntOk(); ok2 {
			return n
		}
	}
	return def
}

func (ev *Event) GetParameterBool(key string, def bool) bool {
	if v, ok := ev.parameters[key]; ok {
		if b, ok2 := v.GetBoolOk(); ok2 {
			return b
		}
	}
	return def
}

func (ev *Event) GetParameterString(key string, def string) string {
	if v, ok := ev.parameters[key]; ok {
		if s, ok2 := v.GetStringOk(); ok2 {
			return s
		}
	}
	return def
}

// SetParameter sets a parameter (used by default actions and data events).
func (ev *Event) SetParameter(key string, value Variant) { ev.parameters[key] = value }

func (ev *Event) GetUnprojectedMouseScreenPos() Vector2f { return ev.mouseScreenPosition }

func (ev *Event) projectMouse(element *Element) {
	if element == nil {
		ev.parameters["mouse_x"] = VariantFloat(ev.mouseScreenPosition.X)
		ev.parameters["mouse_y"] = VariantFloat(ev.mouseScreenPosition.Y)
		return
	}
	if element.GetTransformState() != nil {
		_, okx := ev.parameters["mouse_x"]
		_, oky := ev.parameters["mouse_y"]
		if !okx || !oky {
			return
		}
		projected, ok := element.Project(ev.mouseScreenPosition)
		if ok {
			ev.parameters["mouse_x"] = VariantFloat(projected.X)
			ev.parameters["mouse_y"] = VariantFloat(projected.Y)
		} else {
			ev.StopPropagation()
		}
	}
}

// EventSpecification is Rml::EventSpecification.
type EventSpecification struct {
	Id                 EventId
	Type               string
	Interruptible      bool
	Bubbles            bool
	DefaultActionPhase DefaultActionPhase
}

var eventSpecifications []*EventSpecification
var eventTypeLookup map[string]EventId

// EventSpecificationInitialize is EventSpecificationInterface::Initialize.
func EventSpecificationInitialize() {
	eventSpecifications = []*EventSpecification{}
	eventTypeLookup = map[string]EventId{}
	add := func(id EventId, t string, interruptible bool, bubbles bool, phase DefaultActionPhase) {
		eventSpecifications = append(eventSpecifications, &EventSpecification{id, t, interruptible, bubbles, phase})
		eventTypeLookup[t] = id
	}
	add(EventIdInvalid, "invalid", false, false, DefaultActionPhaseNone)
	add(EventIdMousedown, "mousedown", true, true, DefaultActionPhaseTargetAndBubble)
	add(EventIdMousescroll, "mousescroll", true, true, DefaultActionPhaseNone)
	add(EventIdMouseover, "mouseover", true, true, DefaultActionPhaseTarget)
	add(EventIdMouseout, "mouseout", true, true, DefaultActionPhaseTarget)
	add(EventIdFocus, "focus", false, false, DefaultActionPhaseTarget)
	add(EventIdBlur, "blur", false, false, DefaultActionPhaseTarget)
	add(EventIdKeydown, "keydown", true, true, DefaultActionPhaseTargetAndBubble)
	add(EventIdKeyup, "keyup", true, true, DefaultActionPhaseTargetAndBubble)
	add(EventIdTextinput, "textinput", true, true, DefaultActionPhaseTargetAndBubble)
	add(EventIdMouseup, "mouseup", true, true, DefaultActionPhaseTargetAndBubble)
	add(EventIdClick, "click", true, true, DefaultActionPhaseTargetAndBubble)
	add(EventIdDblclick, "dblclick", true, true, DefaultActionPhaseTargetAndBubble)
	add(EventIdLoad, "load", false, false, DefaultActionPhaseNone)
	add(EventIdUnload, "unload", false, false, DefaultActionPhaseNone)
	add(EventIdShow, "show", false, false, DefaultActionPhaseNone)
	add(EventIdHide, "hide", false, false, DefaultActionPhaseNone)
	add(EventIdMousemove, "mousemove", true, true, DefaultActionPhaseNone)
	add(EventIdDragmove, "dragmove", true, true, DefaultActionPhaseNone)
	add(EventIdDrag, "drag", false, true, DefaultActionPhaseTarget)
	add(EventIdDragstart, "dragstart", false, true, DefaultActionPhaseTarget)
	add(EventIdDragover, "dragover", true, true, DefaultActionPhaseNone)
	add(EventIdDragdrop, "dragdrop", true, true, DefaultActionPhaseNone)
	add(EventIdDragout, "dragout", true, true, DefaultActionPhaseNone)
	add(EventIdDragend, "dragend", true, true, DefaultActionPhaseNone)
	add(EventIdHandledrag, "handledrag", false, true, DefaultActionPhaseNone)
	add(EventIdResize, "resize", false, false, DefaultActionPhaseNone)
	add(EventIdScroll, "scroll", false, true, DefaultActionPhaseNone)
	add(EventIdAnimationend, "animationend", false, true, DefaultActionPhaseNone)
	add(EventIdTransitionend, "transitionend", false, true, DefaultActionPhaseNone)
	add(EventIdChange, "change", false, true, DefaultActionPhaseNone)
	add(EventIdSubmit, "submit", true, true, DefaultActionPhaseNone)
	add(EventIdTabchange, "tabchange", false, true, DefaultActionPhaseNone)
}

func EventSpecificationShutdown() {
	eventSpecifications = nil
	eventTypeLookup = nil
}

// EventSpecificationGet returns the specification for id (invalid if unknown).
func EventSpecificationGet(id EventId) *EventSpecification {
	if id >= 0 && id < len(eventSpecifications) {
		return eventSpecifications[id]
	}
	return eventSpecifications[0]
}

func eventSpecificationGetOrInsertFull(eventType string, interruptible bool, bubbles bool, phase DefaultActionPhase) *EventSpecification {
	if id, ok := eventTypeLookup[eventType]; ok {
		return EventSpecificationGet(id)
	}
	newId := len(eventSpecifications)
	if newId >= EventIdMaxNumIds {
		LogMessage(LogError, "Error while registering event type '"+eventType+"': Maximum number of allowed events exceeded.")
		return eventSpecifications[0]
	}
	spec := &EventSpecification{newId, eventType, interruptible, bubbles, phase}
	eventSpecifications = append(eventSpecifications, spec)
	eventTypeLookup[eventType] = newId
	return spec
}

// EventSpecificationGetOrInsert registers unknown types as interruptible,
// bubbling, with no default action.
func EventSpecificationGetOrInsert(eventType string) *EventSpecification {
	return eventSpecificationGetOrInsertFull(eventType, true, true, DefaultActionPhaseNone)
}

func EventSpecificationGetIdOrInsert(eventType string) EventId {
	if id, ok := eventTypeLookup[eventType]; ok {
		return id
	}
	return EventSpecificationGetOrInsert(eventType).Id
}

// RegisterEventType is Rml::RegisterEventType (InsertOrReplaceCustom).
func RegisterEventType(eventType string, interruptible bool, bubbles bool, phase DefaultActionPhase) EventId {
	sizeBefore := len(eventSpecifications)
	spec := eventSpecificationGetOrInsertFull(eventType, interruptible, bubbles, phase)
	if sizeBefore == len(eventSpecifications) && spec.Id >= EventIdFirstCustomId {
		spec.Interruptible = interruptible
		spec.Bubbles = bubbles
		spec.DefaultActionPhase = phase
	}
	return spec.Id
}
