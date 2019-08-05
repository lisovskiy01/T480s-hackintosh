#!/bin/bash

cd "$(dirname "$0")"

TOP="$PWD"

confgen="$TOP/bin/confgen"
target="$TOP/../EFI/CLOVER/config.plist"
example="$TOP/../EFI/CLOVER/config.example.plist"
force=false

if [[ "$1" == "-f" ]] || [[ "$1" == "--force" ]]; then
    force=true
fi

if [[ "$(uname)" == "Linux" ]]; then
    confgen="$confgen-linux"
fi

if [ -f "$target" ] && [[ "$force" == "false" ]]; then
    "$confgen" -t "$example" -i "$target" -o "$target"
else
    $force && echo "Force overwriting config.plist."
    "$confgen" -t "$example" -o "$target"
fi
