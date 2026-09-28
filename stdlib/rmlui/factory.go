// Port of RmlUi Source/Core/Factory.cpp and Include/RmlUi/Core/Factory.h,
// ElementInstancer.h, EventInstancer.h, EventListenerInstancer.h.
package rmlui

// ElementInstancer is Rml::ElementInstancer.
type ElementInstancer interface {
	InstanceElement(parent *Element, tag string, attributes map[string]Variant) *Element
}

// EventInstancer is Rml::EventInstancer.
type EventInstancer interface {
	InstanceEvent(target *Element, id EventId, eventType string, parameters map[string]Variant, interruptible bool) *Event
	ReleaseEvent(event *Event)
}

// EventListenerInstancer is Rml::EventListenerInstancer.
type EventListenerInstancer interface {
	InstanceEventListener(value string, element *Element) EventListener
}

type defaultElementInstancer struct{}

func (d defaultElementInstancer) InstanceElement(parent *Element, tag string, attributes map[string]Variant) *Element {
	return NewElement(tag)
}

type textElementInstancer struct{}

func (t textElementInstancer) InstanceElement(parent *Element, tag string, attributes map[string]Variant) *Element {
	return newElementText("").GetElement()
}

type defaultEventInstancer struct{}

func (d defaultEventInstancer) InstanceEvent(target *Element, id EventId, eventType string, parameters map[string]Variant, interruptible bool) *Event {
	return NewEvent(target, id, eventType, parameters, interruptible)
}

func (d defaultEventInstancer) ReleaseEvent(event *Event) {}

type factoryState struct {
	elements        map[string]ElementInstancer
	decorators      map[string]DecoratorInstancer
	filters         map[string]FilterInstancer
	fontEffects     map[string]FontEffectInstancer
	eventInstancer  EventInstancer
	listenerInstancer EventListenerInstancer
}

var factory *factoryState

func FactoryInitialise() {
	factory = &factoryState{
		elements:       map[string]ElementInstancer{},
		decorators:     map[string]DecoratorInstancer{},
		filters:        map[string]FilterInstancer{},
		fontEffects:    map[string]FontEffectInstancer{},
		eventInstancer: defaultEventInstancer{},
	}
	FactoryRegisterElementInstancer("*", defaultElementInstancer{})
	FactoryRegisterElementInstancer("#text", textElementInstancer{})
}

func FactoryShutdown() { factory = nil }

// FactoryRegisterElementInstancer is Factory::RegisterElementInstancer.
func FactoryRegisterElementInstancer(name string, instancer ElementInstancer) {
	if factory == nil || instancer == nil {
		return
	}
	factory.elements[name] = instancer
}

// FactoryGetElementInstancer is Factory::GetElementInstancer.
func FactoryGetElementInstancer(tag string) ElementInstancer {
	if factory == nil {
		return nil
	}
	if inst, ok := factory.elements[tag]; ok {
		return inst
	}
	if inst, ok := factory.elements["*"]; ok {
		return inst
	}
	return nil
}

// FactoryInstanceElement is Factory::InstanceElement.
func FactoryInstanceElement(parent *Element, instancerName string, tag string, attributes map[string]Variant) *Element {
	inst := FactoryGetElementInstancer(instancerName)
	if inst == nil {
		inst = FactoryGetElementInstancer(tag)
	}
	if inst == nil {
		return nil
	}
	element := inst.InstanceElement(parent, tag, attributes)
	if element == nil {
		return nil
	}
	element.SetInstancer(inst)
	if parent != nil {
		element.SetOwnerDocument(parent.GetOwnerDocument(), false)
	}
	if attributes != nil {
		for name, value := range attributes {
			element.SetAttribute(name, value)
		}
	}
	if parent != nil {
		parent.AppendChild(element, true)
	}
	return element
}

// FactoryInstanceElementText is Factory::InstanceElementText. A string with
// no markup becomes one text element; a string with tags is parsed as an
// RML fragment parented to parent.
func FactoryInstanceElementText(parent *Element, text string) bool {
	if parent == nil {
		return false
	}
	if text == "" {
		return true
	}
	if indexByte(text, '<') >= 0 {
		return ParseRMLFragment(parent, text)
	}
	textEl := newElementText(text)
	element := textEl.GetElement()
	element.SetInstancer(FactoryGetElementInstancer("#text"))
	if parent != nil {
		element.SetOwnerDocument(parent.GetOwnerDocument(), false)
		parent.AppendChild(element, true)
	}
	return true
}

// FactoryInstanceEvent is Factory::InstanceEvent.
func FactoryInstanceEvent(target *Element, id EventId, eventType string, parameters map[string]Variant, interruptible bool) *Event {
	if factory == nil || factory.eventInstancer == nil {
		return NewEvent(target, id, eventType, parameters, interruptible)
	}
	return factory.eventInstancer.InstanceEvent(target, id, eventType, parameters, interruptible)
}

// FactoryRegisterEventInstancer is Factory::RegisterEventInstancer.
func FactoryRegisterEventInstancer(instancer EventInstancer) {
	if factory == nil {
		return
	}
	factory.eventInstancer = instancer
}

// FactoryRegisterEventListenerInstancer is Factory::RegisterEventListenerInstancer.
func FactoryRegisterEventListenerInstancer(instancer EventListenerInstancer) {
	if factory == nil {
		return
	}
	factory.listenerInstancer = instancer
}

// FactoryInstanceEventListener is Factory::InstanceEventListener.
func FactoryInstanceEventListener(value string, element *Element) EventListener {
	if factory == nil || factory.listenerInstancer == nil {
		return nil
	}
	return factory.listenerInstancer.InstanceEventListener(value, element)
}

// FactoryRegisterDecoratorInstancer is Factory::RegisterDecoratorInstancer.
func FactoryRegisterDecoratorInstancer(name string, instancer DecoratorInstancer) {
	if factory == nil {
		return
	}
	factory.decorators[name] = instancer
}

// FactoryGetDecoratorInstancer is Factory::GetDecoratorInstancer.
func FactoryGetDecoratorInstancer(name string) DecoratorInstancer {
	if factory == nil {
		return nil
	}
	inst, ok := factory.decorators[name]
	if !ok {
		return nil
	}
	return inst
}

// FactoryRegisterFilterInstancer is Factory::RegisterFilterInstancer.
func FactoryRegisterFilterInstancer(name string, instancer FilterInstancer) {
	if factory == nil {
		return
	}
	factory.filters[name] = instancer
}

// FactoryGetFilterInstancer is Factory::GetFilterInstancer.
func FactoryGetFilterInstancer(name string) FilterInstancer {
	if factory == nil {
		return nil
	}
	inst, ok := factory.filters[name]
	if !ok {
		return nil
	}
	return inst
}

// FactoryRegisterFontEffectInstancer is Factory::RegisterFontEffectInstancer.
func FactoryRegisterFontEffectInstancer(name string, instancer FontEffectInstancer) {
	if factory == nil {
		return
	}
	factory.fontEffects[name] = instancer
}

// FactoryGetFontEffectInstancer is Factory::GetFontEffectInstancer.
func FactoryGetFontEffectInstancer(name string) FontEffectInstancer {
	if factory == nil {
		return nil
	}
	inst, ok := factory.fontEffects[name]
	if !ok {
		return nil
	}
	return inst
}

// FactoryInstanceStyleSheetString is Factory::InstanceStyleSheetString.
func FactoryInstanceStyleSheetString(rcss string, context *Context) *StyleSheet {
	container := NewStyleSheetContainer()
	if !container.LoadStyleSheetContainer(NewStreamMemory(rcss), 1) && len(rcss) > 0 {
		return nil
	}
	if context != nil {
		container.UpdateCompiledStyleSheet(context)
	}
	return container.GetCompiledStyleSheet()
}
