#!/bin/bash
# Script to clean up and reorganize the docs directory

# Create necessary directories if they don't exist
mkdir -p source/_static source/_templates doxygen/xml _build

# Move documentation source files to source directory
cp *.rst source/
cp *.md source/

# Make sure the Doxyfile is in the doxygen directory
if [ ! -f doxygen/Doxyfile ]; then
  cp Doxyfile doxygen/
fi

# Remove XML files from the root directory
rm -f *.xml

# Create a .gitignore file to ignore generated files
cat > .gitignore << EOF
# Generated documentation files
_build/
doxygen/xml/
doxygen/html/
html/
xml/

# Temporary files
*.swp
*.swo
*.tmp
*~
EOF

echo "Documentation directory cleanup complete!"
echo "New structure:"
echo "  - source/: Documentation source files (.rst, .md)"
echo "  - doxygen/: Doxygen configuration and output"
echo "  - _build/: Sphinx build output"
echo "  - conf.py: Sphinx configuration"
echo "  - requirements.txt: Python dependencies"
echo ""
echo "To build documentation:"
echo "  1. cd to docs directory"
echo "  2. Run 'make html' to build HTML documentation"
echo "  3. Open _build/html/index.html in your browser"
