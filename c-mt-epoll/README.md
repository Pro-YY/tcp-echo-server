# Multi-threaded Epoll TCP Echo Server

### Build with cmake
```
# Debug (Release)
mkdir -p _build/Debug
cd _build/Debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cd -
make -C _build/Debug
# run
./_build/Debug/server 1337
```

### Implementation Details

1. thread pool: pthread

2. event loop: epoll ET with non-blocking IO
