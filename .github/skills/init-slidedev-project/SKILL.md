---
name: init-slidedev-project
description: Initialize a new Slidev project with slides.md and configuration.
license: MIT
compatibility: Requires Slidev, pnpm, and project template.
metadata:
  author: user
  version: "1.0"
  generatedBy: "copilot"
---

# init-slidedev-project Skill

**Description:**
Initialize a new Slidev presentation project from scratch, including starter slides and configuration.

## Steps
1. Query `context7` for the latest Slidev configuration options using `/slidevjs/slidev`.
2. Copy `_project_template_` to a new project directory (e.g., `my-new-presentation`).
   ```bash
   cp -r _project_template_ my-new-presentation
   ```
3. Create or update `slides.md` in the new directory with starter content (see below for example).
4. Run `pnpm install` in the new project directory to install dependencies.
5. Run `pnpm dev` to start the development server and preview the presentation.
6. (Optional) To add a custom theme, search for available themes, add to `package.json`, and update the `theme` field in `slides.md` frontmatter.

## Example slides.md Starter Content
```markdown
---
theme: default
title: My Presentation
info: |
  A presentation created with Slidev
highlighter: shiki
transition: slide-left
mdc: true
---

# Welcome to Slidev

Presentation slides for developers

---

# What is Slidev?

Slidev is a slides maker and presenter designed for developers.

- 📝 **Markdown-based** - focus on content
- 🎨 **Themable** - themes can be shared and reused
- 🧑‍💻 **Developer Friendly** - code highlighting, live coding
- 🤹 **Interactive** - embed Vue components
- 🎥 **Recording** - built-in recording and camera view

---
layout: center
---

# Thank You!

[Documentation](https://sli.dev) · [GitHub](https://github.com/slidevjs/slidev)
```

## Rules
- Always use the latest Slidev syntax and configuration.
- Use `pnpm` for all dependency management and commands.
- Never run commands without user confirmation.
- Always work from within the specific project directory.
- Validate that `slides.md` and `package.json` exist after setup.
- Minimal edits; only scaffold the requested project.
