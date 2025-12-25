to get this project running you need to

1. clone the libdatachannel library into the external/ folder

```
# Taken from the github

cd external
git clone https://github.com/paullouisageneau/libdatachannel.git
cd libdatachannel
git submodule update --init --recursive --depth 1
```

2. Generate build

`cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=on`


3. Build

`cmake --build build`


4. Run

`./build/src/sender`


...
