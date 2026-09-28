// Package rmlui is a Go++ port of the RmlUi 6.3 core library
// (namespace Rml): RML documents, RCSS style sheets, the layout engine,
// the render manager, events, and data bindings.
//
// The port is file-for-file. Each Go file names the RmlUi source it
// mirrors in its header comment, and PARITY.md in this directory is the
// ledger of which RmlUi Core files and classes are ported, partial, or not
// yet started. Behavior follows the RmlUi source, not CSS or the browser:
// where RmlUi deviates from CSS, so does this package.
//
// Mapping from the C++ source:
//
//   - Rml::X becomes rmlui.X. Enum classes become int constants named
//     <Enum><Value> (Unit::PX is UnitPX, PropertyId::Width is
//     PropertyIdWidth, Style::Display::Block is DisplayBlock).
//   - Class inheritance becomes composition. Element is one struct; the
//     subclass behavior RmlUi gets from virtual methods (ElementText,
//     ElementDocument, form controls, ElementImage, ...) lives behind the
//     ElementImpl interface stored in Element.impl.
//   - Callback types RmlUi takes as std::function become interfaces
//     (EventListener, DataEventFunc, ...), which cross package boundaries
//     cleanly in this compiler.
//   - Singletons (Factory, StyleSheetSpecification, the font engine, the
//     system and render interfaces) are package-level state set up by
//     Initialise, as in RmlUi.
//
// Float arithmetic is float32 throughout, matching RmlUi's float, so layout
// rounding matches the C++ library.
//
// GuiKit (import "guikit") is a front end over this package: GML syntax
// and GS scripts compile to the same element tree, listeners, and data
// models, so there is one document model and one renderer.
package rmlui

// RmlUiVersion is the RmlUi release this package ports (Rml::GetVersion).
const RmlUiVersion = "6.3"
