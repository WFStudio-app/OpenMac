#!/data/data/com.termux/files/usr/bin/bash
# Formula: iterm2 -> mapped to Termux styling + termux-services (adapted from Homebrew cask idea)
# License note: structure inspired by Homebrew (BSD-2). Implementation rewritten for Termux.
case "${1:-info}" in
  install) pkg install -y termux-services tmux; echo "Installed tmux+termux-services as iterm2 analog.";;
  uninstall) pkg uninstall -y termux-services tmux;;
  info) echo "iterm2 (OpenMac formula): terminal multiplexing via tmux on Termux."; exit 0;;
esac
