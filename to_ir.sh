if [ "$#" -ne 1 ]; then
  echo "Usage: $0 kal_source"
  exit
fi

LLVM_VERSION=14

./kalcc $1 | opt-$LLVM_VERSION -mem2reg | llvm-dis-$LLVM_VERSION