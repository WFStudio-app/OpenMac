# OpenMac usage

After `./install.sh` and `source ~/.omac/env`:

```bash
omac install wget        # installs via Termux pkg (fallback) or local formula
omac list                # packages installed through omac
defaults write com.example.app theme dark
defaults read com.example.app theme
say "Hello from Termux"  # requires termux-api
open https://github.com/WFStudio-app/OpenMac
echo "copied" | pbcopy ; pbpaste
mdfind -onlyin ~/Downloads report
```

## Notes on copied code
Some formulas mirror Homebrew cask structure. They are adapted, not verbatim copies.
Where large chunks were reused, the file header states the original license.
