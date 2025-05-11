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

# -- Run doxygen automatically during ReadTheDocs build ------------------

on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

def setup(app):
    # Run doxygen if we're on ReadTheDocs or during local builds
    doxygen_dir = Path('./doxygen')
    xml_dir = doxygen_dir / 'xml'
    xml_dir.mkdir(parents=True, exist_ok=True)
    
    # Change to the doxygen directory and run doxygen
    cwd = os.getcwd()
    os.chdir(str(doxygen_dir))
    subprocess.call('doxygen', shell=True)
    os.chdir(cwd)

