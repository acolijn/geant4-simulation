#!/bin/bash
# Script to build documentation

# Set variables
DOCS_DIR=$(pwd)
BUILD_DIR="${DOCS_DIR}/build"
DOXYGEN_DIR="${DOCS_DIR}/doxygen"
SOURCE_DIR="${DOCS_DIR}/source"

# Function to display help
show_help() {
    echo "Documentation Build Script"
    echo "Usage: ./build_docs.sh [option]"
    echo "Options:"
    echo "  clean     - Clean all generated documentation files"
    echo "  doxygen   - Generate Doxygen XML files only"
    echo "  html      - Build HTML documentation (default)"
    echo "  help      - Show this help message"
}

# Function to clean documentation
clean_docs() {
    echo "Cleaning documentation files..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${DOXYGEN_DIR}/xml"
    rm -rf "${DOXYGEN_DIR}/html"
    echo "Documentation cleaned."
}

# Function to build Doxygen XML
build_doxygen() {
    echo "Generating Doxygen XML files..."
    cd "${DOXYGEN_DIR}" || exit 1
    doxygen
    cd "${DOCS_DIR}" || exit 1
    echo "Doxygen XML generation complete."
}

# Function to build HTML documentation
build_html() {
    echo "Building HTML documentation..."
    
    # First build Doxygen XML
    build_doxygen
    
    # Then build Sphinx HTML
    mkdir -p "${BUILD_DIR}/html"
    sphinx-build -b html -c "${DOCS_DIR}" "${SOURCE_DIR}" "${BUILD_DIR}/html"
    
    echo "HTML documentation build complete."
    echo "You can view the documentation by opening: ${BUILD_DIR}/html/index.html"
}

# Process command line arguments
case "$1" in
    clean)
        clean_docs
        ;;
    doxygen)
        build_doxygen
        ;;
    html|"")
        build_html
        ;;
    help)
        show_help
        ;;
    *)
        echo "Unknown option: $1"
        show_help
        exit 1
        ;;
esac

exit 0
