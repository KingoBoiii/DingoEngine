#!/bin/sh
cd "$(dirname "$0")" && exec vendor/premake/bin/premake5 gmake "$@"
