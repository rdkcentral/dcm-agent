---
name: export-presentation
description: Build a static SPA or export a Slidev presentation to PDF.
license: MIT
compatibility: Requires Slidev project with slides.md and pnpm. Playwright required for PDF export.
metadata:
  author: user
  version: "1.0"
  generatedBy: "copilot"
---

# export-presentation Skill

**Description:**
Export your Slidev presentation as a static website (SPA) or PDF for sharing, deployment, or printing.

## Steps

### Option 1: Build Static SPA
1. Ensure all slides render correctly in dev mode (`pnpm dev`).
2. Run the build command:
   ```bash
   pnpm build
   ```
3. Output will be in the `./dist/` directory.
4. Preview with:
   ```bash
   npx serve dist
   ```
5. Deploy to any static host (Netlify, Vercel, GitHub Pages, Cloudflare Pages, etc).

#### Build Options (slides.md frontmatter)
```yaml
---
download: true           # Add download PDF button
exportFilename: my-talk  # Custom export filename
routerMode: hash         # Use hash router for static hosts
---
```

### Option 2: Export to PDF
1. Ensure all slides render correctly in dev mode.
2. Install Playwright (one-time):
   ```bash
   pnpm add -D playwright-chromium
   npx playwright install chromium
   ```
3. Run the export command:
   ```bash
   pnpm export
   ```
4. Output will be `./slides-export.pdf` by default.

#### PDF Export Options
- Custom filename:
  ```bash
  npx slidev export --output my-presentation.pdf
  ```
- Dark mode:
  ```bash
  npx slidev export --dark
  ```
- With click animations:
  ```bash
  npx slidev export --with-clicks
  ```
- Specific slide range:
  ```bash
  npx slidev export --range 1-10
  ```

## Prerequisite Check
- Before exporting to PDF, check if Playwright is installed:
  ```bash
  pnpm list playwright-chromium
  ```
- If not installed, add it and install Chromium:
  ```bash
  pnpm add -D playwright-chromium
  npx playwright install chromium
  ```
- The default PDF output is `slides-export.pdf`. To use a custom filename, run:
  ```bash
  npx slidev export --output <custom-filename>.pdf
  ```

## Rules
- **Never delete or overwrite source slides.md** during export.
- **Only export or build**; do not modify presentation content unless explicitly requested.
- **Confirm output location** and options before running export/build.

## Example Usage
- *Export the current presentation to PDF with a custom filename.*
- *Build a static SPA for deployment to GitHub Pages.*

## Tips
- Always preview your slides before exporting.
- Use frontmatter options for custom export behavior.
- For PDF export, ensure Playwright is installed and up to date.

## Limitations
- PDF export requires Playwright and Chromium.
- Some advanced animations or transitions may not render identically in PDF.

- Always `cd` to the correct presentation folder (where `package.json` and `slides.md` are located) before running any build or export commands:
  ```bash
  cd presentations/your-presentation-folder
  ```
