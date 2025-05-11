import os
import sys
import subprocess
from pathlib import Path

# Determine if we're on ReadTheDocs
on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

# Handle paths differently depending on where we're running
if on_rtd:
    # On ReadTheDocs, we need to adjust paths
    print("Running on ReadTheDocs - adjusting paths")
    # Get the current directory
    current_dir = os.getcwd()
    print(f"Current directory: {current_dir}")
    
    # Add the current directory to the path
    sys.path.insert(0, os.path.abspath('.'))
    
    # If we're in the project root, add the docs directory
    if os.path.exists('docs'):
        sys.path.insert(0, os.path.abspath('docs'))
        # Also add the source directory
        if os.path.exists(os.path.join('docs', 'source')):
            sys.path.insert(0, os.path.abspath(os.path.join('docs', 'source')))
    
    # Print the path for debugging
    print(f"Python path: {sys.path}")
else:
    # For local builds, just add the current and source directories
    sys.path.insert(0, os.path.abspath('.'))
    sys.path.insert(0, os.path.abspath('source'))

# Project information
project = 'Geant4-Simulation'
copyright = '2025, Windsurf Team'
author = 'Windsurf Team'
release = '1.0'

# Add any Sphinx extension module names here
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.viewcode',
    'sphinx.ext.napoleon',
    'breathe',
    'myst_parser'  # For parsing Markdown files
]

# Source directory for documentation
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}
source_encoding = 'utf-8-sig'

# The master toctree document
master_doc = 'index'
root_doc = 'index'

# Path setup for templates and static files
templates_path = ['source/_templates']
html_static_path = ['source/_static']

# Files to exclude from building
exclude_patterns = ['build', 'Thumbs.db', '.DS_Store', 'doxygen', '**.xml']

# If we're on ReadTheDocs, we need to adjust some paths
if on_rtd:
    # Make sure we can find the source files
    html_extra_path = ['source']
    # Include both the docs directory and the source directory in the search path
    html_additional_pages = {'index': 'index.html'}
    # Make sure we can find the index file
    master_doc = 'index'
    root_doc = 'index'

# HTML output options
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    'navigation_depth': 4,
    'titles_only': False,
}

# -- Breathe configuration -------------------------------------------------

# Setup the breathe extension
breathe_projects = {
    "Geant4-Simulation": "doxygen/xml"
}
breathe_default_project = "Geant4-Simulation"
breathe_default_members = ('members', 'undoc-members')

# Tell sphinx what the primary language being documented is.
primary_domain = 'cpp'

# Tell sphinx what the pygments highlight language should be.
highlight_language = 'cpp'

# -- ReadTheDocs configuration ------------------

on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

# Make sure the XML directory exists
os.makedirs('doxygen/xml', exist_ok=True)

# Always run Doxygen during the Sphinx build process
def run_doxygen():
    try:
        print("Running Doxygen...")
        doxyfile_path = os.path.abspath('Doxyfile')
        if os.path.exists(doxyfile_path):
            print(f"Found Doxyfile at {doxyfile_path}")
            subprocess.call(['doxygen', doxyfile_path], shell=True)
            print("Doxygen completed successfully using Doxyfile")
        else:
            print(f"Doxyfile not found at {doxyfile_path}, trying default command")
            subprocess.call(['doxygen'], shell=True)
            print("Doxygen completed successfully using default command")
    except Exception as e:
        print(f"Error running Doxygen: {e}")

# Run Doxygen before Sphinx processes the documentation
if not on_rtd:
    # Only run Doxygen locally - ReadTheDocs will handle it automatically
    run_doxygen()

def setup(app):
    # This function is called by Sphinx during the build process
    # We don't need to do anything here since we already ran Doxygen above
    return

