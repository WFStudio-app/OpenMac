#!/data/data/com.termux/files/usr/bin/bash
# neofetch formula - installs neofetch and applies a macOS-style logo patch hint
case "${1:-install}" in
  install) pkg install -y neofetch; echo "Tip: run 'neofetch --source macos' style config in ~/.config/neofetch";;
  uninstall) pkg uninstall -y neofetch;;
esac
