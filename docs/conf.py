import os
import sys

project = 'Windsurf'
copyright = '2025, Windsurf Team'
author = 'Windsurf Team'
release = '1.0'

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.viewcode',
    'sphinx.ext.napoleon',
    'breathe'
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# HTML output options
html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# Breathe configuration
breathe_projects = {"Windsurf": "."}
breathe_default_project = "Windsurf"
breathe_default_members = ('members', 'undoc-members')
