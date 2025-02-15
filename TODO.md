# Fallback Context

 * Foundry is very project oriented. However we could have a "fallback"
   context that is used when there is no `.foundry` available. This could
   make it easier to edit things in `~/Downloads/`, `/etc`, and similar.

   One aspect of that would be to disable recursive file-monitors as well
   as services that do code execution.

# CLI Commands

 * foundry debug
 * foundry profile
 * foundry valgrind
 * foundry pipeline target (list|switch)
 * foundry buildconfig ...

# Build Systems

 * Maven
 * Gradle
 * buildstream
 * cargo
 * npm
 * golang
 * dub

# Configurations

 * Buildconfig

# Managers

 * File Manager
   * Try to avoid recursive monitors past X deep. Mark the project
     as degraded if we go past and require install before every
     run (as opposed to skipping if not).
   * Manage project tree data abstractions.
 * Operations Manager
 * Persistent Logging 
 * Language Server Manager
 * Debugger Manager
 * Code Action Manager
 * Search Manager
 * VCS Manager
 * Code Indexing

# Devices

 * Qemu-user-static
 * Simulator SDK

# File Formatters

 * clang-format
 * swift-format

# File/Language Settings

 * modelines
 * editorconfig
 * gsettings (and allow tweaking with foundry settings)

# Completion Providers

 * HTML / XML Completion

# Diagnostics Providers

 * eslint
 * flake8
 * gdiagnose
 * gettext
 * hadolint
 * rstcheck
 * rubocop
 * shellcheck
 * stylelint
 * swiftlint
 * xmllint

# Symbol Resolvers

 * Ctags
 * Clang

# Language Servers

 * bash
 * blueprint
 * clangd
 * elixir
 * glsl
 * gopls
 * intelephense
 * jdtls
 * jedi
 * lua
 * mesonlsp
 * python-lsp-server
 * rust-analyzer
 * serve-d
 * sourcekit
 * ts/js
 * vala
 * vhdl
 * zls

# DAP Servers

 * GDB
 * LLDB

# Editing

 * GTK-less text iter engine so we can run foundry without a display
   and still have useful API for plug-ins.

# Build Pipeline

 * Error extraction regex (using intermediate PTY)
   * GCC
   * Mono

# Project Templates

 * Make templates
 * Meson templates

# VCS

 * Git

