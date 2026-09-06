#!/bin/bash

mkdir -p "$HOME/.local/share/applications"
mkdir -p "$HOME/.local/share/dbdoverlay"
mkdir -p "$HOME/.local/bin"

cp dbdoverlay.desktop "$HOME/.local/share/applications/"
cp dbdoverlay "$HOME/.local/bin/"
cp -r maps "$HOME/.local/share/dbdoverlay/""

if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
		  echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
fi

echo "dbdoverlay Installed!"
