#!/bin/bash
# Script to clean up documentation build files from Git repository
# while keeping the files on disk

# Set -e to exit on error
set -e

echo "Cleaning up documentation build files from Git repository..."

# Remove HTML documentation files from Git tracking
echo "Removing HTML documentation files..."
git rm --cached -r docs/html/ docs/docs/html/ 2>/dev/null || true

# Remove XML documentation files from Git tracking
echo "Removing XML documentation files..."
git rm --cached docs/*.xml docs/docs/xml/*.xml 2>/dev/null || true

# Remove any other build directories
echo "Removing other build directories..."
git rm --cached -r docs/_build/ docs/**/_build/ 2>/dev/null || true

# Update .gitignore to ensure XML files are also ignored
echo "Updating .gitignore file..."
if ! grep -q "# Documentation XML files" .gitignore; then
  cat >> .gitignore << EOF

# Documentation XML files
docs/*.xml
docs/**/*.xml
EOF
fi

echo "Changes prepared. To complete the cleanup:"
echo "1. Review the changes with 'git status'"
echo "2. Commit the changes with: git commit -m \"Remove documentation build files from repository\""
echo "3. Push the changes with: git push"
echo ""
echo "Done!"
