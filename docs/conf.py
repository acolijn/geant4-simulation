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

# Use standard paths for ReadTheDocs
if on_rtd:
    # Make sure we can find the source files
    master_doc = 'source/index'
    root_doc = 'source/index'

# HTML output options
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    'navigation_depth': 4,
    'titles_only': False,
}

# -- Breathe configuration -------------------------------------------------

# Setup the breathe extension with special handling for ReadTheDocs
if on_rtd:
    # On ReadTheDocs, the XML files are in a different location
    breathe_projects = {
        "Geant4-Simulation": os.path.abspath('/home/docs/checkouts/readthedocs.org/user_builds/geant4-simulation/checkouts/latest/docs/doxygen/xml')
    }
    # Create the directory if it doesn't exist
    os.makedirs(breathe_projects['Geant4-Simulation'], exist_ok=True)
else:
    # For local builds, use the local path
    breathe_projects = {
        "Geant4-Simulation": os.path.abspath(os.path.join(os.getcwd(), 'doxygen/xml'))
    }

breathe_default_project = "Geant4-Simulation"
breathe_default_members = ('members', 'undoc-members')

# Debug output for Breathe
print(f"Breathe project path: {breathe_projects['Geant4-Simulation']}")
print(f"Current working directory: {os.getcwd()}")
print(f"Does XML directory exist? {os.path.exists(breathe_projects['Geant4-Simulation'])}")
if os.path.exists(breathe_projects['Geant4-Simulation']):
    print(f"XML directory contents: {os.listdir(breathe_projects['Geant4-Simulation'])}")
else:
    print("XML directory does not exist, will be created by Doxygen")

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

# Run Doxygen for both local and ReadTheDocs builds
run_doxygen()

def setup(app):
    # This function is called by Sphinx during the build process
    # We don't need to do anything here since we already ran Doxygen above
    return

