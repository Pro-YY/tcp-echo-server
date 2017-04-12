# Multi-threaded Epoll TCP Echo Server

###Build with cmake
```
# Debug (Release)
mkdir -p _build/Debug
cd _build/Debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cd -
make -C _build/Debug
# run
./_build/Debug/server
```

### Implementation Details

1. multi-threaded: using pthread

2. epoll event loop: epoll ET with non-blocking IO
