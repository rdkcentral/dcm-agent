---
name: add-slide
description: Add new slides with various layouts to a Slidev presentation.
license: MIT
compatibility: Requires Slidev project with slides.md.
metadata:
  author: user
  version: "1.0"
  generatedBy: "copilot"
---

# add-slide Skill

**Description:**
Add new slides to an existing Slidev presentation, supporting multiple layouts and markdown content.

## Steps
1. Open or locate the `slides.md` file in your Slidev project.
2. Choose a layout for your new slide (see Layout Reference below).
3. Insert the slide markdown at the desired position in `slides.md`.
4. Save the file and preview changes with `pnpm dev` or your Slidev dev server.

## Layout Reference

### Default Slide
```markdown
---

# Slide Title

Regular content with markdown support.

- Bullet points
- **Bold text**
- `inline code`
```

### Centered Content
```markdown
---
layout: center
---

# Centered Title

This content is vertically and horizontally centered.
```

### Two Columns
```markdown
---
layout: two-cols
---

::left::
- Left column content

::right::
- Right column content
```

### Directory Listing Slide
To showcase the contents of a directory (such as generated specs), use a code block with the `sh` language and format the output as if from the `tree` command. This is ideal for technical audiences and PDF exports.

#### Example
```sh
$ tree specs/ changes/
specs/
├── cpp_specifics_spec.md
├── header_interfaces_spec.md
├── json_rpc_handling_spec.md
├── transport_layer_spec.md
├── transport_recommendations_spec.md
changes/
├── archive/
└── waffle-wpebrowser-extension/
```

## Customization Options

- **Presentation Selection:** Specify the target Slidev presentation by providing the path to its `slides.md` file (e.g., `presentations/my-slides/slides.md`).
- **Insertion Point:** Choose where to insert the new slide:
  - At the beginning (before the first slide)
  - At the end (after the last slide)
  - After a specific slide (by slide number or unique heading)
- **Batch Insert:** Multiple slides can be added in sequence by repeating the insertion process or providing an array of slide contents/positions.

## Rules
- **Never replace existing slides**: This skill must only insert new slides. Do not overwrite or remove any existing slide content unless the user explicitly requests a replacement.
- **Insertion only**: All slide additions must preserve the current content and order of existing slides, except for the specified insertion point.
- **Explicit replacement**: If the user requests to replace a slide, confirm the target slide and proceed only with clear user intent.

## Example Usage

- *Insert a centered slide after the second slide in `presentations/openspec-with-copliot-experience/slides.md`.*
- *Add a two-column slide at the end of `presentations/transport-slides/slides.md`.*

## Tips
- Query `/slidevjs/slidev` for more layouts and options.
- Use markdown features for formatting and structure.
- Save and preview to verify your changes.

## Limitations
- The `two-cols` layout in Slidev does not support rendering code blocks (including mermaid diagrams) or HTML tags inside the `::left::` and `::right::` slots. Only plain markdown and text are reliably rendered in columns.
- To display diagrams or code, place them above or below the two-column layout, or use a single-column slide.
- For advanced layouts, consider custom components or exporting diagrams as images.
