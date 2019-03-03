set -e

# Copied from https://stackoverflow.com/a/14203146/2775471
POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    --extra-cxx-flags)
    EXTRA_CXXFLAGS="$2"
    shift # past argument
    shift # past value
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

CXX=clang++
CXXFLAGS="-std=c++14 -fno-rtti $EXTRA_CXXFLAGS"

echo "CXX: $CXX"
echo "CXXFLAGS: $CXXFLAGS"

$CXX $CXXFLAGS lang.cpp Lexer.cpp Parser.cpp Interpret.cpp
