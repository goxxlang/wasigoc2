# RmlUi 6.3 parity

`rmlui` follows the RmlUi 6.3 core. GuiKit (`import "guikit"`) compiles GML and GS onto this tree: one element model, one RCSS cascade, one data model.

## Ported

Style types, computed values, properties, RCSS parser, element tree, events, decorators/filters as data, render-manager API, math, transforms, the document/context/factory runtime, block and inline layout, RML load, `{{ name }}` text bindings.

## Partial

- Layout formats block, inline, and inline-block. Flex, grid, and table displays stack as blocks.
- Font metrics match WASMBlinker's no-face fallback (ascent 0.8em, descent 0.2em) until the host installs a face. Glyph pixels are `wasmskia::DrawText` in WASMBlinker (`GetPaintFonts`). WASMRasta encodes the PNG and rasterizes SVG.
- Data bindings are scalar `{{ path }}` text views. Structural `data-for` is not implemented.
- Smooth scroll applies the offset immediately.
- The default render interface is a no-op until the host installs one.

## Host entry points

`Initialise`, `CreateContext`, `Context.LoadDocumentFromMemory`, `Context.Update`, `Context.Render`, `guikit.Load`, `guikit.Invoke`.
