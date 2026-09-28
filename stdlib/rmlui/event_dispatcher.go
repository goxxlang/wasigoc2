// Port of RmlUi Source/Core/EventDispatcher.cpp and EventDispatcher.h.
package rmlui

type eventListenerEntry struct {
	id             EventId
	inCapturePhase bool
	listener       EventListener
}

// EventDispatcher is Rml::EventDispatcher: an element's listeners, kept
// sorted by (id, phase) and then by insertion order.
type EventDispatcher struct {
	element   *Element
	listeners []eventListenerEntry
}

func NewEventDispatcher(element *Element) *EventDispatcher {
	return &EventDispatcher{element: element}
}

func entryLessIdPhase(a eventListenerEntry, b eventListenerEntry) bool {
	if a.id != b.id {
		return a.id < b.id
	}
	return !a.inCapturePhase && b.inCapturePhase
}

func (d *EventDispatcher) AttachEvent(id EventId, listener EventListener, inCapturePhase bool) {
	entry := eventListenerEntry{id, inCapturePhase, listener}
	// Upper bound of the (id, phase) range: insert after existing equals.
	insertAt := len(d.listeners)
	for i := 0; i < len(d.listeners); i++ {
		existing := d.listeners[i]
		if existing.id == id && existing.inCapturePhase == inCapturePhase && existing.listener == listener {
			return
		}
		if entryLessIdPhase(entry, existing) {
			insertAt = i
			break
		}
	}
	d.listeners = append(d.listeners, eventListenerEntry{})
	for i := len(d.listeners) - 1; i > insertAt; i-- {
		d.listeners[i] = d.listeners[i-1]
	}
	d.listeners[insertAt] = entry
	listener.OnAttach(d.element)
}

func (d *EventDispatcher) DetachEvent(id EventId, listener EventListener, inCapturePhase bool) {
	for i := 0; i < len(d.listeners); i++ {
		existing := d.listeners[i]
		if existing.id == id && existing.inCapturePhase == inCapturePhase && existing.listener == listener {
			out := []eventListenerEntry{}
			for j := 0; j < len(d.listeners); j++ {
				if j != i {
					out = append(out, d.listeners[j])
				}
			}
			d.listeners = out
			listener.OnDetach(d.element)
			return
		}
	}
}

func (d *EventDispatcher) DetachAllEvents() {
	listeners := d.listeners
	for _, entry := range listeners {
		entry.listener.OnDetach(d.element)
	}
	d.listeners = nil
	n := d.element.GetNumChildren(true)
	for i := 0; i < n; i++ {
		d.element.GetChild(i).GetEventDispatcher().DetachAllEvents()
	}
}

type collectedListener struct {
	element  *Element
	listener EventListener
	// sort is the DOM distance from the target, negative in capture phase.
	sort int
}

func (c collectedListener) phase() EventPhase {
	if c.sort < 0 {
		return EventPhaseCapture
	} else if c.sort == 0 {
		return EventPhaseTarget
	}
	return EventPhaseBubble
}

func (d *EventDispatcher) collectListeners(distance int, id EventId, phases EventPhase, out []collectedListener) []collectedListener {
	inTarget := distance == 0
	listeners := d.listeners
	for _, entry := range listeners {
		if entry.id != id {
			continue
		}
		if inTarget {
			if phases&EventPhaseTarget != 0 {
				out = append(out, collectedListener{d.element, entry.listener, distance})
			}
		} else {
			listenerPhase := EventPhaseBubble
			sortValue := distance
			if entry.inCapturePhase {
				listenerPhase = EventPhaseCapture
				sortValue = -distance
			}
			if phases&listenerPhase != 0 {
				out = append(out, collectedListener{d.element, entry.listener, sortValue})
			}
		}
	}
	return out
}

// EventDispatcherDispatch is EventDispatcher::DispatchEvent: runs capture,
// target, and bubble listeners, then default actions. Returns false if the
// event was stopped.
func EventDispatcherDispatch(target *Element, id EventId, eventType string, parameters map[string]Variant, interruptible bool, bubbles bool, defaultActionPhase DefaultActionPhase) bool {
	listeners := []collectedListener{}
	defaultActionElements := []*Element{}
	phases := EventPhaseCapture | EventPhaseTarget
	if bubbles {
		phases = phases | EventPhaseBubble
	}
	distance := 0
	walk := target
	for walk != nil {
		listeners = walk.GetEventDispatcher().collectListeners(distance, id, phases, listeners)
		if distance == 0 {
			if defaultActionPhase&EventPhaseTarget != 0 {
				defaultActionElements = append(defaultActionElements, walk)
			}
		} else if defaultActionPhase&EventPhaseBubble != 0 {
			defaultActionElements = append(defaultActionElements, walk)
		}
		walk = walk.GetParentNode()
		distance++
	}
	if len(listeners) == 0 && len(defaultActionElements) == 0 {
		return true
	}
	// Stable sort by sort value keeps each element's listener order.
	for i := 1; i < len(listeners); i++ {
		j := i
		for j > 0 && listeners[j].sort < listeners[j-1].sort {
			listeners[j], listeners[j-1] = listeners[j-1], listeners[j]
			j--
		}
	}
	event := FactoryInstanceEvent(target, id, eventType, parameters, interruptible)
	if event == nil {
		return false
	}
	previousSort := 2147483647
	for _, desc := range listeners {
		if desc.sort != previousSort {
			if !event.IsPropagating() {
				break
			}
			event.SetCurrentElement(desc.element)
			event.SetPhase(desc.phase())
			previousSort = desc.sort
		}
		if desc.element != nil && desc.listener != nil {
			desc.listener.ProcessEvent(event)
		}
		if !event.IsImmediatePropagating() {
			break
		}
	}
	for _, element := range defaultActionElements {
		if !event.IsPropagating() {
			break
		}
		event.SetCurrentElement(element)
		if element == target {
			event.SetPhase(EventPhaseTarget)
		} else {
			event.SetPhase(EventPhaseBubble)
		}
		element.ProcessDefaultAction(event)
	}
	return event.IsPropagating()
}

// ToString summarizes listeners as "type (count), ...".
func (d *EventDispatcher) ToString() string {
	result := ""
	if len(d.listeners) == 0 {
		return result
	}
	previous := d.listeners[0].id
	count := 0
	listeners := d.listeners
	for _, entry := range listeners {
		if entry.id != previous {
			result = result + EventSpecificationGet(previous).Type + " (" + FormatInt(count) + "), "
			previous = entry.id
			count = 0
		}
		count++
	}
	if count > 0 {
		result = result + EventSpecificationGet(previous).Type + " (" + FormatInt(count) + "), "
	}
	if len(result) > 2 {
		result = result[:len(result)-2]
	}
	return result
}
