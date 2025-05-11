import os
import sys
import subprocess
from pathlib import Path

# Add the source directory to the path so that Sphinx can find the .rst files
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
source_parsers = {
    '.md': 'myst_parser',
}

# The master toctree document
master_doc = 'index'

# Path setup for templates and static files
templates_path = ['source/_templates']
html_static_path = ['source/_static']

# Files to exclude from building
exclude_patterns = ['build', 'Thumbs.db', '.DS_Store', 'doxygen', '**.xml']

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
doxygen_xml_dir = os.path.abspath('doxygen/xml')
os.makedirs(doxygen_xml_dir, exist_ok=True)

# Update breathe configuration with the correct path
breathe_projects = {
    "Geant4-Simulation": doxygen_xml_dir
}

def setup(app):
    # Run doxygen if we're on ReadTheDocs
    if on_rtd:
        # Ensure the doxygen directory exists
        os.makedirs('doxygen', exist_ok=True)
        
        # Run doxygen from the docs directory
        current_dir = os.getcwd()
        print(f"Current directory: {current_dir}")
        
        try:
            print("Running Doxygen...")
            subprocess.call(['doxygen', 'Doxyfile'], cwd=current_dir)
            print("Doxygen completed successfully")
        except Exception as e:
            print(f"Error running Doxygen: {e}")
    else:
        print("Not on ReadTheDocs, skipping Doxygen generation")
        
    # Check if the XML directory exists and has files
    if os.path.exists(doxygen_xml_dir):
        xml_files = os.listdir(doxygen_xml_dir)
        print(f"XML directory contains {len(xml_files)} files")
    else:
        print(f"XML directory does not exist: {doxygen_xml_dir}")
        
    return

