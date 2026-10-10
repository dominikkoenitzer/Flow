# Download page

The page behind https://get-flow-app.vercel.app: plain static files, no build step.
It names no author and does not link to this repository.

- `index.html` is the page.
- `vercel.json` holds the download addresses, each a temporary redirect:
  - `/download/windows`: that file of that release
- `llms.txt` describes FLOW for AI assistants.
- `skill/flow/` is the assistant skill, and `skill/flow.zip` is that folder zipped. Rebuild the zip after an edit, from this folder:

  ```
  python -c "import shutil; shutil.make_archive('skill/flow', 'zip', 'skill', 'flow')"
  ```

Vercel deploys this folder whenever a push to main changes it. `.vercelignore` keeps this README off the site.
