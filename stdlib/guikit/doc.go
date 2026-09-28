// Package guikit compiles GML markup and GS scripts onto the rmlui element
// tree. There is one document, one RCSS cascade, and one data model.
//
// A source file is either GML, tag dot class hash id colon attribute dot
// value and parenthesised children, or a document of state, style, view,
// and handle blocks. style blocks and rule tags are RCSS. state, handle,
// func, and script tags are GS. gk-click, gk-submit, and gk-change become
// the listeners that run those handlers. dml.append, dml.remove,
// dml.addClass, dml.removeClass, and dml.setValue edit the live tree.
// Template text of the form {{ name }} reads the data model.
package guikit

// ModelName is the data model Load registers on the context.
const ModelName = "guikit"
