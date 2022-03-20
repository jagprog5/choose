# choose

Splits an input into tokens based on a regex delimiter, and provides a text based ui for selecting which token are sent to the output.

![screenshot](./screenshot.png)

Install with:

```bash
sudo apt-get install -y libncurses5-dev
mkdir build && cd build && cmake .. && sudo cmake --build . --target install 
```

See doc:

```bash
choose --help
```
