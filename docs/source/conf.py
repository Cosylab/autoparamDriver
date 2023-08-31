# SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
#
# SPDX-License-Identifier: MIT-0

# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))

import subprocess

# -- Project information -----------------------------------------------------

project = 'autoparamDriver'
project_copyright = '2022, Cosylab d.d.'
author = 'Cosylab d.d.'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'breathe',
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

html_static_path = ['_static']

html_title = project + " Documentation"
html_logo = "_static/Cosylab_Logo.png"
html_favicon = "_static/Cosylab_Favicon.png"

extensions += ['sphinx_rtd_theme']
html_theme = 'sphinx_rtd_theme'
html_css_files = [ 'custom.css' ]
html_theme_options = {
    'collapse_navigation': False,
    'display_version': False,
    'style_nav_header_background': '#371D4F',
    #'analytics_id'
}

# -- Breathe setup -----------------------------------------------------------

breathe_projects = { 'autoparamDriver': '../doxygen/xml' }
breathe_default_project =  'autoparamDriver'
breathe_default_members = ( 'members', 'protected-members' )

subprocess.check_call('cd ../..; doxygen docs/Doxyfile', shell=True)

# -- C++ setup ---------------------------------------------------------------

cpp_index_common_prefix = [ 'Autoparam::' ]
